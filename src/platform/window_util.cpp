#include "pch.hpp"

#include "platform/window_util.hpp"

#include <aclapi.h>
#include <array>
#include <dwmapi.h>
#include <iostream>
#include <sddl.h>
#include <string_view>

namespace genie::platform {
namespace {

bool IsExcludedClassName(std::wstring_view class_name) {
  constexpr std::array<std::wstring_view, 8> kExcludedClassNames = {
      L"Progman",
      L"WorkerW",
      L"Shell_TrayWnd",
      L"Shell_SecondaryTrayWnd",
      L"DV2ControlHost",
      L"Windows.UI.Core.CoreWindow",
      L"XamlExplorerHostIslandWindow",
      L"ApplicationFrameInputSinkWindow",
  };
  for (std::wstring_view excluded : kExcludedClassNames) {
    if (class_name == excluded) {
      return true;
    }
  }
  return false;
}

BOOL CALLBACK CollectWindowsProc(HWND window, LPARAM parameter) {
  auto* context = reinterpret_cast<std::pair<HWND, std::vector<HWND>*>*>(parameter);
  if (IsInterestingTopLevelWindow(window, context->first)) {
    context->second->push_back(window);
  }
  return TRUE;
}

}  // namespace

std::optional<RECT> GetExtendedFrameBounds(HWND window) {
  if (!IsWindow(window)) {
    return std::nullopt;
  }

  RECT bounds{};
  bool use_placement = IsIconic(window) != FALSE;

  if (!use_placement) {
    HRESULT hr =
        DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds));
    if (FAILED(hr) || bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
      if (!GetWindowRect(window, &bounds)) {
        return std::nullopt;
      }
    }
    if (bounds.left < -30000 && bounds.top < -30000) {
      use_placement = true;
    }
  }

  if (use_placement) {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(window, &placement)) {
      RECT normal_rect = placement.rcNormalPosition;
      HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
      if (monitor != nullptr) {
        MONITORINFO monitor_info{};
        monitor_info.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfoW(monitor, &monitor_info)) {
          const LONG offset_x = monitor_info.rcWork.left - monitor_info.rcMonitor.left;
          const LONG offset_y = monitor_info.rcWork.top - monitor_info.rcMonitor.top;
          bounds = RECT{
              .left = normal_rect.left + offset_x,
              .top = normal_rect.top + offset_y,
              .right = normal_rect.right + offset_x,
              .bottom = normal_rect.bottom + offset_y,
          };
        } else {
          bounds = normal_rect;
        }
      } else {
        bounds = normal_rect;
      }
    } else {
      return std::nullopt;
    }
  }

  if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
    return std::nullopt;
  }
  return bounds;
}

bool IsInterestingTopLevelWindow(HWND window, HWND ignored_window) {
  if (window == nullptr || window == ignored_window || !IsWindow(window) ||
      !(IsWindowVisible(window) || IsIconic(window))) {
    return false;
  }
  if (GetAncestor(window, GA_ROOT) != window) {
    return false;
  }
  if (GetWindow(window, GW_OWNER) != nullptr) {
    return false;
  }

  LONG_PTR ex_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
  if ((ex_style & WS_EX_TOOLWINDOW) != 0) {
    return false;
  }

  BOOL cloaked = FALSE;
  if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
      cloaked) {
    return false;
  }

  wchar_t class_name[256]{};
  if (GetClassNameW(window, class_name, static_cast<int>(std::size(class_name))) > 0) {
    if (IsExcludedClassName(class_name)) {
      return false;
    }
  }

  const auto bounds = GetExtendedFrameBounds(window);
  if (!bounds.has_value()) {
    return false;
  }
  const LONG width = bounds->right - bounds->left;
  const LONG height = bounds->bottom - bounds->top;
  return width >= 48 && height >= 48;
}

std::vector<HWND> EnumerateTopLevelWindows(HWND ignored_window) {
  std::vector<HWND> windows;
  auto context = std::pair<HWND, std::vector<HWND>*>(ignored_window, &windows);
  EnumWindows(&CollectWindowsProc, reinterpret_cast<LPARAM>(&context));
  return windows;
}

