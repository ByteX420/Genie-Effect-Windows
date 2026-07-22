#include "pch.hpp"

#include "platform/windows/startup_manager.hpp"

#include <vector>

namespace minimize::platform::windows {
namespace {

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"MinimizeEffect";

std::wstring CurrentExecutablePath() {
  std::vector<wchar_t> buffer(512);
  while (buffer.size() <= 32768) {
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) return {};
    if (length < buffer.size() - 1 || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return std::wstring(buffer.data(), length);
    }
    buffer.resize(buffer.size() * 2);
  }
  return {};
}

}  // namespace

bool ConfigureRunAtStartup(bool enabled) {
  HKEY run_key = nullptr;
  const LSTATUS open_status =
      RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0,
                      KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &run_key, nullptr);
  if (open_status != ERROR_SUCCESS) return false;

  LSTATUS status = ERROR_SUCCESS;
  if (enabled) {
    const std::wstring executable_path = CurrentExecutablePath();
    if (executable_path.empty() || executable_path.find(L'"') != std::wstring::npos) {
      RegCloseKey(run_key);
      return false;
    }
    const std::wstring command = L"\"" + executable_path + L"\"";
    status = RegSetValueExW(run_key, kRunValueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(command.c_str()),
                            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
  } else {
    status = RegDeleteValueW(run_key, kRunValueName);
    if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
  }
  RegCloseKey(run_key);
  return status == ERROR_SUCCESS;
}

}  // namespace minimize::platform::windows
