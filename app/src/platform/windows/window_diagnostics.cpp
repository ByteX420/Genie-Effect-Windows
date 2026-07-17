#include "pch.hpp"

#include "platform/windows/window_diagnostics.hpp"

#include <dwmapi.h>
#include <sstream>

#include "core/logger.hpp"

namespace genie::platform::windows {
namespace {

std::wstring DescribeRect(const RECT& rect) {
  std::wstringstream stream;
  stream << L"(" << rect.left << L"," << rect.top << L"," << rect.right << L"," << rect.bottom
         << L")";
  return stream.str();
}

}  // namespace

std::wstring DescribeWindow(HWND window) {
  std::wstringstream stream;
  stream << L"hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(window) << std::dec;
  if (window == nullptr || !IsWindow(window)) return stream.str() + L" invalid";
  wchar_t class_name[256]{};
  wchar_t title[256]{};
  GetClassNameW(window, class_name, 256);
  GetWindowTextW(window, title, 256);
  const LONG_PTR extended_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
  BOOL cloaked = FALSE;
  DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
  RECT bounds{};
  GetWindowRect(window, &bounds);
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  const BOOL has_placement = GetWindowPlacement(window, &placement);
  stream << L" class=\"" << class_name << L"\" title=\"" << title << L"\" visible="
         << (IsWindowVisible(window) != FALSE) << L" iconic=" << (IsIconic(window) != FALSE)
         << L" zoomed=" << (IsZoomed(window) != FALSE) << L" cloaked=" << cloaked << L" ex_style=0x"
         << std::hex << extended_style << std::dec << L" rect=" << DescribeRect(bounds);
  if (has_placement)
    stream << L" showCmd=" << placement.showCmd << L" flags=0x" << std::hex << placement.flags
           << std::dec << L" normal=" << DescribeRect(placement.rcNormalPosition);
  return stream.str();
}

void TraceWindowEvent(const std::wstring& event_name, HWND window) {
  core::LogTrace(L"App", event_name + L" " + DescribeWindow(window));
}

}  // namespace genie::platform::windows