void SetDwmTransitionsDisabled(HWND window, bool disabled) {
  if (!IsWindow(window)) {
    return;
  }
  const BOOL value = disabled ? TRUE : FALSE;
  DwmSetWindowAttribute(window, DWMWA_TRANSITIONS_FORCEDISABLED, &value, sizeof(value));
}

void SetWindowCloaked(HWND window, bool cloaked) {
  if (!IsWindow(window)) {
    return;
  }
  const BOOL value = cloaked ? TRUE : FALSE;
  DwmSetWindowAttribute(window, DWMWA_CLOAKED, &value, sizeof(value));
}

RECT GetVirtualScreenRect() {
  const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  return RECT{
      .left = left,
      .top = top,
      .right = left + width,
      .bottom = top + height,
  };
}

HWND FindTaskbarWindowForRect(const RECT& rect) {
  HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
  if (taskbar != nullptr) {
    HMONITOR taskbar_monitor = MonitorFromWindow(taskbar, MONITOR_DEFAULTTONEAREST);
    HMONITOR rect_monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    if (taskbar_monitor == rect_monitor) {
      return taskbar;
    }
  }

  HWND secondary_taskbar = nullptr;
  while ((secondary_taskbar = FindWindowExW(nullptr, secondary_taskbar, L"Shell_SecondaryTrayWnd",
                                            nullptr)) != nullptr) {
    HMONITOR taskbar_monitor = MonitorFromWindow(secondary_taskbar, MONITOR_DEFAULTTONEAREST);
    HMONITOR rect_monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    if (taskbar_monitor == rect_monitor) {
      return secondary_taskbar;
    }
  }

  return taskbar;
}

bool GrantAppContainerPermissions(const std::wstring& path) {
  std::wstring normalized_path = path;
  if (normalized_path.size() > 1 &&
      (normalized_path.back() == L'\\' || normalized_path.back() == L'/')) {
    normalized_path.pop_back();
  }

  PACL old_dacl = nullptr;
  PSECURITY_DESCRIPTOR sd = nullptr;

  DWORD err =
      GetNamedSecurityInfoW(normalized_path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                            nullptr, nullptr, &old_dacl, nullptr, &sd);
  if (err != ERROR_SUCCESS) {
    std::wcerr << L"GetNamedSecurityInfoW failed for " << normalized_path << L" error=" << err
               << L"\n";
    return false;
  }

  bool success = false;
  PSID package_sid = nullptr;
  PSID restricted_package_sid = nullptr;

  if (ConvertStringSidToSidW(L"S-1-15-2-1", &package_sid) &&
      ConvertStringSidToSidW(L"S-1-15-2-2", &restricted_package_sid)) {
    EXPLICIT_ACCESSW ea[2]{};

    ea[0].grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
    ea[0].grfAccessMode = GRANT_ACCESS;
    ea[0].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[0].Trustee.ptstrName = reinterpret_cast<LPWSTR>(package_sid);

    ea[1].grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
    ea[1].grfAccessMode = GRANT_ACCESS;
    ea[1].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[1].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[1].Trustee.ptstrName = reinterpret_cast<LPWSTR>(restricted_package_sid);

    PACL new_dacl = nullptr;
    err = SetEntriesInAclW(2, ea, old_dacl, &new_dacl);
    if (err == ERROR_SUCCESS) {
      err = SetNamedSecurityInfoW(const_cast<LPWSTR>(normalized_path.c_str()), SE_FILE_OBJECT,
                                  DACL_SECURITY_INFORMATION, nullptr, nullptr, new_dacl, nullptr);
      if (err == ERROR_SUCCESS) {
        success = true;
      } else {
        std::wcerr << L"SetNamedSecurityInfoW failed for " << normalized_path << L" error=" << err
                   << L"\n";
      }
      LocalFree(new_dacl);
    } else {
      std::wcerr << L"SetEntriesInAclW failed for " << normalized_path << L" error=" << err
                 << L"\n";
    }
  }

  if (package_sid != nullptr) {
    LocalFree(package_sid);
  }
  if (restricted_package_sid != nullptr) {
    LocalFree(restricted_package_sid);
  }
  LocalFree(sd);
  return success;
}

}  // namespace genie::platform
