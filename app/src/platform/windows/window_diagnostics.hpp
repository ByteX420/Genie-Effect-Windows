#pragma once

#include <string>
#include <windows.h>

namespace genie::platform::windows {

[[nodiscard]] std::wstring DescribeWindow(HWND window);
void TraceWindowEvent(const std::wstring& event_name, HWND window);

}  // namespace genie::platform::windows
