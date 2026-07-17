#include "pch.hpp"

#include "platform/windows/process_info.hpp"

#include <format>
#include <string_view>
#include <vector>

#pragma comment(lib, "version.lib")

namespace genie::platform {
namespace {

std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) return {};
  const int length =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<std::size_t>(length), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(), length, nullptr,
                          nullptr) != length) {
    return {};
  }
  return result;
}

}  // namespace

DWORD WindowProcessId(HWND window) {
  DWORD process_id = 0;
  if (window != nullptr) GetWindowThreadProcessId(window, &process_id);
  return process_id;
}

std::optional<std::string> GetWindowExecutableName(HWND window) {
  if (window == nullptr || !IsWindow(window)) return std::nullopt;
  const DWORD process_id = WindowProcessId(window);
  if (process_id == 0) return std::nullopt;

  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (process == nullptr) return std::nullopt;
  std::vector<wchar_t> path(32768);
  DWORD length = static_cast<DWORD>(path.size());
  const BOOL queried = QueryFullProcessImageNameW(process, 0, path.data(), &length);
  CloseHandle(process);
  if (!queried || length == 0) return std::nullopt;

  const std::wstring_view full_path(path.data(), length);
  const std::size_t separator = full_path.find_last_of(L"\\/");
  const std::wstring_view filename =
      separator == std::wstring_view::npos ? full_path : full_path.substr(separator + 1);
  std::string utf8 = WideToUtf8(filename);
  return utf8.empty() ? std::nullopt : std::optional<std::string>(std::move(utf8));
}

bool IsCurrentProcessElevated() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
  TOKEN_ELEVATION elevation{};
  DWORD size = sizeof(elevation);
  const bool elevated =
      GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) &&
      elevation.TokenIsElevated != 0;
  CloseHandle(token);
  return elevated;
}

std::wstring ExecutableDirectory() {
  std::wstring path(MAX_PATH, L'\0');
  DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  while (length == path.size()) {
    path.resize(path.size() * 2);
    length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  }
  if (length == 0) return L".\\";
  path.resize(length);
  const std::size_t slash = path.find_last_of(L"\\/");
  return slash == std::wstring::npos ? L".\\" : path.substr(0, slash + 1);
}

std::string ExecutableProductVersion() {
  std::wstring module_path(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
  if (length == 0 || length >= module_path.size()) return {};
  module_path.resize(length);

  DWORD handle = 0;
  const DWORD size = GetFileVersionInfoSizeW(module_path.c_str(), &handle);
  if (size == 0) return {};
  std::vector<BYTE> buffer(size);
  if (!GetFileVersionInfoW(module_path.c_str(), 0, size, buffer.data())) return {};

  struct Translation {
    WORD language;
    WORD codepage;
  };
  Translation* translations = nullptr;
  UINT translation_bytes = 0;
  if (VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation",
                     reinterpret_cast<LPVOID*>(&translations), &translation_bytes) &&
      translations != nullptr && translation_bytes >= sizeof(Translation)) {
    wchar_t key[64]{};
    swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\ProductVersion", translations[0].language,
               translations[0].codepage);
    wchar_t* product_version = nullptr;
    UINT product_length = 0;
    if (VerQueryValueW(buffer.data(), key, reinterpret_cast<LPVOID*>(&product_version),
                       &product_length) &&
        product_version != nullptr && product_length > 1) {
      std::wstring version(product_version);
      while (!version.empty() && (version.back() == L'\0' || version.back() == L' ')) {
        version.pop_back();
      }
      std::string utf8 = WideToUtf8(version);
      if (!utf8.empty()) return utf8;
    }
  }

  VS_FIXEDFILEINFO* fixed = nullptr;
  UINT fixed_length = 0;
  if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&fixed), &fixed_length) ||
      fixed == nullptr || fixed_length < sizeof(VS_FIXEDFILEINFO)) {
    return {};
  }
  const DWORD major = HIWORD(fixed->dwProductVersionMS);
  const DWORD minor = LOWORD(fixed->dwProductVersionMS);
  const DWORD patch = HIWORD(fixed->dwProductVersionLS);
  const DWORD build = LOWORD(fixed->dwProductVersionLS);
  return build == 0 ? std::format("{}.{}.{}", major, minor, patch)
                    : std::format("{}.{}.{}.{}", major, minor, patch, build);
}

}  // namespace genie::platform
