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

std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) return {};
  const int length =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<size_t>(length), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(), length, nullptr,
                          nullptr) != length) {
    return {};
  }
  return result;
}

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

std::optional<std::string> GetWindowExecutableName(HWND window) {
  if (window == nullptr || !IsWindow(window)) return std::nullopt;
  DWORD process_id = 0;
  GetWindowThreadProcessId(window, &process_id);
  if (process_id == 0) return std::nullopt;
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (process == nullptr) return std::nullopt;

  std::vector<wchar_t> path(32768);
  DWORD length = static_cast<DWORD>(path.size());
  const BOOL queried = QueryFullProcessImageNameW(process, 0, path.data(), &length);
  CloseHandle(process);
  if (!queried || length == 0) return std::nullopt;
  std::wstring_view full_path(path.data(), length);
  const size_t separator = full_path.find_last_of(L"\\/");
  const std::wstring_view filename =
      separator == std::wstring_view::npos ? full_path : full_path.substr(separator + 1);
  std::string utf8 = WideToUtf8(filename);
  if (utf8.empty()) return std::nullopt;
  return utf8;
}

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

bool SetOwnedWindowRegion(HWND window, HRGN region, bool redraw) {
  if (!IsWindow(window)) {
    if (region != nullptr) {
      DeleteObject(region);
    }
    return false;
  }

  if (SetWindowRgn(window, region, redraw ? TRUE : FALSE) != 0) {
    return true;
  }

  if (region != nullptr) {
    DeleteObject(region);
  }
  return false;
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

std::optional<double> GetMonitorRefreshRateHz(HMONITOR monitor) {
  if (monitor == nullptr) {
    return std::nullopt;
  }

  MONITORINFOEXW monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfoW(monitor, &monitor_info)) {
    return std::nullopt;
  }

  // QueryDisplayConfig exposes the active path as a rational refresh rate and
  // therefore preserves rates such as 59.94 Hz instead of rounding them to an
  // integer. Match the path through the monitor's GDI device name so mixed-rate
  // multi-monitor setups select the mode belonging to the animated window.
  constexpr UINT32 kDisplayConfigFlags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;
  for (int attempt = 0; attempt < 3; ++attempt) {
    UINT32 path_count = 0;
    UINT32 mode_count = 0;
    LONG result = GetDisplayConfigBufferSizes(kDisplayConfigFlags, &path_count, &mode_count);
    if (result != ERROR_SUCCESS) {
      break;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
    result = QueryDisplayConfig(kDisplayConfigFlags, &path_count, paths.data(), &mode_count,
                                modes.data(), nullptr);
    if (result == ERROR_INSUFFICIENT_BUFFER) {
      continue;
    }
    if (result != ERROR_SUCCESS) {
      break;
    }

    paths.resize(path_count);
    for (const DISPLAYCONFIG_PATH_INFO& path : paths) {
      DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name{};
      source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
      source_name.header.size = sizeof(source_name);
      source_name.header.adapterId = path.sourceInfo.adapterId;
      source_name.header.id = path.sourceInfo.id;
      if (DisplayConfigGetDeviceInfo(&source_name.header) != ERROR_SUCCESS ||
          _wcsicmp(source_name.viewGdiDeviceName, monitor_info.szDevice) != 0) {
        continue;
      }

      const DISPLAYCONFIG_RATIONAL refresh = path.targetInfo.refreshRate;
      if (refresh.Numerator != 0 && refresh.Denominator != 0) {
        const double hertz =
            static_cast<double>(refresh.Numerator) / static_cast<double>(refresh.Denominator);
        if (hertz > 0.0) {
          return hertz;
        }
      }
    }
    break;
  }

  // Older display drivers can omit usable DisplayConfig path data. The
  // current DEVMODE is still monitor-specific because szDevice came from the
  // HMONITOR selected for the animated window.
  DEVMODEW current_mode{};
  current_mode.dmSize = sizeof(current_mode);
  if (EnumDisplaySettingsExW(monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &current_mode, 0) &&
      (current_mode.dmFields & DM_DISPLAYFREQUENCY) != 0 && current_mode.dmDisplayFrequency > 1) {
    return static_cast<double>(current_mode.dmDisplayFrequency);
  }

  return std::nullopt;
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
