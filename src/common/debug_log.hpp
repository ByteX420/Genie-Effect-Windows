#pragma once
#include <chrono>
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

inline void LogDebugLine(const std::wstring& module_name, const std::wstring& level,
                         const std::wstring& message) {
#ifdef _DEBUG
  const std::wstring& log_path = GenieDebugLogPath();

  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t time_str[64]{};
  swprintf_s(time_str, L"%04d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

  wchar_t proc_path[MAX_PATH]{};
  GetModuleFileNameW(nullptr, proc_path, MAX_PATH);
  std::wstring proc_name = proc_path;
  size_t slash = proc_name.find_last_of(L"\\/");
  if (slash != std::wstring::npos) {
    proc_name = proc_name.substr(slash + 1);
  }

  std::wstringstream ss;
  ss << L"[" << time_str << L"] [" << proc_name << L":" << GetCurrentProcessId() << L":"
     << GetCurrentThreadId() << L"] [" << level << L"] [" << module_name << L"] " << message
     << L"\r\n";
  const std::string entry = GenieLogToUtf8(ss.str());

  for (int i = 0; i < 10; ++i) {
    HANDLE file =
        CreateFileW(log_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file != INVALID_HANDLE_VALUE) {
      LARGE_INTEGER size{};
      if (GetFileSizeEx(file, &size) && size.QuadPart == 0) {
        DWORD written = 0;
        const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        WriteFile(file, bom, static_cast<DWORD>(sizeof(bom)), &written, nullptr);
      }
      DWORD written = 0;
      if (!entry.empty()) {
        WriteFile(file, entry.data(), static_cast<DWORD>(entry.size()), &written, nullptr);
      }
      if (IsSynchronousLoggingEnabled()) {
        FlushFileBuffers(file);
      }
      CloseHandle(file);
      break;
    }
    Sleep(5);
  }
#else
  (void)module_name;
  (void)level;
  (void)message;
#endif
}

inline void LogDebug(const std::wstring& module_name, const std::wstring& message) {
  LogDebugLine(module_name, L"DEBUG", message);
}

inline void LogTrace(const std::wstring& module_name, const std::wstring& message) {
  if (IsTraceLoggingEnabled()) {
    LogDebugLine(module_name, L"TRACE", message);
  }
}
