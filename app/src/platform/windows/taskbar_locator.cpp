#include "pch.hpp"

#include "platform/windows/taskbar_locator.hpp"

namespace genie::platform {

HWND FindTaskbarWindowForRect(const RECT& rect) {
  const HMONITOR rect_monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
  HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
  if (taskbar != nullptr && MonitorFromWindow(taskbar, MONITOR_DEFAULTTONEAREST) == rect_monitor) {
    return taskbar;
  }

  HWND secondary = nullptr;
  while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) !=
         nullptr) {
    if (MonitorFromWindow(secondary, MONITOR_DEFAULTTONEAREST) == rect_monitor) return secondary;
  }
  return taskbar;
}

}  // namespace genie::platform
