#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <windows.h>

namespace minimize::platform {

[[nodiscard]] DWORD WindowProcessId(HWND window);
[[nodiscard]] std::optional<std::string> GetWindowExecutableName(HWND window);
[[nodiscard]] bool IsCurrentProcessElevated();
[[nodiscard]] std::wstring ExecutableDirectory();
[[nodiscard]] std::string ExecutableProductVersion();
[[nodiscard]] std::string FileProductVersion(std::wstring_view file_path);

}  // namespace minimize::platform
