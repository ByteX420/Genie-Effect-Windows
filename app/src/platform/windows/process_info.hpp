#pragma once

#include <optional>
#include <string>
#include <windows.h>

namespace genie::platform {

[[nodiscard]] DWORD WindowProcessId(HWND window);
[[nodiscard]] std::optional<std::string> GetWindowExecutableName(HWND window);
[[nodiscard]] bool IsCurrentProcessElevated();
[[nodiscard]] std::wstring ExecutableDirectory();
[[nodiscard]] std::string ExecutableProductVersion();

}  // namespace genie::platform
