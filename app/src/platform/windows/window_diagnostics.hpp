#pragma once

#include <string>
#include <windows.h>

namespace minimize::platform::windows {

[[nodiscard]] std::wstring DescribeWindow(HWND window);
void TraceWindowEvent(const std::wstring& event_name, HWND window);

}  // namespace minimize::platform::windows
