#pragma once

#include <optional>
#include <string>
#include <windows.h>

namespace minimize::platform {

[[nodiscard]] DWORD WindowProcessId(HWND window);
[[nodiscard]] std::optional<std::string> GetWindowExecutableName(HWND window);
[[nodiscard]] bool IsCurrentProcessElevated();
[[nodiscard]] std::wstring ExecutableDirectory();
[[nodiscard]] std::string ExecutableProductVersion();

}  // namespace minimize::platform
