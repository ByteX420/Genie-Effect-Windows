#pragma once

#include <optional>

namespace minimize::platform::windows {

// Returns a process exit code when the command line is an updater-helper invocation.
// Returns std::nullopt for a normal application launch.
[[nodiscard]] std::optional<int> TryRunUpdateInstaller(int argument_count, wchar_t* arguments[]);

}  // namespace minimize::platform::windows
