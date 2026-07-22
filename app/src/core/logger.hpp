#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace minimize::core {

[[nodiscard]] bool IsTraceLoggingEnabled();
[[nodiscard]] const std::wstring& DebugLogPath();
[[nodiscard]] std::uintmax_t DebugLogFolderSize();
void CleanupDebugLogs(std::size_t maximum_files = 5,
                      std::uintmax_t maximum_total_bytes = 10u * 1024u * 1024u);
void ShutdownLogger();

#ifdef _DEBUG
void LogDebug(std::wstring_view module_name, std::wstring_view message);
void LogTrace(std::wstring_view module_name, std::wstring_view message);
#else
inline void LogDebug(std::wstring_view, std::wstring_view) {}
inline void LogTrace(std::wstring_view, std::wstring_view) {}
#endif

}  // namespace minimize::core
