#include "pch.hpp"

#include "platform/windows/window_state.hpp"

#include <array>
#include <dwmapi.h>
#include <string_view>

namespace minimize::platform {
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
    if (class_name == excluded) return true;
  }
  return false;
}

BOOL CALLBACK CollectWindows(HWND window, LPARAM parameter) {
  auto* context = reinterpret_cast<std::pair<HWND, std::vector<HWND>*>*>(parameter);
  if (IsInterestingTopLevelWindow(window, context->first)) context->second->push_back(window);
  return TRUE;
}

}  // namespace

std::optional<WINDOWPLACEMENT> GetWindowPlacementSnapshot(HWND window) {
  if (!IsWindow(window)) return std::nullopt;
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  return GetWindowPlacement(window, &placement) ? std::optional<WINDOWPLACEMENT>(placement)
                                                : std::nullopt;
}

std::optional<RECT> GetExtendedFrameBounds(HWND window) {
  if (!IsWindow(window)) return std::nullopt;
  RECT bounds{};
  bool use_placement = IsIconic(window) != FALSE;
  if (!use_placement) {
    const HRESULT result =
        DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds));
    if (FAILED(result) || bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
      if (!GetWindowRect(window, &bounds)) return std::nullopt;
    }
    use_placement = bounds.left < -30000 && bounds.top < -30000;
  }
  if (use_placement) {
    const auto placement = GetWindowPlacementSnapshot(window);
    if (!placement.has_value()) return std::nullopt;
    bounds = placement->rcNormalPosition;
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitor_info)) {
      const LONG offset_x = monitor_info.rcWork.left - monitor_info.rcMonitor.left;
      const LONG offset_y = monitor_info.rcWork.top - monitor_info.rcMonitor.top;
      OffsetRect(&bounds, offset_x, offset_y);
    }
  }
  return bounds.right > bounds.left && bounds.bottom > bounds.top ? std::optional<RECT>(bounds)
                                                                  : std::nullopt;
}

bool IsInterestingTopLevelWindow(HWND window, HWND ignored_window) {
  if (window == nullptr || window == ignored_window || !IsWindow(window) ||
      !(IsWindowVisible(window) || IsIconic(window)) || GetAncestor(window, GA_ROOT) != window ||
      GetWindow(window, GW_OWNER) != nullptr) {
    return false;
  }
  if ((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) return false;
  BOOL cloaked = FALSE;
  if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
      cloaked) {
    return false;
  }
  wchar_t class_name[256]{};
  if (GetClassNameW(window, class_name, static_cast<int>(std::size(class_name))) > 0 &&
      IsExcludedClassName(class_name)) {
    return false;
  }
  const auto bounds = GetExtendedFrameBounds(window);
  return bounds.has_value() && bounds->right - bounds->left >= 48 &&
         bounds->bottom - bounds->top >= 48;
}

std::vector<HWND> EnumerateTopLevelWindows(HWND ignored_window) {
  std::vector<HWND> windows;
  auto context = std::pair<HWND, std::vector<HWND>*>(ignored_window, &windows);
  EnumWindows(&CollectWindows, reinterpret_cast<LPARAM>(&context));
  return windows;
}

void SetDwmTransitionsDisabled(HWND window, bool disabled) {
  if (!IsWindow(window)) return;
  const BOOL value = disabled ? TRUE : FALSE;
  DwmSetWindowAttribute(window, DWMWA_TRANSITIONS_FORCEDISABLED, &value, sizeof(value));
}

void SetWindowCloaked(HWND window, bool cloaked) {
  if (!IsWindow(window)) return;
  const BOOL value = cloaked ? TRUE : FALSE;
  DwmSetWindowAttribute(window, DWMWA_CLOAKED, &value, sizeof(value));
}

bool SetOwnedWindowRegion(HWND window, HRGN region, bool redraw) {
  if (IsWindow(window) && SetWindowRgn(window, region, redraw ? TRUE : FALSE) != 0) return true;
  if (region != nullptr) DeleteObject(region);
  return false;
}

bool BringWindowToCaptureForeground(HWND window) {
  if (!IsWindow(window) || IsIconic(window)) return false;
  const bool was_topmost = (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
  SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  SetForegroundWindow(window);
  BringWindowToTop(window);
  DwmFlush();
  return was_topmost;
}

bool IsExactForegroundWindow(HWND window, HWND ignored_window) {
  HWND foreground = GetForegroundWindow();
  if (foreground == nullptr || foreground == ignored_window) return false;
  return foreground == window || GetAncestor(foreground, GA_ROOT) == window;
}

}  // namespace minimize::platform
