#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

inline bool GenieEnvFlagEnabled(const wchar_t* name) {
  constexpr DWORD kValueSize = 16;
  wchar_t value[kValueSize]{};
  const DWORD length = GetEnvironmentVariableW(name, value, kValueSize);
  if (length == 0 || length >= kValueSize) {
    return false;
  }
  return value[0] == L'1' || value[0] == L'y' || value[0] == L'Y' || value[0] == L't' ||
         value[0] == L'T';
}

inline bool IsTraceLoggingEnabled() {
#ifdef _DEBUG
  static const bool enabled = GenieEnvFlagEnabled(L"GENIE_TRACE");
  return enabled;
#else
  return false;
#endif
}

inline bool IsSynchronousLoggingEnabled() {
#ifdef _DEBUG
  static const bool enabled = GenieEnvFlagEnabled(L"GENIE_LOG_SYNC");
  return enabled;
#else
  return false;
#endif
}

inline const std::wstring& GenieDebugLogPath() {
  static const std::wstring log_path = []() {
    std::wstring path;
    wchar_t env_path[MAX_PATH]{};
    DWORD len = GetEnvironmentVariableW(L"GENIE_DEBUG_LOG", env_path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
      path = env_path;
    } else {
      wchar_t default_path[MAX_PATH]{};
      len = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\GenieEffect\\genie_debug.log", default_path,
                                      MAX_PATH);
      if (len > 0 && len < MAX_PATH) {
        path = default_path;
      } else {
        path = L"genie_debug.log";
      }
    }

    const size_t last_slash = path.find_last_of(L"\\/");
    if (last_slash != std::wstring::npos) {
      CreateDirectoryW(path.substr(0, last_slash).c_str(), nullptr);
    }
    return path;
  }();
  return log_path;
}

inline std::uintmax_t GenieLogFolderSize() {
  std::error_code error;
  const std::filesystem::path folder = std::filesystem::path(GenieDebugLogPath()).parent_path();
  std::uintmax_t total = 0;
  for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
    if (error) break;
    if (entry.is_regular_file(error) && entry.path().filename().wstring().starts_with(L"genie_"
                                                                                      L"debug")) {
      total += entry.file_size(error);
      error.clear();
    }
  }
  return total;
}

inline void CleanupGenieLogs(std::size_t maximum_files = 5,
                             std::uintmax_t maximum_total_bytes = 10u * 1024u * 1024u) {
  std::error_code error;
  const std::filesystem::path active = GenieDebugLogPath();
  const std::filesystem::path folder = active.parent_path();
  if (std::filesystem::exists(active, error) &&
      std::filesystem::file_size(active, error) > 2u * 1024u * 1024u) {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t suffix[40]{};
    swprintf_s(suffix, L"genie_debug_%04u%02u%02u_%02u%02u%02u.log", time.wYear, time.wMonth,
               time.wDay, time.wHour, time.wMinute, time.wSecond);
    std::filesystem::rename(active, folder / suffix, error);
    error.clear();
  }
  std::vector<std::filesystem::directory_entry> logs;
  for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
    if (error) break;
    if (entry.is_regular_file(error) && entry.path() != active &&
        entry.path().filename().wstring().starts_with(L"genie_debug"))
      logs.push_back(entry);
    error.clear();
  }
  std::sort(logs.begin(), logs.end(), [&error](const auto& left, const auto& right) {
    const auto left_time = left.last_write_time(error);
    error.clear();
    const auto right_time = right.last_write_time(error);
    error.clear();
    return left_time < right_time;
  });
  std::uintmax_t total = GenieLogFolderSize();
  while (!logs.empty() && (logs.size() + 1 > maximum_files || total > maximum_total_bytes)) {
    const std::uintmax_t size = logs.front().file_size(error);
    error.clear();
    std::filesystem::remove(logs.front().path(), error);
    error.clear();
    total = total > size ? total - size : 0;
    logs.erase(logs.begin());
  }
}

inline std::string GenieLogToUtf8(const std::wstring& text) {
  if (text.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                       nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    return {};
  }
  std::string utf8(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size,
                      nullptr, nullptr);
  return utf8;
}

inline const std::wstring& GenieLogProcessName() {
  static const std::wstring process_name = [] {
    wchar_t process_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, process_path, MAX_PATH);
    std::wstring name = process_path;
    const size_t slash = name.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
      name.erase(0, slash + 1);
    }
    return name;
  }();
  return process_name;
}

inline HANDLE& GenieLogFileHandle() {
  static HANDLE file = INVALID_HANDLE_VALUE;
  return file;
}

inline SRWLOCK& GenieLogFileLock() {
  static SRWLOCK lock = SRWLOCK_INIT;
  return lock;
}

inline void LogDebugLine(const std::wstring& module_name, const std::wstring& level,
                         const std::wstring& message) {
#ifdef _DEBUG
  const std::wstring& log_path = GenieDebugLogPath();

  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t time_str[64]{};
  swprintf_s(time_str, L"%04d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

  std::wstringstream ss;
  ss << L"[" << time_str << L"] [" << GenieLogProcessName() << L":" << GetCurrentProcessId() << L":"
     << GetCurrentThreadId() << L"] [" << level << L"] [" << module_name << L"] " << message
     << L"\r\n";
  const std::string entry = GenieLogToUtf8(ss.str());

  AcquireSRWLockExclusive(&GenieLogFileLock());
  HANDLE& file = GenieLogFileHandle();
  if (file == INVALID_HANDLE_VALUE) {
    file = CreateFileW(log_path.c_str(), FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      LARGE_INTEGER size{};
      if (GetFileSizeEx(file, &size) && size.QuadPart == 0) {
        DWORD written = 0;
        constexpr unsigned char kUtf8Bom[] = {0xEF, 0xBB, 0xBF};
        WriteFile(file, kUtf8Bom, static_cast<DWORD>(sizeof(kUtf8Bom)), &written, nullptr);
      }
    }
  }
  if (file != INVALID_HANDLE_VALUE && !entry.empty()) {
    DWORD written = 0;
    if (!WriteFile(file, entry.data(), static_cast<DWORD>(entry.size()), &written, nullptr)) {
      CloseHandle(file);
      file = INVALID_HANDLE_VALUE;
    } else if (IsSynchronousLoggingEnabled()) {
      FlushFileBuffers(file);
    }
  }
  ReleaseSRWLockExclusive(&GenieLogFileLock());
#else
  (void)module_name;
  (void)level;
  (void)message;
#endif
}

#ifdef _DEBUG
#define LogDebug(module_name, message) LogDebugLine((module_name), L"DEBUG", (message))
#define LogTrace(module_name, message)                  \
  do {                                                  \
    if (IsTraceLoggingEnabled()) {                      \
      LogDebugLine((module_name), L"TRACE", (message)); \
    }                                                   \
  } while (false)
#else
#define LogDebug(module_name, message) ((void)0)
#define LogTrace(module_name, message) ((void)0)
#endif
