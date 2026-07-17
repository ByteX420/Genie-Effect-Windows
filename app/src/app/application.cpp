#include "pch.hpp"

#include "app/application.hpp"

#include <atomic>
#include <cstring>
#include <dwmapi.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <shellapi.h>
#include <sstream>
#include <string>
#include <string_view>
#include <timeapi.h>

#include "animation/geometry.hpp"
#include "app/resource.hpp"
#include "app/startup_manager.hpp"
#include "common/debug_log.hpp"
#include "platform/window_util.hpp"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "version.lib")

namespace genie::app {
namespace {

constexpr wchar_t kHookDllName[] = L"GenieHookPost.dll";

// Product version from the running EXE's VERSIONINFO (set in GenieEffect.rc).
std::string QueryExecutableProductVersion() {
  wchar_t module_path[MAX_PATH]{};
  if (GetModuleFileNameW(nullptr, module_path, MAX_PATH) == 0) return {};

  DWORD handle = 0;
  const DWORD size = GetFileVersionInfoSizeW(module_path, &handle);
  if (size == 0) return {};

  std::vector<BYTE> buffer(size);
  if (!GetFileVersionInfoW(module_path, 0, size, buffer.data())) return {};

  // Prefer the ProductVersion string if present.
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
    UINT product_len = 0;
    if (VerQueryValueW(buffer.data(), key, reinterpret_cast<LPVOID*>(&product_version),
                       &product_len) &&
        product_version != nullptr && product_len > 1) {
      std::wstring wide(product_version);
      // Trim trailing NULs / whitespace from version resources.
      while (!wide.empty() && (wide.back() == L'\0' || wide.back() == L' ')) wide.pop_back();
      if (!wide.empty()) {
        std::string utf8(wide.size(), '\0');
        const int written =
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
        if (written > 0) {
          utf8.resize(static_cast<size_t>(written));
          return utf8;
        }
      }
    }
  }

  VS_FIXEDFILEINFO* fixed = nullptr;
  UINT fixed_len = 0;
  if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&fixed), &fixed_len) ||
      fixed == nullptr || fixed_len < sizeof(VS_FIXEDFILEINFO)) {
    return {};
  }
  const DWORD major = HIWORD(fixed->dwProductVersionMS);
  const DWORD minor = LOWORD(fixed->dwProductVersionMS);
  const DWORD patch = HIWORD(fixed->dwProductVersionLS);
  const DWORD build = LOWORD(fixed->dwProductVersionLS);
  if (build == 0) return std::format("{}.{}.{}", major, minor, patch);
  return std::format("{}.{}.{}.{}", major, minor, patch, build);
}
constexpr char kCbtProcName[] = "CBTProc";
constexpr char kDecoratedCbtProcName[] = "_CBTProc@12";
constexpr wchar_t kAllowMinimizeProperty[] = L"GenieAllowMinimize";
constexpr wchar_t kAllowRestoreProperty[] = L"GenieAllowRestore";
constexpr wchar_t kExcludedApplicationProperty[] = L"GenieExcludedApplication";
constexpr wchar_t kIsMinimizingProperty[] = L"GenieIsMinimizing";
constexpr wchar_t kOriginalPlacementLeftProperty[] = L"GenieOriginalPlacementLeft";
constexpr wchar_t kOriginalPlacementTopProperty[] = L"GenieOriginalPlacementTop";
constexpr wchar_t kOriginalPlacementRightProperty[] = L"GenieOriginalPlacementRight";
constexpr wchar_t kOriginalPlacementBottomProperty[] = L"GenieOriginalPlacementBottom";
constexpr wchar_t kMovedOffscreenProperty[] = L"GenieMovedOffscreen";
constexpr wchar_t kWasMaximizedProperty[] = L"GenieWasMaximized";
constexpr wchar_t kTransparencySavedProperty[] = L"GenieTransparencySaved";
constexpr wchar_t kOriginalExStyleProperty[] = L"GenieOriginalExStyle";
constexpr wchar_t kWasLayeredProperty[] = L"GenieWasLayered";
constexpr wchar_t kOriginalAlphaProperty[] = L"GenieOriginalAlpha";
constexpr wchar_t kOriginalFlagsProperty[] = L"GenieOriginalFlags";
constexpr std::size_t kMaxPreMinimizeSnapshots = 4;
constexpr DWORD kInitialRendererRecoveryDelayMs = 250;
constexpr DWORD kMaximumRendererRecoveryDelayMs = 4000;
constexpr int kHotkeyBaseId = 4100;
constexpr std::uint32_t kSupportedHotkeyModifiers = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;

using CbtProc = LRESULT(CALLBACK*)(int, WPARAM, LPARAM);

void MakeWindowTransparent(HWND window);

DWORD WindowProcessId(HWND window) {
  DWORD process_id = 0;
  if (window != nullptr) {
    GetWindowThreadProcessId(window, &process_id);
  }
  return process_id;
}

bool IsProcessElevated() {
  bool elevated = false;
  HANDLE token = nullptr;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
      elevated = elevation.TokenIsElevated != 0;
    }
    CloseHandle(token);
  }
  return elevated;
}

struct EmbeddedResource {
  const unsigned char* data = nullptr;
  std::size_t size = 0;
};

EmbeddedResource LoadEmbeddedResource(int resource_id) {
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  HRSRC resource =
      FindResourceW(instance, MAKEINTRESOURCEW(resource_id), reinterpret_cast<LPCWSTR>(RT_RCDATA));
  if (resource == nullptr) return {};
  HGLOBAL loaded = LoadResource(instance, resource);
  if (loaded == nullptr) return {};
  const DWORD size = SizeofResource(instance, resource);
  const void* data = LockResource(loaded);
  if (data == nullptr || size == 0) return {};
  return {static_cast<const unsigned char*>(data), static_cast<std::size_t>(size)};
}

std::uint64_t ResourceFingerprint(const EmbeddedResource& resource) {
  std::uint64_t hash = 14695981039346656037ull;
  for (std::size_t index = 0; index < resource.size; ++index) {
    hash ^= resource.data[index];
    hash *= 1099511628211ull;
  }
  return hash;
}

bool FileMatchesResource(const std::filesystem::path& path, const EmbeddedResource& resource) {
  std::error_code error;
  if (std::filesystem::file_size(path, error) != resource.size || error) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  std::vector<unsigned char> bytes(resource.size);
  input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return input && std::memcmp(bytes.data(), resource.data, resource.size) == 0;
}

std::filesystem::path HookCacheDirectory() {
  const std::wstring settings_path = SettingsFilePath();
  if (!settings_path.empty()) {
    return std::filesystem::path(settings_path).parent_path() / L"hooks";
  }

  std::wstring temporary(MAX_PATH, L'\0');
  const DWORD length = GetTempPathW(static_cast<DWORD>(temporary.size()), temporary.data());
  if (length == 0 || length >= temporary.size()) return {};
  temporary.resize(length);
  return std::filesystem::path(temporary) / L"GenieEffect" / L"hooks";
}

std::wstring ExtractEmbeddedHookDll() {
  const EmbeddedResource hook = LoadEmbeddedResource(IDR_GENIE_HOOK);
  if (hook.data == nullptr) return {};

  const std::filesystem::path directory = HookCacheDirectory();
  if (directory.empty()) return {};
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) return {};

  const std::wstring file_name =
      std::format(L"GenieHookPost-{:016x}.dll", ResourceFingerprint(hook));
  const std::filesystem::path destination = directory / file_name;
  if (FileMatchesResource(destination, hook)) return destination.wstring();

  const std::filesystem::path temporary =
      destination.wstring() + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) return {};
    output.write(reinterpret_cast<const char*>(hook.data), static_cast<std::streamsize>(hook.size));
    output.flush();
    if (!output) {
      output.close();
      std::filesystem::remove(temporary, error);
      return {};
    }
  }

  if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary, error);
    return FileMatchesResource(destination, hook) ? destination.wstring() : std::wstring{};
  }
  return destination.wstring();
}

std::wstring GetExecutableDirectory() {
  std::wstring path(MAX_PATH, L'\0');
  DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  while (length == path.size()) {
    path.resize(path.size() * 2);
    length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  }
  if (length == 0) {
    return L".\\";
  }
  path.resize(length);

  const size_t slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos) {
    return L".\\";
  }
  path.resize(slash + 1);
  return path;
}

genie::animation::RectF ToRectF(const RECT& rect) {
  return genie::animation::RectF{
      .left = static_cast<float>(rect.left),
      .top = static_cast<float>(rect.top),
      .right = static_cast<float>(rect.right),
      .bottom = static_cast<float>(rect.bottom),
  };
}

std::wstring RectTraceString(const RECT& rect) {
  std::wstringstream ss;
  ss << L"(" << rect.left << L"," << rect.top << L"," << rect.right << L"," << rect.bottom << L")";
  return ss.str();
}

std::wstring RectFTraceString(const genie::animation::RectF& rect) {
  std::wstringstream ss;
  ss << L"(" << rect.left << L"," << rect.top << L"," << rect.right << L"," << rect.bottom << L")";
  return ss.str();
}

std::wstring WindowTraceString(HWND window) {
  std::wstringstream ss;
  ss << L"hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(window) << std::dec;
  if (window == nullptr || !IsWindow(window)) {
    ss << L" invalid";
    return ss.str();
  }

  wchar_t class_name[256]{};
  GetClassNameW(window, class_name, 256);
  wchar_t title[256]{};
  GetWindowTextW(window, title, 256);
  LONG_PTR ex_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
  BOOL cloaked = FALSE;
  DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));

  RECT window_rect{};
  GetWindowRect(window, &window_rect);
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  const BOOL has_placement = GetWindowPlacement(window, &placement);

  ss << L" class=\"" << class_name << L"\" title=\"" << title << L"\"" << L" visible="
     << (IsWindowVisible(window) != FALSE) << L" iconic=" << (IsIconic(window) != FALSE)
     << L" zoomed=" << (IsZoomed(window) != FALSE) << L" cloaked=" << cloaked << L" ex_style=0x"
     << std::hex << ex_style << std::dec << L" rect=" << RectTraceString(window_rect);
  if (has_placement) {
    ss << L" showCmd=" << placement.showCmd << L" flags=0x" << std::hex << placement.flags
       << std::dec << L" normal=" << RectTraceString(placement.rcNormalPosition);
  }
  return ss.str();
}

void TraceWindowEvent(const std::wstring& event_name, HWND window) {
  (void)event_name;
  (void)window;
  LogTrace(L"App", event_name + L" " + WindowTraceString(window));
}

std::optional<RECT> ClipRectToVirtualScreen(const RECT& rect) {
  RECT clipped{};
  const RECT virtual_screen = platform::GetVirtualScreenRect();
  if (!IntersectRect(&clipped, &rect, &virtual_screen) || clipped.right <= clipped.left ||
      clipped.bottom <= clipped.top) {
    return std::nullopt;
  }
  return clipped;
}

std::optional<WINDOWPLACEMENT> GetPlacement(HWND window) {
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(WINDOWPLACEMENT);
  if (!GetWindowPlacement(window, &placement)) {
    return std::nullopt;
  }
  return placement;
}

bool IsUsableRect(const RECT& rect) {
  return rect.right > rect.left && rect.bottom > rect.top && rect.left > -30000 &&
         rect.top > -30000;
}

bool IsMinimizedShowCommand(UINT show_command) {
  return show_command == SW_SHOWMINIMIZED || show_command == SW_MINIMIZE ||
         show_command == SW_SHOWMINNOACTIVE;
}

RECT RectWithSizeAt(const RECT& rect, LONG left, LONG top) {
  return RECT{
      .left = left,
      .top = top,
      .right = left + (rect.right - rect.left),
      .bottom = top + (rect.bottom - rect.top),
  };
}

void StoreOriginalPlacementProperties(HWND window, const RECT& rect) {
  SetPropW(window, kOriginalPlacementLeftProperty,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rect.left)));
  SetPropW(window, kOriginalPlacementTopProperty,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rect.top)));
  SetPropW(window, kOriginalPlacementRightProperty,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rect.right)));
  SetPropW(window, kOriginalPlacementBottomProperty,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rect.bottom)));
}

std::optional<RECT> ReadOriginalPlacementProperties(HWND window) {
  RECT rect{
      .left = static_cast<LONG>(
          reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalPlacementLeftProperty))),
      .top = static_cast<LONG>(
          reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalPlacementTopProperty))),
      .right = static_cast<LONG>(
          reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalPlacementRightProperty))),
      .bottom = static_cast<LONG>(
          reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalPlacementBottomProperty))),
  };
  if (!IsUsableRect(rect)) {
    return std::nullopt;
  }
  return rect;
}

void StoreWasMaximizedProperty(HWND window, bool was_maximized) {
  if (was_maximized) {
    SetPropW(window, kWasMaximizedProperty, reinterpret_cast<HANDLE>(1));
  } else {
    RemovePropW(window, kWasMaximizedProperty);
  }
}

bool HasGenieWindowState(HWND window) {
  return GetPropW(window, kMovedOffscreenProperty) != nullptr ||
         GetPropW(window, kTransparencySavedProperty) != nullptr ||
         GetPropW(window, kOriginalExStyleProperty) != nullptr ||
         GetPropW(window, kOriginalPlacementLeftProperty) != nullptr;
}

std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) return {};
  const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                           nullptr, 0, nullptr, nullptr);
  if (required <= 0) return {};
  std::string result(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(),
                      required, nullptr, nullptr);
  return result;
}

animation::AnimationStyle AnimationStyleFromName(std::string_view style) {
  if (style == "Gienie curvy") return animation::AnimationStyle::kCurvy;
  if (style == "Squash") return animation::AnimationStyle::kSquash;
  return animation::AnimationStyle::kClassic;
}

float AnimationStyleDurationScale(std::string_view style) {
  if (style == "Gienie curvy") return 0.78f;
  if (style == "Squash") return 0.55f;
  return 1.0f;
}

void ClearGenieWindowProperties(HWND window) {
  if (!IsWindow(window)) {
    return;
  }
  RemovePropW(window, kOriginalPlacementLeftProperty);
  RemovePropW(window, kOriginalPlacementTopProperty);
  RemovePropW(window, kOriginalPlacementRightProperty);
  RemovePropW(window, kOriginalPlacementBottomProperty);
  RemovePropW(window, kMovedOffscreenProperty);
  RemovePropW(window, kWasMaximizedProperty);
  RemovePropW(window, kIsMinimizingProperty);
  RemovePropW(window, kAllowMinimizeProperty);
  RemovePropW(window, kAllowRestoreProperty);
}

bool WasOrWillRestoreMaximized(HWND window) {
  const std::optional<WINDOWPLACEMENT> placement = GetPlacement(window);
  if (!placement.has_value()) {
    return IsZoomed(window) != FALSE;
  }

  return placement->showCmd == SW_SHOWMAXIMIZED || (placement->flags & WPF_RESTORETOMAXIMIZED) != 0;
}

bool IsCurrentlyMaximized(HWND window) {
  const std::optional<WINDOWPLACEMENT> placement = GetPlacement(window);
  if (placement.has_value() && placement->showCmd == SW_SHOWMAXIMIZED) {
    return true;
  }
  return IsZoomed(window) != FALSE;
}

bool BringWindowForwardForCapture(HWND window) {
  if (!IsWindow(window) || IsIconic(window) != FALSE) {
    return false;
  }

  TraceWindowEvent(L"BringWindowForwardForCapture begin", window);
  const bool was_topmost = (GetWindowLongW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
  const BOOL top_ok =
      SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  const BOOL foreground_ok = SetForegroundWindow(window);
  BringWindowToTop(window);
  DwmFlush();
  (void)top_ok;
  (void)foreground_ok;
  LogTrace(L"App", L"BringWindowForwardForCapture foreground_ok=" +
                       std::to_wstring(foreground_ok != FALSE) + L" top_ok=" +
                       std::to_wstring(top_ok != FALSE) + L" was_topmost=" +
                       std::to_wstring(was_topmost) + L" window " + WindowTraceString(window));
  return was_topmost;
}

bool ForegroundIsExactWindow(HWND window, HWND ignored_window) {
  HWND foreground = GetForegroundWindow();
  if (foreground == nullptr || foreground == ignored_window) {
    return false;
  }
  return foreground == window || GetAncestor(foreground, GA_ROOT) == window;
}

void KeepGenieMinimizedWindowHidden(HWND window) {
  if (!IsWindow(window)) {
    return;
  }

  platform::SetWindowCloaked(window, true);
  MakeWindowTransparent(window);
  DwmFlush();

  if (IsIconic(window) == FALSE) {
    SetPropW(window, kAllowMinimizeProperty, reinterpret_cast<HANDLE>(1));
    ShowWindow(window, SW_SHOWMINNOACTIVE);
    RemovePropW(window, kAllowMinimizeProperty);
  }
}

std::optional<RECT> GetMonitorWorkArea(HWND window, const std::optional<RECT>& fallback) {
  HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
  if (monitor == nullptr && fallback.has_value()) {
    monitor = MonitorFromRect(&*fallback, MONITOR_DEFAULTTONEAREST);
  }

  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(MONITORINFO);
  if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitor_info)) {
    return std::nullopt;
  }
  return monitor_info.rcWork;
}

std::optional<RECT> ResolveAnimationBounds(HWND window) {
  const std::optional<RECT> extended_bounds = platform::GetExtendedFrameBounds(window);

  if (IsCurrentlyMaximized(window)) {
    const std::optional<RECT> work_area = GetMonitorWorkArea(window, extended_bounds);
    if (work_area.has_value()) {
      return work_area;
    }
  }

  if (!extended_bounds.has_value()) {
    std::wcerr << L"GetExtendedFrameBounds failed for hwnd=0x" << std::hex
               << reinterpret_cast<std::uintptr_t>(window) << std::dec << L"\n";
    return std::nullopt;
  }

  auto clipped = ClipRectToVirtualScreen(*extended_bounds);
  if (!clipped.has_value()) {
    RECT vs = platform::GetVirtualScreenRect();
    std::wcerr << L"ClipRectToVirtualScreen failed for hwnd=0x" << std::hex
               << reinterpret_cast<std::uintptr_t>(window) << std::dec << L" bounds=("
               << extended_bounds->left << L"," << extended_bounds->top << L","
               << extended_bounds->right << L"," << extended_bounds->bottom << L")"
               << L" virtual_screen=(" << vs.left << L"," << vs.top << L"," << vs.right << L","
               << vs.bottom << L")\n";
  }
  return clipped;
}

void MakeWindowTransparent(HWND window) {
  if (GetPropW(window, kTransparencySavedProperty) != nullptr) {
    TraceWindowEvent(L"MakeWindowTransparent skipped: already transparent", window);
    return;
  }

  TraceWindowEvent(L"MakeWindowTransparent begin", window);
  LONG_PTR ex_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
  if (!SetPropW(window, kTransparencySavedProperty, reinterpret_cast<HANDLE>(1))) {
    return;
  }
  if (ex_style != 0 &&
      !SetPropW(window, kOriginalExStyleProperty, reinterpret_cast<HANDLE>(ex_style))) {
    RemovePropW(window, kTransparencySavedProperty);
    return;
  }

  BYTE alpha = 255;
  DWORD flags = 0;
  BOOL was_layered = (ex_style & WS_EX_LAYERED) != 0;
  if (was_layered) {
    GetLayeredWindowAttributes(window, nullptr, &alpha, &flags);
  }
  SetPropW(window, kWasLayeredProperty,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(was_layered ? 1 : 0)));
  SetPropW(window, kOriginalAlphaProperty, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(alpha)));
  SetPropW(window, kOriginalFlagsProperty, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(flags)));

  SetWindowLongPtrW(window, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
  SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA);
  TraceWindowEvent(L"MakeWindowTransparent end", window);
}

void RestoreWindowTransparency(HWND window) {
  if (GetPropW(window, kTransparencySavedProperty) == nullptr &&
      GetPropW(window, kOriginalExStyleProperty) == nullptr) {
    TraceWindowEvent(L"RestoreWindowTransparency skipped: no original style", window);
    return;
  }

  TraceWindowEvent(L"RestoreWindowTransparency begin", window);
  BOOL was_layered = GetPropW(window, kWasLayeredProperty) != nullptr &&
                     reinterpret_cast<INT_PTR>(GetPropW(window, kWasLayeredProperty)) != 0;
  BYTE alpha =
      static_cast<BYTE>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalAlphaProperty)));
  DWORD flags =
      static_cast<DWORD>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalFlagsProperty)));

  // Only restore the transparency state that Genie changed. Replaying the
  // complete saved extended style can resurrect a temporary WS_EX_TOPMOST bit
  // or overwrite unrelated style changes made by the target application.
  const LONG_PTR current_ex_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
  if (was_layered) {
    SetWindowLongPtrW(window, GWL_EXSTYLE, current_ex_style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(window, 0, alpha, flags);
  } else {
    SetWindowLongPtrW(window, GWL_EXSTYLE, current_ex_style & ~WS_EX_LAYERED);
  }

  RemovePropW(window, kTransparencySavedProperty);
  RemovePropW(window, kOriginalExStyleProperty);
  RemovePropW(window, kWasLayeredProperty);
  RemovePropW(window, kOriginalAlphaProperty);
  RemovePropW(window, kOriginalFlagsProperty);
  TraceWindowEvent(L"RestoreWindowTransparency end", window);
}

}  // namespace

Application::~Application() {
  CleanupAndRestoreAll();
  if (animation_frame_timer_ != nullptr) {
    CloseHandle(animation_frame_timer_);
    animation_frame_timer_ = nullptr;
  }
}

std::string Application::ReadSessionState() const {
  const std::wstring settings_path = SettingsFilePath();
  if (settings_path.empty()) return {};
  std::filesystem::path path(settings_path);
  path.replace_filename(L"session.state");
  std::ifstream input(path, std::ios::binary);
  std::string state;
  if (input) std::getline(input, state);
  return state;
}

bool Application::WriteSessionState(std::string_view state) const {
  const std::wstring settings_path = SettingsFilePath();
  if (settings_path.empty()) return false;
  std::filesystem::path path(settings_path);
  path.replace_filename(L"session.state");
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  const std::filesystem::path temporary = path.wstring() + L".tmp";
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output << state << '\n';
  output.flush();
  if (!output) {
    output.close();
    std::filesystem::remove(temporary, error);
    return false;
  }
  output.close();
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  return true;
}

bool Application::Initialize(HINSTANCE instance) {
  CleanupGenieLogs();
  instance_ = instance;
  main_thread_id_ = GetCurrentThreadId();
  settings_ = LoadSettings();
  safe_mode_ = false;
  is_enabled_ = settings_.enabled;
  minimize_duration_seconds_ = settings_.minimize_duration;
  restore_duration_seconds_ = settings_.restore_duration;
  if (settings_.run_at_startup && !ConfigureRunAtStartup(true)) {
    LogDebug(L"Startup", L"Could not repair the per-user startup entry; disabling the option");
    settings_.run_at_startup = false;
    if (!SaveSettings(settings_)) {
      LogDebug(L"Startup", L"Could not persist the repaired startup state");
    }
  }
#ifdef _DEBUG
  // Touch log file and grant permissions so AppContainers can write to it
  {
    const std::wstring& log_path = GenieDebugLogPath();
    HANDLE file =
        CreateFileW(log_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      CloseHandle(file);
      platform::GrantAppContainerPermissions(log_path);
    }
  }
#endif

  LogDebug(L"App", L"Application::Initialize started");
  LogTrace(L"App", L"Application::Initialize started");

  animation_frame_timer_ = CreateWaitableTimerExW(
      nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
  animation_frame_timer_is_high_resolution_ = animation_frame_timer_ != nullptr;
  if (animation_frame_timer_ == nullptr) {
    animation_frame_timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
  }

  HealLeftoverWindows();

  if (!IsProcessElevated()) {
    std::wcerr << L"WARNING: Not running as Administrator. Elevated windows (like Task Manager, "
                  L"cmd as Admin, etc.)\n"
               << L"         will NOT be hooked due to Windows UIPI security restrictions.\n"
               << L"         To hook all windows, please run GenieEffect.exe as Administrator.\n\n";
    LogDebug(L"App", L"Warning: Not running as Administrator");
  } else {
    LogDebug(L"App", L"Running as Administrator");
  }

  if (!safe_mode_ && !CreateAnimationRenderer()) return false;

  if (!settings_window_.Initialize(
          instance, [this](bool enabled) { return SetEnabled(enabled); },
          [this](float min_dur, float rest_dur, bool save) {
            return SetAnimationDurations(min_dur, rest_dur, save);
          },
          [this](bool linked) { return SetLinkSpeeds(linked); },
          [this](bool enabled) { return SetDisableAnimationsFullscreen(enabled); },
          [this](bool enabled) { return SetDisableEffectsBatterySaver(enabled); },
          [this](const std::string& minimize, const std::string& restore) {
            return SetEasing(minimize, restore);
          },
          [this](bool is_minimize, animation::CubicBezier bezier, bool save) {
            return SetCustomEasingBezier(is_minimize, bezier, save);
          },
          [this](const std::string& style) { return SetAnimationStyle(style); },
          [this](const std::string& mode) { return SetQualityMode(mode); },
          [this](float strength, bool save) { return SetGenieStrength(strength, save); },
          [this](const std::string& strength) { return SetFadeStrength(strength); },
          [this](bool enabled) { return SetTargetIndicator(enabled); },
          [this](const std::string& behavior) { return SetCloseBehavior(behavior); },
          [this](bool run_at_startup, bool start_minimized) {
            return SetStartupOptions(run_at_startup, start_minimized);
          },
          [this](const std::string& name, bool excluded) {
            return SetApplicationExcluded(name, excluded);
          },
          [this](TemporaryPauseAction action) { SetTemporaryPause(action); },
          [this](HotkeyAction action, HotkeyBinding binding) { return SetHotkey(action, binding); },
          [this](HotkeyAction action) { ExecuteHotkeyAction(action); },
          [this]() { return BuildDiagnosticsSnapshot(); },
          [this](DiagnosticsAction action) { return ExecuteDiagnosticsAction(action); },
          [this]() { HealLeftoverWindows(); }, [this]() { RequestShutdown(); })) {
    return false;
  }
  settings_window_.UpdateState(settings_);
  if (!safe_mode_) RegisterConfiguredHotkeys();
  settings_window_.UpdatePauseState(false, false);
  if (safe_mode_) settings_window_.ShowDiagnosticsPage();
  settings_window_.Show(safe_mode_ || !settings_.start_minimized ||
                        settings_.close_behavior != "tray");
  UpdateFullscreenSuppression(true);
  UpdatePowerState(true);
  RefreshEffectRuntimeState();

  if (!safe_mode_) {
    if (!window_event_monitor_.Start(
            [this](HWND window) { OnMinimizeStart(window); },
            [this](HWND window) { OnRestoreAttempt(window); },
            [this](HWND window, DWORD event) { OnWindowSeen(window, event); })) {
      return false;
    }
  }

  std::wcout << L"Genie minimize monitor is running.\n";
  LogTrace(L"App", L"Application::Initialize completed");
  std::wcout << L"Set GENIE_TASKBAR_RECT=left,top,right,bottom to aim at a "
                L"custom taskbar rectangle.\n";
  std::wcout << L"Close this console window to restore the previous Windows "
                L"animation setting.\n";
  session_started_ = true;
  if (!WriteSessionState(safe_mode_ ? "safe" : "running")) {
    LogDebug(L"SafeMode", L"Failed to write the active session marker");
  }
  return true;
}

int Application::FindRunForWindow(HWND window) const {
  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    if (runs_[i].animating_window == window) {
      return i;
    }
  }
  return -1;
}

int Application::FindAvailableRun() {
  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    if (runs_[i].state == RunState::kIdle && runs_[i].animating_window == nullptr &&
        !runs_[i].overlay.active()) {
      return i;
    }
  }
  runs_.emplace_back();
  if (!InitializeRun(runs_.back())) {
    runs_.pop_back();
    return -1;
  }
  return static_cast<int>(runs_.size() - 1);
}

bool Application::IsOverlayWindow(HWND window) const {
  return std::any_of(runs_.begin(), runs_.end(), [window](const AnimationRun& slot) {
    return slot.overlay.window() == window;
  });
}

bool Application::InitializeRun(AnimationRun& slot) {
  if (!slot.overlay.Initialize(
          instance_, d3d_device_.get(), [this](HWND window) { return OnMinimizeStart(window); },
          [this](HWND window) { return OnRestoreAttempt(window); })) {
    return false;
  }
  slot.overlay.SetAnimationDuration(minimize_duration_seconds_);
  slot.state = RunState::kIdle;
  slot.state_entered_ms = GetTickCount64();
  return true;
}

bool Application::CreateAnimationRenderer() {
  for (AnimationRun& slot : runs_) slot.overlay.Shutdown();
  runs_.clear();
  desktop_capture_.reset();
  d3d_device_.reset();

  d3d_device_ = rendering::D3dDevice::Create();
  if (d3d_device_ == nullptr) {
    return false;
  }
  runs_.emplace_back();
  if (!InitializeRun(runs_.back())) {
    runs_.clear();
    d3d_device_.reset();
    return false;
  }
  desktop_capture_ = std::make_unique<rendering::DesktopCapture>(d3d_device_.get());
  return true;
}

bool Application::AnimationRendererDeviceLost() const {
  const bool overlay_lost = std::any_of(runs_.begin(), runs_.end(), [](const AnimationRun& slot) {
    return slot.overlay.device_lost();
  });
  return overlay_lost || (desktop_capture_ != nullptr && desktop_capture_->device_lost()) ||
         (d3d_device_ != nullptr && d3d_device_->IsDeviceLost());
}

void Application::BeginAnimationRendererRecovery() {
  if (animation_renderer_recovery_pending_) {
    return;
  }

  recent_device_failures_ = std::min(recent_device_failures_ + 1u, 8u);
  LogDebug(L"App", L"Animation renderer device lost; rebuilding D3D resources");
  native_animation_blocker_.Disable();

  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    auto& slot = runs_[i];
    HWND interrupted_window = slot.animating_window;
    const bool interrupted_restore = slot.animating_restore;
    slot.overlay.Shutdown();

    if (interrupted_window != nullptr && IsWindow(interrupted_window)) {
      RestoreWindowFromGenieState(interrupted_window, interrupted_restore);
      if (!interrupted_restore) {
        SetPropW(interrupted_window, kAllowMinimizeProperty, reinterpret_cast<HANDLE>(1));
        ShowWindow(interrupted_window, SW_MINIMIZE);
        RemovePropW(interrupted_window, kAllowMinimizeProperty);
      }
    }

    slot.animating_window = nullptr;
    slot.pending_native_minimize_window = nullptr;
    slot.animating_restore = false;
    slot.live_animation_capture_enabled = false;
    slot.animation_monitor = nullptr;
    slot.animation_frame_interval = std::chrono::steady_clock::duration::zero();
  }

  EndFallbackTimerResolution();

  desktop_capture_.reset();
  restore_snapshots_.clear();
  pre_minimize_snapshots_.clear();
  d3d_device_.reset();

  animation_renderer_recovery_pending_ = true;
  animation_renderer_recovery_delay_ms_ = kInitialRendererRecoveryDelayMs;
  next_animation_renderer_recovery_ms_ = GetTickCount64();
  TryRecoverAnimationRenderer();
}

bool Application::TryRecoverAnimationRenderer() {
  if (!animation_renderer_recovery_pending_) {
    return true;
  }
  const ULONGLONG now = GetTickCount64();
  if (now < next_animation_renderer_recovery_ms_) {
    return false;
  }

  if (CreateAnimationRenderer()) {
    animation_renderer_recovery_pending_ = false;
    animation_renderer_recovery_delay_ms_ = kInitialRendererRecoveryDelayMs;
    if (IsEffectActive()) {
      native_animation_blocker_.Enable(GetOverlayWindow());
    }
    LogDebug(L"App", L"Animation renderer recovery completed");
    return true;
  }

  next_animation_renderer_recovery_ms_ = now + animation_renderer_recovery_delay_ms_;
  animation_renderer_recovery_delay_ms_ =
      std::min(animation_renderer_recovery_delay_ms_ * 2, kMaximumRendererRecoveryDelayMs);
  LogDebug(L"App", L"Animation renderer recovery retry scheduled");
  return false;
}

int Application::Run() {
  MSG message{};
  bool running = true;
#ifdef _DEBUG
  bool device_recovery_test_pending = GenieEnvFlagEnabled(L"GENIE_TEST_DEVICE_RECOVERY");
#endif

  while (running) {
    if (shutting_down_.load(std::memory_order_acquire)) {
      break;
    }

    UpdateTemporaryPause();
    UpdateFullscreenSuppression();
    UpdatePowerState();
    CheckAnimationTimeouts();

    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      if (message.message == WM_QUIT) {
        running = false;
        break;
      }
      if (message.message == WM_DISPLAYCHANGE) {
        for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
          runs_[i].animation_monitor = nullptr;
        }
      }
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    if (!running || shutting_down_.load(std::memory_order_acquire)) {
      break;
    }

    settings_window_.Render();

#ifdef _DEBUG
    if (device_recovery_test_pending) {
      device_recovery_test_pending = false;
      BeginAnimationRendererRecovery();
    }
#endif

    if (AnimationRendererDeviceLost()) {
      BeginAnimationRendererRecovery();
    }
    if (animation_renderer_recovery_pending_ && !TryRecoverAnimationRenderer()) {
      MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
      continue;
    }

    for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
      auto& slot = runs_[i];
      if (slot.pending_native_minimize_window != nullptr) {
        TraceWindowEvent(L"Run pending_native_minimize before CompletePendingNativeMinimize",
                         slot.pending_native_minimize_window);
        CompletePendingNativeMinimize(i);
        TraceWindowEvent(L"Run pending_native_minimize after CompletePendingNativeMinimize",
                         slot.pending_native_minimize_window);
      }

      if (slot.overlay.active() && !slot.overlay.restoring() && slot.animating_window != nullptr) {
        if (!slot.overlay.clock_started()) {
          const bool is_iconic = IsIconic(slot.animating_window) != FALSE;
          const bool is_moved = GetPropW(slot.animating_window, kMovedOffscreenProperty) != nullptr;
          if (is_iconic || is_moved) {
            slot.overlay.StartAnimationClock();
            SetRunState(i, slot.animating_restore ? RunState::kRestoring : RunState::kAnimating);
            if (slot.pending_native_minimize_window == slot.animating_window) {
              slot.pending_native_minimize_window = nullptr;
            }
            std::wcout << L"Target is minimized, starting animation clock.\n";
          } else {
            const ULONGLONG now = GetTickCount64();
            if (now - slot.minimize_start_time_ms >= 800) {
              HWND stalled_window = slot.animating_window;
              TraceWindowEvent(L"Run minimize timeout aborting stalled animation", stalled_window);
              std::wcerr << L"Genie minimize event timeout before native minimize completed; "
                            L"aborting animation.\n";
              if (stalled_window != nullptr && IsWindow(stalled_window)) {
                platform::SetWindowCloaked(stalled_window, false);
                RestoreWindowTransparency(stalled_window);
                ClearGenieWindowProperties(stalled_window);
                native_animation_blocker_.SetTransitionsDisabledForWindow(stalled_window, false);
              }
              slot.overlay.CancelAnimation();
              slot.live_animation_capture_enabled = false;
              restore_snapshots_.erase(stalled_window);
              slot.animating_window = nullptr;
              slot.animating_restore = false;
              slot.pending_native_minimize_window = nullptr;
              SetRunState(i, RunState::kIdle);
            }
          }
        }
      }

      const bool was_active = slot.overlay.active();
      const bool was_restoring = slot.animating_restore;
      if (was_active && slot.live_animation_capture_enabled) {
        if (slot.animating_window == nullptr || !IsWindow(slot.animating_window) ||
            IsIconic(slot.animating_window) || !IsWindowVisible(slot.animating_window)) {
          slot.live_animation_capture_enabled = false;
        } else {
          const ULONGLONG now_ms = GetTickCount64();
          constexpr ULONGLONG refresh_interval_ms = 16;
          if (now_ms - slot.last_animation_texture_refresh_ms >= refresh_interval_ms) {
            slot.last_animation_texture_refresh_ms = now_ms;
            if (!desktop_capture_->RefreshCapturedTexture(
                    slot.live_animation_bounds, slot.overlay.mutable_captured_texture())) {
              slot.live_animation_capture_enabled = false;
            }
          }
        }
      }

      bool animation_active = false;
      if (was_active) {
        UpdateAnimationFramePacingMonitor(i);
        if (IsAnimationFrameDue(i)) {
          animation_active = slot.overlay.Tick();
          AdvanceAnimationFrameDeadline(i);
        } else {
          animation_active = true;
        }
      }

      if (AnimationRendererDeviceLost()) {
        BeginAnimationRendererRecovery();
        break;
      }

      if (was_active && !animation_active && slot.animating_window != nullptr) {
        slot.live_animation_capture_enabled = false;
        if (was_restoring) {
          RestoreWindowFromGenieState(slot.animating_window);
          DwmFlush();
          slot.overlay.FinishRestoreAnimation();
          restore_snapshots_.erase(slot.animating_window);
          std::wcout << L"Restore animation completed.\n";
        } else {
          RemovePropW(slot.animating_window, kAllowMinimizeProperty);
          HRGN hidden_region = CreateRectRgn(0, 0, 0, 0);
          platform::SetOwnedWindowRegion(slot.animating_window, hidden_region, true);
          std::wcout << L"Minimize animation completed.\n";
        }
        slot.animating_window = nullptr;
        slot.animating_restore = false;
        SetRunState(i, RunState::kIdle);
      }
    }

    bool any_active = false;
    for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
      if (runs_[i].overlay.active()) {
        any_active = true;
      }
    }

    const ULONGLONG now_ms = GetTickCount64();
    if (IsEffectActive() && now_ms - last_snapshot_refresh_ms_ >= 120) {
      last_snapshot_refresh_ms_ = now_ms;
      UpdatePreMinimizeSnapshot(GetForegroundWindow());
    }

    if (IsEffectActive() && FindAvailableRun() != -1) {
      for (auto& [hwnd, snapshot] : restore_snapshots_) {
        (void)snapshot;
        if (FindRunForWindow(hwnd) != -1) {
          continue;
        }
        if (IsWindow(hwnd) && IsWindowVisible(hwnd) && IsGenieWindowRestored(hwnd)) {
          std::wcout << L"Poll: detected restore for hwnd=0x" << std::hex
                     << reinterpret_cast<std::uintptr_t>(hwnd) << std::dec << std::endl;
          OnRestoreAttempt(hwnd);
          break;  // Only handle one at a time
        }
      }
    }

    any_active = false;
    for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
      if (runs_[i].overlay.active()) {
        any_active = true;
      }
    }

    if (!any_active) {
      EndFallbackTimerResolution();
    }

    const bool settings_active = settings_window_.WantsContinuousRendering();
    if (any_active) {
      WaitForAnimationFrameOrMessage();
    } else if (settings_active) {
      MsgWaitForMultipleObjects(0, nullptr, FALSE, 0, QS_ALLINPUT);
    } else {
      MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
    }
  }

  return static_cast<int>(message.wParam);
}

void Application::RequestShutdown() {
  shutting_down_.store(true, std::memory_order_release);
  if (animation_frame_timer_ != nullptr) {
    LARGE_INTEGER wake_now{};
    SetWaitableTimer(animation_frame_timer_, &wake_now, 0, nullptr, nullptr, FALSE);
  }
  // PostQuitMessage works when called from the main thread (settings UI click path).
  // PostThreadMessage covers the same queue if we are already on it, and is the
  // wake path for console-control / other-thread shutdown requests.
  PostQuitMessage(0);
  if (main_thread_id_ != 0) {
    PostThreadMessageW(main_thread_id_, WM_QUIT, 0, 0);
  }
}

void Application::ResetAnimationFramePacing(int run_index, HWND window,
                                            const RECT& animation_bounds) {
  auto& slot = runs_[run_index];
  BeginFallbackTimerResolution();
  slot.live_animation_bounds = animation_bounds;
  if (window != nullptr && IsWindow(window)) {
    const std::optional<RECT> current_bounds = platform::GetExtendedFrameBounds(window);
    if (current_bounds.has_value()) {
      slot.live_animation_bounds = *current_bounds;
    }
  }
  slot.animation_monitor = nullptr;
  slot.animation_frame_interval = std::chrono::steady_clock::duration::zero();
  slot.next_animation_frame_time = std::chrono::steady_clock::now();
  UpdateAnimationFramePacingMonitor(run_index);
}

void Application::UpdateAnimationFramePacingMonitor(int run_index) {
  auto& slot = runs_[run_index];
  RECT monitor_bounds = slot.live_animation_bounds;
  if (slot.animating_window != nullptr && IsWindow(slot.animating_window) &&
      IsIconic(slot.animating_window) == FALSE) {
    const std::optional<RECT> current_bounds =
        platform::GetExtendedFrameBounds(slot.animating_window);
    if (current_bounds.has_value()) {
      monitor_bounds = *current_bounds;
      slot.live_animation_bounds = *current_bounds;
    }
  }

  HMONITOR monitor = nullptr;
  if (slot.animating_window != nullptr && IsWindow(slot.animating_window)) {
    monitor = MonitorFromWindow(slot.animating_window, MONITOR_DEFAULTTONEAREST);
  }
  if (monitor == nullptr) {
    monitor = MonitorFromRect(&monitor_bounds, MONITOR_DEFAULTTONEAREST);
  }
  if (monitor == nullptr || monitor == slot.animation_monitor) {
    return;
  }

  slot.animation_monitor = monitor;
  const std::optional<double> refresh_rate = platform::GetMonitorRefreshRateHz(monitor);
  if (!refresh_rate.has_value() || *refresh_rate <= 0.0) {
    slot.animation_frame_interval = std::chrono::steady_clock::duration::zero();
    slot.next_animation_frame_time = std::chrono::steady_clock::now();
    LogDebug(L"App", L"No monitor refresh rate available; fixed FPS limit disabled");
    return;
  }

  slot.animation_frame_interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(1.0 / *refresh_rate));
  slot.next_animation_frame_time = std::chrono::steady_clock::now() + slot.animation_frame_interval;
  LogDebug(L"App", L"Animation frame pacing monitor=0x" +
                       std::to_wstring(reinterpret_cast<std::uintptr_t>(monitor)) + L" refresh=" +
                       std::to_wstring(*refresh_rate) + L"Hz");
}

bool Application::IsAnimationFrameDue(int run_index) const {
  const auto& slot = runs_[run_index];
  return slot.animation_frame_interval <= std::chrono::steady_clock::duration::zero() ||
         std::chrono::steady_clock::now() >= slot.next_animation_frame_time;
}

void Application::AdvanceAnimationFrameDeadline(int run_index) {
  auto& slot = runs_[run_index];
  if (slot.animation_frame_interval <= std::chrono::steady_clock::duration::zero()) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (slot.next_animation_frame_time == std::chrono::steady_clock::time_point{}) {
    slot.next_animation_frame_time = now + slot.animation_frame_interval;
    return;
  }

  if (slot.next_animation_frame_time <= now) {
    const auto missed_intervals =
        (now - slot.next_animation_frame_time) / slot.animation_frame_interval;
    if (missed_intervals > 0) {
      recent_missed_frames_ =
          std::min(recent_missed_frames_ + static_cast<unsigned int>(missed_intervals), 120u);
    } else if (recent_missed_frames_ > 0) {
      --recent_missed_frames_;
    }
    slot.next_animation_frame_time += slot.animation_frame_interval * (missed_intervals + 1);
  }
}

void Application::WaitForAnimationFrameOrMessage() {
  bool has_valid_interval = false;
  auto earliest_next = std::chrono::steady_clock::time_point::max();

  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    auto& slot = runs_[i];
    if (slot.overlay.active()) {
      if (slot.animation_frame_interval <= std::chrono::steady_clock::duration::zero()) {
        DwmFlush();
        return;
      }
      if (slot.next_animation_frame_time < earliest_next) {
        earliest_next = slot.next_animation_frame_time;
      }
      has_valid_interval = true;
    }
  }

  if (!has_valid_interval) {
    MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (now >= earliest_next) {
    return;
  }

  const auto wait_duration = earliest_next - now;
  if (animation_frame_timer_ != nullptr) {
    const auto hundred_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(wait_duration).count() / 100;
    LARGE_INTEGER due_time{};
    due_time.QuadPart = -std::max<std::int64_t>(1, hundred_ns);
    if (SetWaitableTimerEx(animation_frame_timer_, &due_time, 0, nullptr, nullptr, nullptr, 0)) {
      const HANDLE handles[] = {animation_frame_timer_};
      MsgWaitForMultipleObjectsEx(1, handles, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
      return;
    }
  }

  const auto wait_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(wait_duration).count();
  const DWORD timeout_ms =
      static_cast<DWORD>(std::max<std::int64_t>(1, (wait_ns + 999999) / 1000000));
  MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout_ms, QS_ALLINPUT);
}

void Application::BeginFallbackTimerResolution() {
  if (animation_frame_timer_is_high_resolution_ || fallback_timer_resolution_active_) {
    return;
  }

  TIMECAPS capabilities{};
  if (timeGetDevCaps(&capabilities, sizeof(capabilities)) != TIMERR_NOERROR ||
      capabilities.wPeriodMin == 0) {
    return;
  }
  if (timeBeginPeriod(capabilities.wPeriodMin) == TIMERR_NOERROR) {
    fallback_timer_period_ms_ = capabilities.wPeriodMin;
    fallback_timer_resolution_active_ = true;
  }
}

void Application::EndFallbackTimerResolution() {
  if (!fallback_timer_resolution_active_) {
    return;
  }
  timeEndPeriod(fallback_timer_period_ms_);
  fallback_timer_period_ms_ = 0;
  fallback_timer_resolution_active_ = false;
}

bool Application::SetEnabled(bool enabled) {
  if (is_enabled_ == enabled) {
    settings_window_.UpdateState(settings_);
    return true;
  }

  AppSettings proposed = settings_;
  proposed.enabled = enabled;
  if (!SaveSettings(proposed)) {
    LogDebug(L"Settings", L"Failed to persist enabled state");
    return false;
  }

  is_enabled_ = enabled;
  settings_ = std::move(proposed);
  RefreshEffectRuntimeState();
  settings_window_.UpdateState(settings_);
  return true;
}

const char* Application::RunStateName(RunState state) {
  switch (state) {
    case RunState::kIdle:
      return "Idle";
    case RunState::kCapturing:
      return "Capturing";
    case RunState::kWaitingForNativeMinimize:
      return "WaitingForNativeMinimize";
    case RunState::kAnimating:
      return "Animating";
    case RunState::kRestoring:
      return "Restoring";
    case RunState::kAborting:
      return "Aborting";
    case RunState::kCleaningUp:
      return "CleaningUp";
  }
  return "Unknown";
}

void Application::SetRunState(int run_index, RunState state) {
  if (run_index < 0 || run_index >= static_cast<int>(runs_.size())) return;
  runs_[run_index].state = state;
  runs_[run_index].state_entered_ms = GetTickCount64();
}

void Application::CheckAnimationTimeouts() {
  const ULONGLONG now = GetTickCount64();
  for (int index = 0; index < static_cast<int>(runs_.size()); ++index) {
    AnimationRun& slot = runs_[index];
    if (slot.state == RunState::kIdle) continue;
    ULONGLONG timeout_ms = 10000;
    if (slot.state == RunState::kCapturing) timeout_ms = 2500;
    if (slot.state == RunState::kWaitingForNativeMinimize) timeout_ms = 2000;
    if (slot.state == RunState::kAborting || slot.state == RunState::kCleaningUp) {
      timeout_ms = 1500;
    }
    const ULONGLONG elapsed = now - slot.state_entered_ms;
    if (elapsed <= timeout_ms) continue;

    HWND window = slot.animating_window != nullptr ? slot.animating_window
                                                   : slot.pending_native_minimize_window;
    wchar_t title[128]{};
    if (window != nullptr) GetWindowTextW(window, title, 128);
    LogDebug(L"Watchdog",
             L"Aborting stuck animation=" + std::to_wstring(index) + L" state=" +
                 std::wstring(RunStateName(slot.state),
                              RunStateName(slot.state) + std::strlen(RunStateName(slot.state))) +
                 L" elapsed_ms=" + std::to_wstring(elapsed) + L" hwnd=" +
                 std::to_wstring(reinterpret_cast<std::uintptr_t>(window)) + L" title=\"" + title +
                 L"\"");
    SetRunState(index, RunState::kAborting);
    slot.overlay.CancelAnimation();
    slot.live_animation_capture_enabled = false;
    slot.animating_window = nullptr;
    slot.pending_native_minimize_window = nullptr;
    slot.animating_restore = false;
    SetRunState(index, RunState::kCleaningUp);
    if (window != nullptr && IsWindow(window)) RestoreWindowFromGenieState(window, true);
    restore_snapshots_.erase(window);
    pre_minimize_snapshots_.erase(window);
    slot.animation_monitor = nullptr;
    slot.animation_frame_interval = std::chrono::steady_clock::duration::zero();
    SetRunState(index, RunState::kIdle);
  }
}

bool Application::IsTemporarilyPaused() const {
  return paused_until_restart_ ||
         (paused_until_tick_ms_ != 0 && GetTickCount64() < paused_until_tick_ms_);
}

bool Application::IsEffectActive() const {
  return !safe_mode_ && is_enabled_ && !IsTemporarilyPaused() && !fullscreen_suppressed_ &&
         !battery_saver_suppressed_;
}

bool Application::IsFullscreenApplicationActive() const {
  const HWND window = GetForegroundWindow();
  if (window == nullptr || window == settings_window_.hwnd() || IsOverlayWindow(window) ||
      WindowProcessId(window) == GetCurrentProcessId() || IsWindowVisible(window) == FALSE ||
      IsIconic(window) != FALSE || IsZoomed(window) != FALSE ||
      !platform::IsInterestingTopLevelWindow(window, GetOverlayWindow())) {
    return false;
  }

  RECT bounds{};
  const std::optional<RECT> extended_bounds = platform::GetExtendedFrameBounds(window);
  if (extended_bounds.has_value()) {
    bounds = *extended_bounds;
  } else if (!GetWindowRect(window, &bounds)) {
    return false;
  }
  const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitor_info)) return false;

  constexpr int kBoundsTolerance = 2;
  const RECT& monitor_bounds = monitor_info.rcMonitor;
  const bool covers_monitor = bounds.left <= monitor_bounds.left + kBoundsTolerance &&
                              bounds.top <= monitor_bounds.top + kBoundsTolerance &&
                              bounds.right >= monitor_bounds.right - kBoundsTolerance &&
                              bounds.bottom >= monitor_bounds.bottom - kBoundsTolerance;
  if (!covers_monitor) return false;

  const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
  return (style & WS_CAPTION) == 0 || (style & WS_THICKFRAME) == 0;
}

void Application::UpdateFullscreenSuppression(bool force) {
  const ULONGLONG now = GetTickCount64();
  if (!force && now - last_fullscreen_check_ms_ < 500) return;
  last_fullscreen_check_ms_ = now;
  const bool suppressed =
      settings_.disable_animations_fullscreen && IsFullscreenApplicationActive();
  if (fullscreen_suppressed_ == suppressed) return;
  fullscreen_suppressed_ = suppressed;
  LogDebug(L"Fullscreen", suppressed ? L"Fullscreen application detected; effect suspended"
                                     : L"Fullscreen application ended; effect resumed");
  RefreshEffectRuntimeState();
}

void Application::UpdatePowerState(bool force) {
  const ULONGLONG now = GetTickCount64();
  if (!force && now - last_power_check_ms_ < 5000) return;
  last_power_check_ms_ = now;
  SYSTEM_POWER_STATUS status{};
  if (!GetSystemPowerStatus(&status)) return;
  const bool on_battery = status.ACLineStatus == 0;
  const bool saver_active = status.SystemStatusFlag != 0;
  const bool suppressed = settings_.disable_effects_battery_saver && saver_active;
  const bool suppression_changed = battery_saver_suppressed_ != suppressed;
  const bool power_changed =
      running_on_battery_ != on_battery || battery_saver_active_ != saver_active;
  running_on_battery_ = on_battery;
  battery_saver_active_ = saver_active;
  battery_saver_suppressed_ = suppressed;
  if (power_changed) {
    LogDebug(L"Power", L"Power state changed: battery=" + std::to_wstring(on_battery) + L" saver=" +
                           std::to_wstring(saver_active));
  }
  if (suppression_changed) RefreshEffectRuntimeState();
}

void Application::DisableEffectRuntime() {
  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    FinishActiveAnimation(i);
  }
  UninstallCbtHook();
  native_animation_blocker_.Disable();

  std::vector<HWND> tracked_windows;
  for (const auto& [window, snapshot] : restore_snapshots_) {
    (void)snapshot;
    tracked_windows.push_back(window);
  }
  for (HWND window : tracked_windows) {
    RestoreWindowFromGenieState(window, false);
  }
  restore_snapshots_.clear();
  pre_minimize_snapshots_.clear();
  if (desktop_capture_ != nullptr) desktop_capture_->ClearHistory();
  effect_runtime_active_ = false;
}

void Application::EnableEffectRuntime() {
  const bool cbt_hook_installed = InstallCbtHook();
  if (!cbt_hook_installed) {
    LogDebug(L"Pause", L"Global CBT hook unavailable; WinEvent fallback remains active");
  }
  if (!animation_renderer_recovery_pending_ && GetOverlayWindow() != nullptr) {
    native_animation_blocker_.Enable(GetOverlayWindow());
  }
  effect_runtime_active_ = true;
}

void Application::RefreshEffectRuntimeState() {
  const bool should_be_active = IsEffectActive();
  if (should_be_active != effect_runtime_active_) {
    if (should_be_active) {
      EnableEffectRuntime();
    } else {
      DisableEffectRuntime();
    }
  }
  ApplyExclusionTransitionOverrides();
}

void Application::SetTemporaryPause(TemporaryPauseAction action) {
  paused_until_restart_ = action == TemporaryPauseAction::kUntilRestart;
  paused_until_tick_ms_ = 0;
  if (action == TemporaryPauseAction::kTenMinutes) {
    paused_until_tick_ms_ = GetTickCount64() + 10ULL * 60ULL * 1000ULL;
  } else if (action == TemporaryPauseAction::kOneHour) {
    paused_until_tick_ms_ = GetTickCount64() + 60ULL * 60ULL * 1000ULL;
  }
  RefreshEffectRuntimeState();
  settings_window_.UpdatePauseState(IsTemporarilyPaused(), paused_until_restart_);
}

void Application::UpdateTemporaryPause() {
  if (paused_until_tick_ms_ == 0 || GetTickCount64() < paused_until_tick_ms_) return;
  paused_until_tick_ms_ = 0;
  LogDebug(L"Pause", L"Temporary pause expired; resuming Genie Effect");
  RefreshEffectRuntimeState();
  settings_window_.UpdatePauseState(false, false);
}

void Application::UnregisterAllHotkeys() {
  if (settings_window_.hwnd() == nullptr) return;
  for (size_t index = 0; index < static_cast<size_t>(HotkeyAction::kCount); ++index) {
    UnregisterHotKey(settings_window_.hwnd(), kHotkeyBaseId + static_cast<int>(index));
  }
}

void Application::RegisterConfiguredHotkeys() {
  UnregisterAllHotkeys();
  for (size_t index = 0; index < settings_.hotkeys.size(); ++index) {
    const HotkeyBinding& binding = settings_.hotkeys[index];
    bool duplicate = false;
    if (binding.virtual_key != 0) {
      for (size_t previous = 0; previous < index; ++previous) {
        if (settings_.hotkeys[previous].virtual_key == binding.virtual_key &&
            settings_.hotkeys[previous].modifiers == binding.modifiers) {
          duplicate = true;
          break;
        }
      }
    }
    const bool available =
        binding.virtual_key == 0 ||
        (!duplicate &&
         RegisterHotKey(settings_window_.hwnd(), kHotkeyBaseId + static_cast<int>(index),
                        binding.modifiers | MOD_NOREPEAT, binding.virtual_key) != FALSE);
    settings_window_.SetHotkeyRegistrationStatus(static_cast<HotkeyAction>(index), available);
    if (!available) {
      LogDebug(L"Hotkey",
               L"Configured hotkey could not be registered for action " + std::to_wstring(index));
    }
  }
}

HotkeyUpdateResult Application::SetHotkey(HotkeyAction action, HotkeyBinding binding) {
  const size_t index = static_cast<size_t>(action);
  if (index >= settings_.hotkeys.size() || binding.virtual_key > 254 ||
      (binding.modifiers & ~kSupportedHotkeyModifiers) != 0) {
    return HotkeyUpdateResult::kInvalid;
  }
  if (binding.virtual_key == 0) binding.modifiers = 0;
  if (binding.virtual_key == VK_CONTROL || binding.virtual_key == VK_SHIFT ||
      binding.virtual_key == VK_MENU || binding.virtual_key == VK_LWIN ||
      binding.virtual_key == VK_RWIN) {
    return HotkeyUpdateResult::kInvalid;
  }
  for (size_t other = 0; other < settings_.hotkeys.size(); ++other) {
    if (other != index && binding.virtual_key != 0 &&
        settings_.hotkeys[other].virtual_key == binding.virtual_key &&
        settings_.hotkeys[other].modifiers == binding.modifiers) {
      return HotkeyUpdateResult::kDuplicate;
    }
  }

  const HotkeyBinding previous = settings_.hotkeys[index];
  const int identifier = kHotkeyBaseId + static_cast<int>(index);
  if (previous.virtual_key != 0) UnregisterHotKey(settings_window_.hwnd(), identifier);
  if (binding.virtual_key != 0 &&
      RegisterHotKey(settings_window_.hwnd(), identifier, binding.modifiers | MOD_NOREPEAT,
                     binding.virtual_key) == FALSE) {
    const bool restored =
        previous.virtual_key == 0 ||
        RegisterHotKey(settings_window_.hwnd(), identifier, previous.modifiers | MOD_NOREPEAT,
                       previous.virtual_key) != FALSE;
    settings_window_.SetHotkeyRegistrationStatus(action, restored);
    return HotkeyUpdateResult::kUnavailable;
  }

  AppSettings proposed = settings_;
  proposed.hotkeys[index] = binding;
  if (!SaveSettings(proposed)) {
    if (binding.virtual_key != 0) UnregisterHotKey(settings_window_.hwnd(), identifier);
    const bool restored =
        previous.virtual_key == 0 ||
        RegisterHotKey(settings_window_.hwnd(), identifier, previous.modifiers | MOD_NOREPEAT,
                       previous.virtual_key) != FALSE;
    settings_window_.SetHotkeyRegistrationStatus(action, restored);
    return HotkeyUpdateResult::kSaveFailed;
  }

  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  RegisterConfiguredHotkeys();
  return HotkeyUpdateResult::kSuccess;
}

void Application::ExecuteHotkeyAction(HotkeyAction action) {
  switch (action) {
    case HotkeyAction::kToggleEffect:
      (void)SetEnabled(!is_enabled_);
      break;
    case HotkeyAction::kOpenSettings:
      settings_window_.Show(true);
      break;
    case HotkeyAction::kRepairWindows:
      HealLeftoverWindows();
      break;
    case HotkeyAction::kCount:
      break;
  }
}

DiagnosticsSnapshot Application::BuildDiagnosticsSnapshot() const {
  DiagnosticsSnapshot snapshot;
  snapshot.effect = IsEffectActive() ? "Enabled" : "Paused";
  snapshot.hook = cbt_hook_ != nullptr ? "Installed" : "Not installed";
  snapshot.renderer = animation_renderer_recovery_pending_ ? "Recovering" : "Healthy";
  snapshot.d3d_device =
      d3d_device_ != nullptr && !d3d_device_->IsDeviceLost() ? "OK" : "Unavailable";
  int active_animations = 0;
  for (const AnimationRun& slot : runs_) {
    if (slot.animating_window != nullptr || slot.overlay.active()) ++active_animations;
  }
  snapshot.active_animations = std::to_string(active_animations);
  snapshot.watchdog = "Per-window cleanup enabled";
  snapshot.startup_repair = startup_repair_status_;
  snapshot.log_folder_size = std::format("{:.2f} MB", GenieLogFolderSize() / (1024.0 * 1024.0));
  snapshot.version = QueryExecutableProductVersion();
  if (snapshot.version.empty()) {
    // Fallback only if the PE has no VERSIONINFO (should not happen after resource link).
    snapshot.version = std::string("dev ") + __DATE__;
  }
#ifdef _DEBUG
  snapshot.version += " (Debug)";
#endif

  OSVERSIONINFOW version{};
  version.dwOSVersionInfoSize = sizeof(version);
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  const auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
      GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
  if (rtl_get_version != nullptr && rtl_get_version(&version) == 0) {
    snapshot.windows_version = std::to_string(version.dwMajorVersion) + "." +
                               std::to_string(version.dwMinorVersion) + " build " +
                               std::to_string(version.dwBuildNumber);
  } else {
    snapshot.windows_version = "Unavailable";
  }

  snapshot.graphics_adapter = "Unavailable";
  if (d3d_device_ != nullptr && d3d_device_->dxgi_device() != nullptr) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (SUCCEEDED(d3d_device_->dxgi_device()->GetAdapter(&adapter))) {
      DXGI_ADAPTER_DESC description{};
      if (SUCCEEDED(adapter->GetDesc(&description))) {
        snapshot.graphics_adapter = WideToUtf8(description.Description);
      }
    }
  }

  HWND reference_window = last_foreground_window_;
  if (reference_window == nullptr || !IsWindow(reference_window))
    reference_window = GetForegroundWindow();
  HMONITOR monitor = reference_window == nullptr
                         ? MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY)
                         : MonitorFromWindow(reference_window, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEXW monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  if (monitor != nullptr && GetMonitorInfoW(monitor, &monitor_info)) {
    snapshot.window_monitor = WideToUtf8(monitor_info.szDevice);
    const std::optional<double> refresh = platform::GetMonitorRefreshRateHz(monitor);
    snapshot.display_refresh = refresh.has_value()
                                   ? std::to_string(static_cast<int>(std::lround(*refresh))) + " Hz"
                                   : "Unavailable";
  } else {
    snapshot.window_monitor = "Unavailable";
    snapshot.display_refresh = "Unavailable";
  }

  snapshot.taskbar = "Unavailable";
  if (reference_window != nullptr && IsWindow(reference_window)) {
    RECT bounds{};
    if (GetWindowRect(reference_window, &bounds)) {
      const platform::TaskbarTarget target =
          taskbar_target_provider_.GetTargetForWindow(reference_window, bounds);
      switch (target.edge) {
        case animation::GenieEdge::kLeft:
          snapshot.taskbar = "Left";
          break;
        case animation::GenieEdge::kTop:
          snapshot.taskbar = "Top";
          break;
        case animation::GenieEdge::kRight:
          snapshot.taskbar = "Right";
          break;
        case animation::GenieEdge::kBottom:
          snapshot.taskbar = "Bottom";
          break;
      }
    }
  }

  int monitor_count = 0;
  EnumDisplayMonitors(
      nullptr, nullptr,
      [](HMONITOR, HDC, LPRECT, LPARAM data) -> BOOL {
        ++*reinterpret_cast<int*>(data);
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&monitor_count));
  const RECT virtual_screen = platform::GetVirtualScreenRect();
  snapshot.monitor_configuration = std::to_string(monitor_count) + " monitor(s), " +
                                   std::to_string(virtual_screen.right - virtual_screen.left) +
                                   "x" + std::to_string(virtual_screen.bottom - virtual_screen.top);

  std::ostringstream report;
  report << "Genie Effect Diagnostics\r\n"
         << "Version: " << snapshot.version << "\r\n"
         << "Windows: " << snapshot.windows_version << "\r\n"
         << "Graphics adapter: " << snapshot.graphics_adapter << "\r\n"
         << "Effect: " << snapshot.effect << "\r\n"
         << "Hook: " << snapshot.hook << "\r\n"
         << "Renderer: " << snapshot.renderer << "\r\n"
         << "D3D Device: " << snapshot.d3d_device << "\r\n"
         << "Active animations: " << snapshot.active_animations << "\r\n"
         << "Watchdog: " << snapshot.watchdog << "\r\n"
         << "Display refresh: " << snapshot.display_refresh << "\r\n"
         << "Window monitor: " << snapshot.window_monitor << "\r\n"
         << "Taskbar: " << snapshot.taskbar << "\r\n"
         << "Monitors: " << snapshot.monitor_configuration << "\r\n"
         << "Startup repair: " << snapshot.startup_repair << "\r\n";
  snapshot.report = report.str();
  return snapshot;
}

bool Application::ExecuteDiagnosticsAction(DiagnosticsAction action) {
  if (action == DiagnosticsAction::kExitSafeMode) return ExitSafeMode();
  if (action == DiagnosticsAction::kRepairWindows) {
    HealLeftoverWindows();
    return true;
  }
  if (action == DiagnosticsAction::kRestartRenderer) {
    if (safe_mode_) return false;
    BeginAnimationRendererRecovery();
    return !animation_renderer_recovery_pending_ && d3d_device_ != nullptr;
  }
  if (action == DiagnosticsAction::kOpenLogFolder) {
    const std::filesystem::path folder = std::filesystem::path(GenieDebugLogPath()).parent_path();
    return reinterpret_cast<INT_PTR>(ShellExecuteW(settings_window_.hwnd(), L"open", folder.c_str(),
                                                   nullptr, nullptr, SW_SHOWNORMAL)) > 32;
  }
  if (action == DiagnosticsAction::kCopy) {
    const std::string report = BuildDiagnosticsSnapshot().report;
    const int required =
        MultiByteToWideChar(CP_UTF8, 0, report.data(), static_cast<int>(report.size()), nullptr, 0);
    if (required <= 0 || !OpenClipboard(settings_window_.hwnd())) return false;
    EmptyClipboard();
    HGLOBAL memory =
        GlobalAlloc(GMEM_MOVEABLE, (static_cast<size_t>(required) + 1) * sizeof(wchar_t));
    if (memory == nullptr) {
      CloseClipboard();
      return false;
    }
    auto* text = static_cast<wchar_t*>(GlobalLock(memory));
    MultiByteToWideChar(CP_UTF8, 0, report.data(), static_cast<int>(report.size()), text, required);
    text[required] = L'\0';
    GlobalUnlock(memory);
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
      GlobalFree(memory);
      CloseClipboard();
      return false;
    }
    CloseClipboard();
    return true;
  }
  return false;
}

bool Application::ExitSafeMode() {
  if (!safe_mode_) return true;
  if (!CreateAnimationRenderer()) return false;
  if (!window_event_monitor_.Start(
          [this](HWND window) { OnMinimizeStart(window); },
          [this](HWND window) { OnRestoreAttempt(window); },
          [this](HWND window, DWORD event) { OnWindowSeen(window, event); })) {
    for (AnimationRun& slot : runs_) slot.overlay.Shutdown();
    desktop_capture_.reset();
    d3d_device_.reset();
    return false;
  }
  safe_mode_ = false;
  RegisterConfiguredHotkeys();
  RefreshEffectRuntimeState();
  if (!WriteSessionState("running")) {
    LogDebug(L"SafeMode", L"Failed to update the session marker after leaving Safe Mode");
  }
  return true;
}

bool Application::SetAnimationDurations(float minimize_duration, float restore_duration,
                                        bool save) {
  minimize_duration_seconds_ = std::clamp(minimize_duration, 0.10f, 2.00f);
  restore_duration_seconds_ = std::clamp(restore_duration, 0.10f, 2.00f);
  settings_.minimize_duration = minimize_duration_seconds_;
  settings_.restore_duration = restore_duration_seconds_;
  const bool saved = !save || SaveSettings(settings_);
  if (!saved) LogDebug(L"Settings", L"Failed to persist animation durations");
  settings_window_.UpdateState(settings_);
  return saved;
}

bool Application::SetLinkSpeeds(bool linked) {
  AppSettings proposed = settings_;
  proposed.link_speeds = linked;
  if (!SaveSettings(proposed)) {
    LogDebug(L"Settings", L"Failed to persist linked speed state");
    return false;
  }
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::SetDisableAnimationsFullscreen(bool enabled) {
  AppSettings proposed = settings_;
  proposed.disable_animations_fullscreen = enabled;
  if (!SaveSettings(proposed)) {
    LogDebug(L"Settings", L"Failed to persist fullscreen animation behavior");
    return false;
  }
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  UpdateFullscreenSuppression(true);
  return true;
}

bool Application::SetDisableEffectsBatterySaver(bool enabled) {
  AppSettings proposed = settings_;
  proposed.disable_effects_battery_saver = enabled;
  if (!SaveSettings(proposed)) {
    LogDebug(L"Settings", L"Failed to persist battery behavior");
    return false;
  }
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  UpdatePowerState(true);
  return true;
}

bool Application::SetEasing(const std::string& minimize_easing, const std::string& restore_easing) {
  constexpr std::array names = {
      std::string_view{"Linear"},      std::string_view{"Ease In"},  std::string_view{"Ease Out"},
      std::string_view{"Ease In Out"}, std::string_view{"Cubic"},    std::string_view{"Back"},
      std::string_view{"Elastic"},     std::string_view{"Custom"},
  };
  const auto valid = [&names](std::string_view value) {
    return std::find(names.begin(), names.end(), value) != names.end();
  };
  if (!valid(minimize_easing) || !valid(restore_easing)) return false;
  AppSettings proposed = settings_;
  proposed.minimize_easing = minimize_easing;
  proposed.restore_easing = restore_easing;
  if (!SaveSettings(proposed)) return false;
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::SetCustomEasingBezier(bool is_minimize, animation::CubicBezier bezier,
                                        bool save) {
  bezier.ClampHandles();
  if (is_minimize)
    settings_.minimize_custom_bezier = bezier;
  else
    settings_.restore_custom_bezier = bezier;
  if (!save) {
    settings_window_.UpdateState(settings_);
    return true;
  }
  if (!SaveSettings(settings_)) return false;
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::SetAnimationStyle(const std::string& style) {
  if (style != "Gienie classic" && style != "Gienie curvy" && style != "Squash") {
    return false;
  }
  AppSettings proposed = settings_;
  proposed.animation_style = style;
  // Keep minimize/restore easing (including Custom bezier) — style only changes mesh shaping.
  if (!SaveSettings(proposed)) return false;
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::SetQualityMode(const std::string& mode) {
  if (mode != "automatic" && mode != "best_quality" && mode != "power_saving") {
    return false;
  }
  AppSettings proposed = settings_;
  proposed.quality_mode = mode;
  if (!SaveSettings(proposed)) return false;
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::SetGenieStrength(float strength, bool save) {
  settings_.genie_strength = std::clamp(strength, 0.25f, 1.0f);
  const bool saved = !save || SaveSettings(settings_);
  settings_window_.UpdateState(settings_);
  return saved;
}

bool Application::SetFadeStrength(const std::string& strength) {
  if (strength != "No fade" && strength != "Subtle" && strength != "Strong") return false;
  AppSettings proposed = settings_;
  proposed.fade_strength = strength;
  if (!SaveSettings(proposed)) return false;
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::SetTargetIndicator(bool enabled) {
  AppSettings proposed = settings_;
  proposed.show_target_indicator = enabled;
  if (!SaveSettings(proposed)) return false;
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  return true;
}

float Application::CalculateAnimationDuration(float base_duration, const RECT& source,
                                              const animation::RectF& target) const {
  (void)source;
  (void)target;
  return base_duration;
}

int Application::SelectMeshSegmentCount(const RECT& source) const {
  if (settings_.quality_mode == "best_quality") return 50;
  if (settings_.quality_mode == "power_saving") return 20;

  int pressure = 0;
  if (running_on_battery_) ++pressure;
  if (battery_saver_active_) pressure += 2;

  const std::int64_t width = std::max<LONG>(0, source.right - source.left);
  const std::int64_t height = std::max<LONG>(0, source.bottom - source.top);
  const std::int64_t pixels = width * height;
  if (pixels >= 3840ll * 2160ll) {
    pressure += 2;
  } else if (pixels >= 2560ll * 1440ll) {
    ++pressure;
  }

  const int active_animations =
      static_cast<int>(std::count_if(runs_.begin(), runs_.end(),
                                     [](const AnimationRun& run) { return run.overlay.active(); }));
  if (active_animations >= 2) {
    pressure += 2;
  } else if (active_animations == 1) {
    ++pressure;
  }

  if (last_capture_duration_ms_ >= 25.0f) {
    pressure += 2;
  } else if (last_capture_duration_ms_ >= 12.0f) {
    ++pressure;
  }
  if (recent_missed_frames_ >= 10) {
    pressure += 2;
  } else if (recent_missed_frames_ >= 3) {
    ++pressure;
  }
  if (recent_device_failures_ > 0 || animation_renderer_recovery_pending_) pressure += 2;

  if (pressure >= 3) return 20;
  if (pressure >= 1) return 35;
  return 50;
}

bool Application::SetCloseBehavior(const std::string& close_behavior) {
  if (close_behavior != "exit" && close_behavior != "tray") return false;
  AppSettings proposed = settings_;
  proposed.close_behavior = close_behavior;
  if (!SaveSettings(proposed)) {
    LogDebug(L"Settings", L"Failed to persist close behavior");
    return false;
  }
  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::SetStartupOptions(bool run_at_startup, bool start_minimized) {
  const AppSettings previous = settings_;
  AppSettings proposed = settings_;
  proposed.run_at_startup = run_at_startup;
  proposed.start_minimized = start_minimized;

  if (!SaveSettings(proposed)) {
    LogDebug(L"Startup", L"Could not persist startup options");
    return false;
  }

  if (run_at_startup != previous.run_at_startup && !ConfigureRunAtStartup(run_at_startup)) {
    LogDebug(L"Startup", L"Could not update the per-user startup entry");
    if (!SaveSettings(previous)) {
      LogDebug(L"Startup", L"Could not roll back persisted startup options");
    }
    return false;
  }

  settings_ = std::move(proposed);
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::SetApplicationExcluded(const std::string& executable_name, bool excluded) {
  std::optional<std::string> normalized = NormalizeExecutableName(executable_name);
  if (!normalized.has_value()) return false;

  const std::vector<std::string> previous = settings_.excluded_applications;
  auto& applications = settings_.excluded_applications;
  const auto existing = std::find_if(
      applications.begin(), applications.end(),
      [&normalized](const std::string& entry) { return ExecutableNamesEqual(entry, *normalized); });
  if (excluded) {
    if (existing != applications.end()) return false;
    applications.push_back(std::move(*normalized));
  } else {
    if (existing == applications.end()) return false;
    applications.erase(existing);
  }
  if (!SaveSettings(settings_)) {
    settings_.excluded_applications = previous;
    return false;
  }
  ApplyExclusionTransitionOverrides();
  settings_window_.UpdateState(settings_);
  return true;
}

bool Application::IsWindowExcluded(HWND window) const {
  const std::optional<std::string> executable_name = platform::GetWindowExecutableName(window);
  return executable_name.has_value() &&
         ContainsExcludedApplication(settings_.excluded_applications, *executable_name);
}

HWND Application::GetOverlayWindow() const {
  return runs_.empty() ? nullptr : runs_[0].overlay.window();
}

void Application::ApplyExclusionTransitionOverrides() {
  for (HWND window : platform::EnumerateTopLevelWindows(GetOverlayWindow())) {
    const bool excluded = IsEffectActive() && IsWindowExcluded(window);
    if (excluded) {
      SetPropW(window, kExcludedApplicationProperty, reinterpret_cast<HANDLE>(1));
    } else {
      RemovePropW(window, kExcludedApplicationProperty);
    }
    platform::SetDwmTransitionsDisabled(window, IsEffectActive() && !excluded);
  }
}

bool Application::InstallCbtHook() {
  if (cbt_hook_ != nullptr) {
    return true;
  }

  std::wstring hook_path = ExtractEmbeddedHookDll();
  if (hook_path.empty()) {
    hook_path = GetExecutableDirectory() + kHookDllName;
  }
  const std::wstring hook_directory =
      std::filesystem::path(hook_path).parent_path().wstring() + L"\\";

  // Programmatically grant AppContainer permissions
  platform::GrantAppContainerPermissions(hook_directory);
  platform::GrantAppContainerPermissions(hook_path);

  hook_dll_ = LoadLibraryW(hook_path.c_str());
  if (hook_dll_ == nullptr) {
    std::wcerr << L"LoadLibraryW(" << hook_path << L") failed: " << GetLastError() << L"\n";
    LogDebug(L"App", L"LoadLibraryW failed for Hook DLL");
    return false;
  }

  FARPROC cbt_proc_address = GetProcAddress(hook_dll_, kCbtProcName);
  if (cbt_proc_address == nullptr) {
    cbt_proc_address = GetProcAddress(hook_dll_, kDecoratedCbtProcName);
  }
  if (cbt_proc_address == nullptr) {
    cbt_proc_address = GetProcAddress(hook_dll_, MAKEINTRESOURCEA(1));
  }
  auto* cbt_proc = reinterpret_cast<CbtProc>(cbt_proc_address);
  if (cbt_proc == nullptr) {
    std::wcerr << L"GetProcAddress(CBTProc) failed: " << GetLastError() << L"\n";
    LogDebug(L"App", L"GetProcAddress failed for CBTProc");
    UninstallCbtHook();
    return false;
  }

  cbt_hook_ = SetWindowsHookExW(WH_CBT, cbt_proc, hook_dll_, 0);
  if (cbt_hook_ == nullptr) {
    std::wcerr << L"SetWindowsHookExW(WH_CBT) failed: " << GetLastError() << L"\n";
    LogDebug(L"App", L"SetWindowsHookExW(WH_CBT) failed");
    UninstallCbtHook();
    return false;
  }

  LogDebug(L"App", L"64-bit CBT hook installed successfully");

  return true;
}

void Application::UninstallCbtHook() {
  LogDebug(L"App", L"UninstallCbtHook called");
  if (cbt_hook_ != nullptr) {
    UnhookWindowsHookEx(cbt_hook_);
    cbt_hook_ = nullptr;
    LogDebug(L"App", L"64-bit CBT hook uninstalled");
  }
  if (hook_dll_ != nullptr) {
    FreeLibrary(hook_dll_);
    hook_dll_ = nullptr;
    LogDebug(L"App", L"Hook DLL freed");
  }
}

bool Application::OnMinimizeStart(HWND window) {
  if (shutting_down_.load(std::memory_order_acquire) || !IsEffectActive() ||
      animation_renderer_recovery_pending_ || desktop_capture_ == nullptr ||
      GetOverlayWindow() == nullptr) {
    return false;
  }
  if (IsWindowExcluded(window)) {
    platform::SetDwmTransitionsDisabled(window, false);
    return false;
  }
  TraceWindowEvent(L"OnMinimizeStart begin", window);
  wchar_t class_name[256]{};
  GetClassNameW(window, class_name, 256);
  wchar_t title[256]{};
  GetWindowTextW(window, title, 256);
  LogDebug(L"App", L"OnMinimizeStart: hwnd=0x" +
                       std::to_wstring(reinterpret_cast<std::uintptr_t>(window)) + L" class=\"" +
                       class_name + L"\" title=\"" + title + L"\"");

  if (in_restore_window_state_) {
    LogDebug(L"App", L"OnMinimizeStart: Ignored because in_restore_window_state_ is true");
    return false;
  }

  if (IsOverlayWindow(window) || !IsWindow(window)) {
    LogDebug(L"App", L"OnMinimizeStart: Ignored because window is overlay or invalid");
    return false;
  }

  if (!platform::IsInterestingTopLevelWindow(window, GetOverlayWindow())) {
    LogDebug(L"App", L"OnMinimizeStart: Ignored because window is not interesting");
    return false;
  }

  int run_index = FindRunForWindow(window);
  if (run_index != -1) {
    auto& slot = runs_[run_index];
    if (slot.pending_native_minimize_window == window ||
        GetPropW(window, kAllowMinimizeProperty) != nullptr) {
      LogDebug(L"App", L"OnMinimizeStart: Allow minimize because already pending/allowed");
      return true;
    }
    slot.animating_restore = false;
    slot.overlay.ContinueMinimizeAnimation();
    SetRunState(run_index, RunState::kAnimating);
    slot.live_animation_capture_enabled = false;
    std::wcout << L"Minimize requested during active animation; continuing "
                  L"toward taskbar.\n";
    LogDebug(L"App", L"OnMinimizeStart: Minimize during active animation, continuing to taskbar");
    return true;
  }

  run_index = FindAvailableRun();
  if (run_index == -1) {
    LogDebug(L"App", L"OnMinimizeStart: Could not create an animation renderer");
    return false;
  }
  auto& slot = runs_[run_index];

  if (restore_snapshots_.count(window) > 0 || GetPropW(window, kIsMinimizingProperty) != nullptr) {
    return true;
  }
  SetRunState(run_index, RunState::kCapturing);

  std::wcout << L"Minimize detected: hwnd=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(window) << std::dec << L"\n";

  struct TopmostRestorer {
    HWND wnd;
    bool was_topmost;
    bool restored = false;

    void RestoreNow() {
      if (restored) {
        return;
      }
      restored = true;
      if (wnd != nullptr && IsWindow(wnd) && !was_topmost) {
        SetWindowPos(wnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      }
    }

    ~TopmostRestorer() { RestoreNow(); }
  } restorer{window, BringWindowForwardForCapture(window)};

  desktop_capture_->ClearHistory();

  const std::optional<RECT> animation_bounds = ResolveAnimationBounds(window);
  if (!animation_bounds.has_value()) {
    TraceWindowEvent(L"OnMinimizeStart failed: no animation bounds", window);
    std::wcerr << L"Minimized window bounds are unavailable for hwnd=0x" << std::hex
               << reinterpret_cast<std::uintptr_t>(window) << std::dec << L" class=\"" << class_name
               << L"\" title=\"" << title << L"\".\n";
    SetRunState(run_index, RunState::kIdle);
    return false;
  }
  LogTrace(L"App", L"OnMinimizeStart animation_bounds=" + RectTraceString(*animation_bounds) +
                       L" window " + WindowTraceString(window));

  const std::optional<WINDOWPLACEMENT> original_placement = GetPlacement(window);
  RECT original_normal_rect =
      original_placement.has_value() ? original_placement->rcNormalPosition : *animation_bounds;
  if (!IsUsableRect(original_normal_rect)) {
    original_normal_rect = *animation_bounds;
  }
  const bool restore_to_maximized = IsCurrentlyMaximized(window);
  LogTrace(L"App", L"OnMinimizeStart captured_current_state restore_to_maximized=" +
                       std::to_wstring(restore_to_maximized) + L" original_normal=" +
                       RectTraceString(original_normal_rect) + L" animation_bounds=" +
                       RectTraceString(*animation_bounds) + L" window " +
                       WindowTraceString(window));

  rendering::CapturedTexture captured_texture;
  const auto capture_started = std::chrono::steady_clock::now();
  RECT source_bounds = *animation_bounds;
  const bool window_is_already_minimized = IsIconic(window) != FALSE;
  RECT captured_window_bounds{};

  PruneSnapshots();
  auto pre_min_it = pre_minimize_snapshots_.find(window);
  const bool has_pre_minimize_snapshot = pre_min_it != pre_minimize_snapshots_.end() &&
                                         pre_min_it->second.texture.shader_resource_view != nullptr;

  if (window_is_already_minimized && has_pre_minimize_snapshot) {
    source_bounds = pre_min_it->second.bounds;
    captured_texture = pre_min_it->second.texture;
    std::wcout << L"Using cached pre-minimize snapshot.\n";
    LogTrace(L"App", L"OnMinimizeStart capture=cached_already_minimized bounds=" +
                         RectTraceString(source_bounds));
  } else if (!window_is_already_minimized &&
             desktop_capture_->CaptureRegion(*animation_bounds, &captured_texture)) {
    std::wcout << L"Using live desktop-region capture.\n";
    LogTrace(L"App", L"OnMinimizeStart capture=desktop bounds=" +
                         RectTraceString(*animation_bounds) + L" texture_size=" +
                         std::to_wstring(captured_texture.size.width) + L"x" +
                         std::to_wstring(captured_texture.size.height));
  } else if (!window_is_already_minimized &&
             desktop_capture_->CaptureWindow(window, *animation_bounds, &captured_texture,
                                             &captured_window_bounds)) {
    source_bounds = captured_window_bounds;
    std::wcout << L"Using live target-window capture fallback.\n";
    LogTrace(L"App", L"OnMinimizeStart capture=window bounds=" + RectTraceString(source_bounds) +
                         L" texture_size=" + std::to_wstring(captured_texture.size.width) + L"x" +
                         std::to_wstring(captured_texture.size.height));
  } else if (has_pre_minimize_snapshot) {
    source_bounds = pre_min_it->second.bounds;
    captured_texture = pre_min_it->second.texture;
    std::wcout << L"Using cached snapshot after live capture failed.\n";
    LogTrace(L"App", L"OnMinimizeStart capture=cached_after_live_failed bounds=" +
                         RectTraceString(source_bounds));
  }
  last_capture_duration_ms_ =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - capture_started)
          .count();

  // The target only needs to be in the topmost band while its pixels are being
  // captured. Restore its original z-order before saving any window styles so
  // the temporary WS_EX_TOPMOST flag can never be persisted and replayed.
  restorer.RestoreNow();

  if (captured_texture.shader_resource_view == nullptr) {
    TraceWindowEvent(L"OnMinimizeStart failed: captured texture missing", window);
    std::wcerr << L"Could not capture window texture; iconic=" << window_is_already_minimized
               << L", cached=" << has_pre_minimize_snapshot << L".\n";
    SetRunState(run_index, RunState::kIdle);
    return false;
  }

  const platform::TaskbarTarget target =
      taskbar_target_provider_.GetTargetForWindow(window, source_bounds);
  LogTrace(L"App", L"OnMinimizeStart taskbar_target rect=" + RectFTraceString(target.rect) +
                       L" edge=" + std::to_wstring(static_cast<int>(target.edge)));

  CachedSnapshot snapshot;
  snapshot.window = window;
  snapshot.bounds = source_bounds;
  snapshot.texture = captured_texture;
  snapshot.target = target;
  snapshot.original_placement = original_normal_rect;
  snapshot.was_maximized = restore_to_maximized;
  snapshot.process_id = WindowProcessId(window);
  snapshot.captured_at_ms = GetTickCount64();

  restore_snapshots_[window] = std::move(snapshot);

  slot.animating_window = window;
  slot.animating_restore = false;
  slot.live_animation_bounds = source_bounds;
  ResetAnimationFramePacing(run_index, window, source_bounds);
  slot.last_animation_texture_refresh_ms = 0;
  slot.live_animation_capture_enabled = false;

  const float animation_duration =
      CalculateAnimationDuration(minimize_duration_seconds_, source_bounds, target.rect) *
      AnimationStyleDurationScale(settings_.animation_style);
  slot.overlay.SetAnimationDuration(animation_duration);
  slot.overlay.SetAnimationEasing(animation::EasingCurveFromName(settings_.minimize_easing),
                                  settings_.minimize_custom_bezier);
  slot.overlay.SetAnimationStyle(AnimationStyleFromName(settings_.animation_style));
  const int mesh_segments = SelectMeshSegmentCount(source_bounds);
  slot.overlay.SetMeshSegmentCount(mesh_segments);
  slot.overlay.SetGenieStrength(std::clamp(settings_.genie_strength, 0.0f, 1.0f));
  slot.overlay.SetFadeStrength(settings_.fade_strength == "Strong"   ? 0.55f
                               : settings_.fade_strength == "Subtle" ? 0.25f
                                                                     : 0.0f);
  slot.overlay.SetTargetIndicatorEnabled(settings_.show_target_indicator);
  LogTrace(L"App", L"Captured minimize duration=" + std::to_wstring(animation_duration) +
                       L" quality=" +
                       std::wstring(settings_.quality_mode.begin(), settings_.quality_mode.end()) +
                       L" segments=" + std::to_wstring(mesh_segments) +
                       L" capture_ms=" + std::to_wstring(last_capture_duration_ms_));
  if (!slot.overlay.StartAnimation(captured_texture, ToRectF(source_bounds), target.rect,
                                   target.edge)) {
    TraceWindowEvent(L"OnMinimizeStart failed: overlay StartAnimation", window);
    std::wcerr << L"Genie animation did not start because overlay start failed.\n";
    restore_snapshots_.erase(window);
    slot.animating_window = nullptr;
    slot.animating_restore = false;
    slot.live_animation_capture_enabled = false;
    SetRunState(run_index, RunState::kIdle);
    return false;
  }
  pre_minimize_snapshots_.erase(window);

  TraceWindowEvent(L"OnMinimizeStart before cloak transparent", window);
  platform::SetWindowCloaked(window, true);
  MakeWindowTransparent(window);
  TraceWindowEvent(L"OnMinimizeStart after cloak transparent", window);

  native_animation_blocker_.SetTransitionsDisabledForWindow(window, true);
  StoreOriginalPlacementProperties(window, original_normal_rect);
  StoreWasMaximizedProperty(window, restore_to_maximized);
  SetPropW(window, kIsMinimizingProperty, reinterpret_cast<HANDLE>(1));

  if (IsIconic(window) == FALSE) {
    SetPropW(window, kAllowMinimizeProperty, reinterpret_cast<HANDLE>(1));
    if (!ShowWindowAsync(window, SW_MINIMIZE)) {
      TraceWindowEvent(L"OnMinimizeStart failed: ShowWindowAsync returned FALSE", window);
      std::wcerr << L"ShowWindowAsync(SW_MINIMIZE) failed; canceling Genie minimize animation.\n";
      RemovePropW(window, kAllowMinimizeProperty);
      platform::SetWindowCloaked(window, false);
      RestoreWindowTransparency(window);
      ClearGenieWindowProperties(window);
      native_animation_blocker_.SetTransitionsDisabledForWindow(window, false);
      slot.overlay.CancelAnimation();
      restore_snapshots_.erase(window);
      slot.animating_window = nullptr;
      slot.animating_restore = false;
      slot.live_animation_capture_enabled = false;
      SetRunState(run_index, RunState::kIdle);
      return false;
    }
    slot.pending_native_minimize_window = window;
    SetRunState(run_index, RunState::kWaitingForNativeMinimize);
  } else {
    slot.pending_native_minimize_window = nullptr;
    slot.overlay.StartAnimationClock();
    SetRunState(run_index, RunState::kAnimating);
    SetPropW(window, kMovedOffscreenProperty, reinterpret_cast<HANDLE>(1));
    auto snap_it = restore_snapshots_.find(window);
    if (snap_it != restore_snapshots_.end()) {
      snap_it->second.moved_offscreen = true;
    }
  }

  slot.minimize_start_time_ms = GetTickCount64();

  TraceWindowEvent(L"OnMinimizeStart completed pending native minimize", window);
  std::wcout << L"Genie overlay visible; native minimize scheduled.\n";
  return true;
}

void Application::FinishActiveAnimation(int run_index) {
  auto& slot = runs_[run_index];
  HWND finished_window = slot.animating_window;
  if (finished_window == nullptr) {
    SetRunState(run_index, RunState::kIdle);
    return;
  }
  SetRunState(run_index, RunState::kCleaningUp);

  const bool was_restoring = slot.animating_restore;
  // Set animating_window to nullptr first to prevent re-entrancy
  slot.animating_window = nullptr;
  slot.animating_restore = false;

  if (was_restoring) {
    TraceWindowEvent(L"FinishActiveAnimation restore completed", finished_window);
    RestoreWindowFromGenieState(finished_window);
    slot.overlay.FinishRestoreAnimation();
    restore_snapshots_.erase(finished_window);
    std::wcout << L"Restore animation forced completion.\n";
  } else {
    TraceWindowEvent(L"FinishActiveAnimation minimize completed", finished_window);
    if (slot.pending_native_minimize_window == finished_window) {
      SetPropW(finished_window, kAllowMinimizeProperty, reinterpret_cast<HANDLE>(1));
      ShowWindow(finished_window, SW_MINIMIZE);
      RemovePropW(finished_window, kAllowMinimizeProperty);
      slot.pending_native_minimize_window = nullptr;
    }
    RemovePropW(finished_window, kAllowMinimizeProperty);
    HRGN hidden_region = CreateRectRgn(0, 0, 0, 0);
    platform::SetOwnedWindowRegion(finished_window, hidden_region, true);
    std::wcout << L"Minimize animation forced completion.\n";
  }

  slot.overlay.CancelAnimation();
  slot.live_animation_capture_enabled = false;

  bool any_other_active = false;
  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    if (runs_[i].overlay.active()) {
      any_other_active = true;
    }
  }
  if (!any_other_active) {
    EndFallbackTimerResolution();
  }

  slot.animation_monitor = nullptr;
  slot.animation_frame_interval = std::chrono::steady_clock::duration::zero();

  if (!any_other_active && animation_frame_timer_ != nullptr) {
    LARGE_INTEGER wake_now{};
    SetWaitableTimer(animation_frame_timer_, &wake_now, 0, nullptr, nullptr, FALSE);
  }
  DwmFlush();
  SetRunState(run_index, RunState::kIdle);
}

void Application::CompletePendingNativeMinimize(int run_index) {
  auto& slot = runs_[run_index];
  HWND window = slot.pending_native_minimize_window;
  TraceWindowEvent(L"CompletePendingNativeMinimize begin", window);

  auto abort_pending_minimize = [this, run_index, window]() {
    TraceWindowEvent(L"CompletePendingNativeMinimize abort", window);
    if (window != nullptr && IsWindow(window)) {
      platform::SetWindowCloaked(window, false);
      RestoreWindowTransparency(window);
      platform::SetOwnedWindowRegion(window, nullptr, true);
      ClearGenieWindowProperties(window);
    }
    runs_[run_index].live_animation_capture_enabled = false;
    if (runs_[run_index].overlay.active() && !runs_[run_index].overlay.restoring()) {
      runs_[run_index].overlay.CancelAnimation();
    }
    if (runs_[run_index].animating_window == window) {
      restore_snapshots_.erase(window);
      runs_[run_index].animating_window = nullptr;
      runs_[run_index].animating_restore = false;
    }
    SetRunState(run_index, RunState::kIdle);
  };

  if (window == nullptr || !IsWindow(window) || slot.animating_window != window ||
      !slot.overlay.active() || slot.overlay.restoring()) {
    TraceWindowEvent(L"CompletePendingNativeMinimize early exit state mismatch", window);
    if (window != nullptr && slot.animating_window == window && !slot.overlay.restoring()) {
      abort_pending_minimize();
    }
    slot.pending_native_minimize_window = nullptr;
    return;
  }

  auto snap_it = restore_snapshots_.find(window);
  if (snap_it == restore_snapshots_.end()) {
    TraceWindowEvent(L"CompletePendingNativeMinimize abort: snapshot missing", window);
    abort_pending_minimize();
    slot.pending_native_minimize_window = nullptr;
    return;
  }

  if (IsIconic(window) == FALSE) {
    // Just wait for the window manager to finish minimizing the window.
    // Do NOT call ShowWindowAsync here to avoid queue flooding.
    return;
  }

  slot.pending_native_minimize_window = nullptr;
  slot.live_animation_capture_enabled = false;

  WINDOWPLACEMENT wp{};
  wp.length = sizeof(wp);
  if (!GetWindowPlacement(window, &wp)) {
    TraceWindowEvent(L"CompletePendingNativeMinimize abort: GetWindowPlacement failed", window);
    std::wcerr << L"Could not read minimized window placement; canceling Genie minimize "
                  L"animation.\n";
    abort_pending_minimize();
    return;
  }

  const bool was_restore_maximized =
      snap_it->second.was_maximized || (wp.flags & WPF_RESTORETOMAXIMIZED) != 0;
  if (was_restore_maximized) {
    SetPropW(window, kWasMaximizedProperty, reinterpret_cast<HANDLE>(1));
  }

  TraceWindowEvent(L"CompletePendingNativeMinimize before final cloak transparent", window);
  platform::SetWindowCloaked(window, true);
  MakeWindowTransparent(window);
  SetPropW(window, kMovedOffscreenProperty, reinterpret_cast<HANDLE>(1));
  TraceWindowEvent(L"CompletePendingNativeMinimize after final cloak transparent moved_offscreen",
                   window);
  snap_it->second.was_maximized = was_restore_maximized;
  snap_it->second.moved_offscreen = true;

  TraceWindowEvent(L"CompletePendingNativeMinimize before StartAnimationClock", window);
  slot.overlay.StartAnimationClock();
  SetRunState(run_index, RunState::kAnimating);
  std::wcout << L"Native minimize completed; starting animation clock.\n";
}

bool Application::PreserveRestorePlacementAndMarkOffscreen(HWND window, CachedSnapshot* snapshot) {
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  if (!GetWindowPlacement(window, &placement)) {
    return false;
  }

  RECT original_rect = placement.rcNormalPosition;
  if (!IsUsableRect(original_rect) && snapshot != nullptr) {
    original_rect = IsUsableRect(snapshot->original_placement) ? snapshot->original_placement
                                                               : snapshot->bounds;
  }
  if (!IsUsableRect(original_rect)) {
    const std::optional<RECT> bounds = platform::GetExtendedFrameBounds(window);
    if (bounds.has_value() && IsUsableRect(*bounds)) {
      original_rect = *bounds;
    }
  }
  if (!IsUsableRect(original_rect)) {
    return false;
  }

  const bool was_maximized =
      IsZoomed(window) != FALSE || (snapshot != nullptr && snapshot->was_maximized);
  if (snapshot != nullptr) {
    if (!IsUsableRect(snapshot->original_placement)) {
      snapshot->original_placement = original_rect;
    }
    snapshot->was_maximized = was_maximized;
    snapshot->moved_offscreen = true;
  }

  StoreOriginalPlacementProperties(window, original_rect);
  SetPropW(window, kMovedOffscreenProperty, reinterpret_cast<HANDLE>(1));
  StoreWasMaximizedProperty(window, was_maximized);
  return true;
}

bool Application::IsGenieWindowRestored(HWND window) const {
  if (restore_snapshots_.count(window) == 0 && GetPropW(window, kIsMinimizingProperty) == nullptr) {
    return false;
  }

  return IsIconic(window) == FALSE;
}

bool Application::OnRestoreAttempt(HWND window) {
  if (!IsEffectActive() || animation_renderer_recovery_pending_ || GetOverlayWindow() == nullptr) {
    return false;
  }
  if (shutting_down_.load(std::memory_order_acquire)) {
    return false;
  }
  if (IsWindowExcluded(window)) {
    platform::SetDwmTransitionsDisabled(window, false);
    const int run_index = FindRunForWindow(window);
    if (run_index != -1) FinishActiveAnimation(run_index);
    const bool has_genie_state = restore_snapshots_.count(window) != 0 ||
                                 GetPropW(window, kIsMinimizingProperty) != nullptr ||
                                 GetPropW(window, kMovedOffscreenProperty) != nullptr;
    if (has_genie_state) {
      RestoreWindowFromGenieState(window, false);
      restore_snapshots_.erase(window);
      pre_minimize_snapshots_.erase(window);
    }
    return false;
  }
  TraceWindowEvent(L"OnRestoreAttempt begin", window);
  wchar_t class_name[256]{};
  GetClassNameW(window, class_name, 256);
  wchar_t title[256]{};
  GetWindowTextW(window, title, 256);

  auto snapshot_it = restore_snapshots_.find(window);
  const bool has_snapshot = snapshot_it != restore_snapshots_.end();
  const bool snapshot_moved_offscreen = has_snapshot && snapshot_it->second.moved_offscreen;
  const bool prop_moved_offscreen = GetPropW(window, kMovedOffscreenProperty) != nullptr;
  const bool is_moved_offscreen = snapshot_moved_offscreen || prop_moved_offscreen;
  const bool window_was_genie_minimized =
      has_snapshot || GetPropW(window, kIsMinimizingProperty) != nullptr;

  LogDebug(L"App", L"OnRestoreAttempt: hwnd=0x" +
                       std::to_wstring(reinterpret_cast<std::uintptr_t>(window)) + L" class=\"" +
                       class_name + L"\" title=\"" + title + L"\"" + L" iconic=" +
                       std::to_wstring(IsIconic(window) != FALSE) + L" genie_minimized=" +
                       std::to_wstring(window_was_genie_minimized) + L" offscreen=" +
                       std::to_wstring(is_moved_offscreen));

  if (in_restore_window_state_) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because in_restore_window_state_ is true");
    return false;
  }

  if (IsOverlayWindow(window) || !IsWindow(window)) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because window is overlay or invalid");
    return false;
  }

  if (!platform::IsInterestingTopLevelWindow(window, GetOverlayWindow())) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because window is not interesting");
    return false;
  }

  std::wcout << L"OnRestoreAttempt: hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(window)
             << std::dec << L" iconic=" << (IsIconic(window) != FALSE) << L" genie_minimized="
             << window_was_genie_minimized << L" offscreen=" << is_moved_offscreen << L"\n";

  int run_index = FindRunForWindow(window);
  if (run_index != -1) {
    auto& slot = runs_[run_index];
    if (slot.pending_native_minimize_window == window) {
      slot.pending_native_minimize_window = nullptr;
    }
    slot.animating_restore = true;
    slot.overlay.ReverseAnimation();
    SetRunState(run_index, RunState::kRestoring);
    slot.live_animation_capture_enabled = false;
    std::wcout << L"Restore requested during active animation; reversing "
                  L"toward window.\n";

    const bool window_is_iconic = IsIconic(window) != FALSE;
    if (!window_is_iconic) {
      TraceWindowEvent(L"OnRestoreAttempt active animation before cloak transparent", window);
      platform::SetWindowCloaked(window, true);
      MakeWindowTransparent(window);
      TraceWindowEvent(L"OnRestoreAttempt active animation after cloak transparent", window);
      if (!is_moved_offscreen) {
        CachedSnapshot* snapshot = has_snapshot ? &snapshot_it->second : nullptr;
        if (!PreserveRestorePlacementAndMarkOffscreen(window, snapshot)) {
          // Release the slot before restoring the real window to prevent re-entrancy.
          slot.animating_window = nullptr;
          slot.pending_native_minimize_window = nullptr;
          slot.animating_restore = false;
          slot.live_animation_capture_enabled = false;
          slot.overlay.CancelAnimation();

          RestoreWindowFromGenieState(window, false);
          restore_snapshots_.erase(window);
          return false;
        }
      }
    }
    return true;
  }

  const bool window_is_iconic = IsIconic(window) != FALSE;
  if (!window_was_genie_minimized && !window_is_iconic) {
    return false;
  }

  if (!window_is_iconic && !is_moved_offscreen) {
    TraceWindowEvent(L"OnRestoreAttempt before pre-animation cloak transparent", window);
    platform::SetWindowCloaked(window, true);
    MakeWindowTransparent(window);
    TraceWindowEvent(L"OnRestoreAttempt after pre-animation cloak transparent", window);
    CachedSnapshot* snapshot = has_snapshot ? &snapshot_it->second : nullptr;
    if (!PreserveRestorePlacementAndMarkOffscreen(window, snapshot)) {
      RestoreWindowFromGenieState(window, false);
      restore_snapshots_.erase(window);
      return false;
    }
  } else if (!window_is_iconic && is_moved_offscreen) {
    TraceWindowEvent(L"OnRestoreAttempt before recloak moved-offscreen window", window);
    platform::SetWindowCloaked(window, true);
    MakeWindowTransparent(window);
    TraceWindowEvent(L"OnRestoreAttempt after recloak moved-offscreen window", window);
  }

  auto it = restore_snapshots_.find(window);
  if (it == restore_snapshots_.end() || it->second.texture.shader_resource_view == nullptr) {
    TraceWindowEvent(L"OnRestoreAttempt failed: missing restore snapshot", window);
    std::wcerr << L"Could not restore with Genie animation; cached snapshot is missing.\n";
    RestoreWindowFromGenieState(window, false);
    return false;
  }
  const CachedSnapshot& current_snapshot = it->second;

  run_index = FindAvailableRun();
  if (run_index == -1) {
    LogDebug(L"App", L"OnRestoreAttempt: Could not create an animation renderer");
    RestoreWindowFromGenieState(window, false);
    restore_snapshots_.erase(window);
    return false;
  }
  SetRunState(run_index, RunState::kRestoring);

  auto& slot = runs_[run_index];
  slot.animating_window = window;
  slot.animating_restore = true;
  slot.live_animation_capture_enabled = false;
  ResetAnimationFramePacing(run_index, window, current_snapshot.bounds);

  native_animation_blocker_.SetTransitionsDisabledForWindow(window, true);

  const float animation_duration =
      CalculateAnimationDuration(restore_duration_seconds_, current_snapshot.bounds,
                                 current_snapshot.target.rect) *
      AnimationStyleDurationScale(settings_.animation_style);
  slot.overlay.SetAnimationDuration(animation_duration);
  slot.overlay.SetAnimationEasing(animation::EasingCurveFromName(settings_.restore_easing),
                                  settings_.restore_custom_bezier);
  slot.overlay.SetAnimationStyle(AnimationStyleFromName(settings_.animation_style));
  const int mesh_segments = SelectMeshSegmentCount(current_snapshot.bounds);
  slot.overlay.SetMeshSegmentCount(mesh_segments);
  slot.overlay.SetGenieStrength(std::clamp(settings_.genie_strength, 0.0f, 1.0f));
  slot.overlay.SetFadeStrength(settings_.fade_strength == "Strong"   ? 0.55f
                               : settings_.fade_strength == "Subtle" ? 0.25f
                                                                     : 0.0f);
  slot.overlay.SetTargetIndicatorEnabled(settings_.show_target_indicator);
  LogTrace(L"App", L"Captured restore duration=" + std::to_wstring(animation_duration) +
                       L" quality=" +
                       std::wstring(settings_.quality_mode.begin(), settings_.quality_mode.end()) +
                       L" segments=" + std::to_wstring(mesh_segments));
  if (!slot.overlay.StartAnimation(current_snapshot.texture, ToRectF(current_snapshot.bounds),
                                   current_snapshot.target.rect, current_snapshot.target.edge, 1.0f,
                                   0.0f)) {
    TraceWindowEvent(L"OnRestoreAttempt failed: overlay StartAnimation", window);
    std::wcerr << L"Restore animation did not start because overlay start failed.\n";
    RestoreWindowFromGenieState(window, true);
    restore_snapshots_.erase(window);
    slot.animating_window = nullptr;
    slot.animating_restore = false;
    SetRunState(run_index, RunState::kIdle);
    return false;
  }

  TraceWindowEvent(L"OnRestoreAttempt before StartAnimationClock", window);
  slot.overlay.StartAnimationClock();

  std::wcout << L"Restore animation started.\n";
  return true;
}

void Application::OnWindowSeen(HWND window, DWORD event) {
  if (shutting_down_.load(std::memory_order_acquire) || !IsEffectActive() ||
      animation_renderer_recovery_pending_) {
    return;
  }
  if (IsOverlayWindow(window)) {
    return;
  }
  if (event == EVENT_SYSTEM_FOREGROUND && WindowProcessId(window) != GetCurrentProcessId() &&
      platform::IsInterestingTopLevelWindow(window, GetOverlayWindow())) {
    last_foreground_window_ = window;
  }
  if (IsWindowExcluded(window)) {
    SetPropW(window, kExcludedApplicationProperty, reinterpret_cast<HANDLE>(1));
    platform::SetDwmTransitionsDisabled(window, false);
    return;
  }
  RemovePropW(window, kExcludedApplicationProperty);
  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    if (window == runs_[i].animating_window) {
      return;
    }
  }

  wchar_t class_name[256]{};
  GetClassNameW(window, class_name, 256);
  LogDebug(L"App", L"OnWindowSeen: hwnd=0x" +
                       std::to_wstring(reinterpret_cast<std::uintptr_t>(window)) + L" class=\"" +
                       class_name + L"\"");

  native_animation_blocker_.SetTransitionsDisabledForWindow(window, true);

  if (IsWindowVisible(window) && IsGenieWindowRestored(window)) {
    std::wcout << L"OnWindowSeen: surprise restore detected for hwnd=0x" << std::hex
               << reinterpret_cast<std::uintptr_t>(window) << std::dec << L"\n";
    LogDebug(L"App", L"OnWindowSeen: surprise restore detected for hwnd=0x" +
                         std::to_wstring(reinterpret_cast<std::uintptr_t>(window)));
    OnRestoreAttempt(window);
    return;
  }

  if (event == EVENT_SYSTEM_FOREGROUND) {
    UpdatePreMinimizeSnapshot(window);
  }
}

void Application::UpdatePreMinimizeSnapshot(HWND window) {
  if (desktop_capture_ == nullptr || animation_renderer_recovery_pending_ ||
      IsOverlayWindow(window) || !IsWindow(window) || IsIconic(window) ||
      !IsWindowVisible(window) || IsWindowExcluded(window)) {
    return;
  }
  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    if (window == runs_[i].animating_window) {
      return;
    }
  }

  const std::optional<RECT> animation_bounds = ResolveAnimationBounds(window);
  if (!animation_bounds.has_value()) {
    TraceWindowEvent(L"UpdatePreMinimizeSnapshot skipped: no bounds", window);
    return;
  }

  rendering::CapturedTexture captured_texture;
  RECT snapshot_bounds = *animation_bounds;
  bool captured = false;
  PruneSnapshots();
  auto existing = pre_minimize_snapshots_.find(window);
  if (existing != pre_minimize_snapshots_.end() &&
      EqualRect(&existing->second.bounds, &snapshot_bounds) &&
      existing->second.texture.texture != nullptr) {
    captured_texture = existing->second.texture;
    captured = desktop_capture_->RefreshCapturedTexture(*animation_bounds, &captured_texture);
  }

  // Prioritize a non-blocking desktop-region capture. When the window bounds
  // are unchanged, RefreshCapturedTexture above reuses the cropped texture and
  // SRV instead of allocating both again on every snapshot refresh.
  if (!captured) {
    captured = desktop_capture_->CaptureRegion(*animation_bounds, &captured_texture);
  }
  if (!captured) {
    RECT captured_window_bounds{};
    if (desktop_capture_->CaptureWindow(window, *animation_bounds, &captured_texture,
                                        &captured_window_bounds)) {
      snapshot_bounds = captured_window_bounds;
    } else {
      LogTrace(L"App", L"UpdatePreMinimizeSnapshot capture failed bounds=" +
                           RectTraceString(*animation_bounds) + L" window " +
                           WindowTraceString(window));
      return;
    }
  }

  CachedSnapshot snapshot;
  snapshot.window = window;
  snapshot.bounds = snapshot_bounds;
  snapshot.texture = captured_texture;
  const std::optional<WINDOWPLACEMENT> placement = GetPlacement(window);
  snapshot.original_placement =
      placement.has_value() ? placement->rcNormalPosition : *animation_bounds;
  if (!IsUsableRect(snapshot.original_placement)) {
    snapshot.original_placement = snapshot_bounds;
  }
  snapshot.was_maximized = IsCurrentlyMaximized(window);
  snapshot.process_id = WindowProcessId(window);
  snapshot.captured_at_ms = GetTickCount64();

  pre_minimize_snapshots_[window] = std::move(snapshot);
  PruneSnapshots();
  LogTrace(L"App", L"UpdatePreMinimizeSnapshot stored bounds=" + RectTraceString(snapshot_bounds) +
                       L" texture_size=" + std::to_wstring(captured_texture.size.width) + L"x" +
                       std::to_wstring(captured_texture.size.height) + L" window " +
                       WindowTraceString(window));
}

void Application::PruneSnapshots() {
  const auto still_matches_window = [](HWND window, const CachedSnapshot& snapshot) {
    return IsWindow(window) && snapshot.process_id != 0 &&
           WindowProcessId(window) == snapshot.process_id;
  };

  for (auto it = restore_snapshots_.begin(); it != restore_snapshots_.end();) {
    if (!still_matches_window(it->first, it->second)) {
      it = restore_snapshots_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = pre_minimize_snapshots_.begin(); it != pre_minimize_snapshots_.end();) {
    if (!still_matches_window(it->first, it->second)) {
      it = pre_minimize_snapshots_.erase(it);
    } else {
      ++it;
    }
  }

  while (pre_minimize_snapshots_.size() > kMaxPreMinimizeSnapshots) {
    auto oldest =
        std::min_element(pre_minimize_snapshots_.begin(), pre_minimize_snapshots_.end(),
                         [](const auto& left, const auto& right) {
                           return left.second.captured_at_ms < right.second.captured_at_ms;
                         });
    if (oldest == pre_minimize_snapshots_.end()) {
      break;
    }
    pre_minimize_snapshots_.erase(oldest);
  }
}

void Application::RestoreWindowFromGenieState(HWND window, bool force_show_if_iconic) {
  if (!IsWindow(window)) {
    return;
  }

  in_restore_window_state_ = true;

  // Restore region
  platform::SetOwnedWindowRegion(window, nullptr, true);

  bool was_maximized = false;
  bool has_restore_rect = false;
  RECT restore_rect{};

  auto snap_it = restore_snapshots_.find(window);
  if (snap_it != restore_snapshots_.end()) {
    was_maximized = snap_it->second.was_maximized;
    if (IsUsableRect(snap_it->second.original_placement)) {
      restore_rect = snap_it->second.original_placement;
      has_restore_rect = true;
    }
  } else {
    auto pre_snap_it = pre_minimize_snapshots_.find(window);
    if (pre_snap_it != pre_minimize_snapshots_.end()) {
      was_maximized = pre_snap_it->second.was_maximized;
      if (IsUsableRect(pre_snap_it->second.original_placement)) {
        restore_rect = pre_snap_it->second.original_placement;
        has_restore_rect = true;
      }
    } else {
      was_maximized = GetPropW(window, kWasMaximizedProperty) != nullptr;
    }
  }

  if (!has_restore_rect) {
    const std::optional<RECT> prop_rect = ReadOriginalPlacementProperties(window);
    if (prop_rect.has_value()) {
      restore_rect = *prop_rect;
      has_restore_rect = true;
    }
  }

  platform::SetWindowCloaked(window, false);
  RestoreWindowTransparency(window);

  ClearGenieWindowProperties(window);

  const bool still_iconic = IsIconic(window) != FALSE;

  if (still_iconic) {
    if (force_show_if_iconic) {
      SetPropW(window, kAllowRestoreProperty, reinterpret_cast<HANDLE>(1));
      if (was_maximized) {
        ShowWindow(window, SW_SHOWMAXIMIZED);
      } else {
        ShowWindow(window, SW_RESTORE);
      }
      RemovePropW(window, kAllowRestoreProperty);
    }
  } else {
    SetPropW(window, kAllowRestoreProperty, reinterpret_cast<HANDLE>(1));
    if (was_maximized) {
      ShowWindow(window, SW_SHOWMAXIMIZED);
    } else {
      ShowWindow(window, SW_RESTORE);
    }
    RemovePropW(window, kAllowRestoreProperty);
  }

  in_restore_window_state_ = false;
}

void Application::CleanupAndRestoreAll() {
  static std::atomic<bool> cleaned_up{false};
  if (cleaned_up.exchange(true)) {
    return;
  }

  // Signal the main loop and all event handlers to stop immediately.
  shutting_down_.store(true, std::memory_order_release);

  LogDebug(L"App", L"CleanupAndRestoreAll starting");
  UnregisterAllHotkeys();
  window_event_monitor_.Stop();
  UninstallCbtHook();
  native_animation_blocker_.Disable();

  // Post WM_QUIT so the main message loop exits if it's still running.
  if (GetOverlayWindow() != nullptr) {
    PostMessageW(GetOverlayWindow(), WM_CLOSE, 0, 0);
  }

  // Take ownership of the maps so the main thread can't see them anymore.
  auto restore_copy = std::move(restore_snapshots_);
  auto pre_minimize_copy = std::move(pre_minimize_snapshots_);

  HWND animating_copies[2];
  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    animating_copies[i] = runs_[i].animating_window;
    runs_[i].animating_window = nullptr;
    runs_[i].animating_restore = false;
    runs_[i].pending_native_minimize_window = nullptr;
  }

  // Enumerate and heal all windows in the system
  EnumWindows(
      [](HWND hwnd, LPARAM) -> BOOL {
        RemovePropW(hwnd, kExcludedApplicationProperty);
        if (HasGenieWindowState(hwnd)) {
          // Inline restore: don't use RestoreWindowFromGenieState to avoid
          // setting in_restore_window_state_ which can race with the main thread.
          platform::SetWindowCloaked(hwnd, false);
          RestoreWindowTransparency(hwnd);
          platform::SetOwnedWindowRegion(hwnd, nullptr, true);
          ClearGenieWindowProperties(hwnd);
        }
        return TRUE;
      },
      0);

  // Also restore any tracked windows from our maps
  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    if (animating_copies[i] != nullptr && IsWindow(animating_copies[i])) {
      RestoreWindowFromGenieState(animating_copies[i]);
    }
  }
  for (const auto& [hwnd, snapshot] : restore_copy) {
    RestoreWindowFromGenieState(hwnd);
  }
  for (const auto& [hwnd, snapshot] : pre_minimize_copy) {
    RestoreWindowFromGenieState(hwnd);
  }

  for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
    runs_[i].overlay.Shutdown();
  }
  settings_window_.Shutdown();
  restore_copy.clear();
  pre_minimize_copy.clear();
  desktop_capture_.reset();
  d3d_device_.reset();
  EndFallbackTimerResolution();
  if (animation_frame_timer_ != nullptr) {
    // Cleanup may run on the console-control thread while the main thread is
    // waiting on this handle. Signal it here; the owning Application
    // destructor closes it only after Run has left the wait.
    LARGE_INTEGER wake_now{};
    SetWaitableTimer(animation_frame_timer_, &wake_now, 0, nullptr, nullptr, FALSE);
  }
  if (session_started_) {
    if (!WriteSessionState("clean")) {
      LogDebug(L"SafeMode", L"Failed to write the clean session marker");
    }
    session_started_ = false;
  }
  LogDebug(L"App", L"CleanupAndRestoreAll completed");
}

void Application::HealLeftoverWindows() {
  LogDebug(L"App", L"HealLeftoverWindows checking for leftover Genie windows");
  std::size_t repaired_count = 0;
  std::pair<Application*, std::size_t*> repair_context{this, &repaired_count};
  EnumWindows(
      [](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* context = reinterpret_cast<std::pair<Application*, std::size_t*>*>(lParam);
        RemovePropW(hwnd, kExcludedApplicationProperty);
        if (HasGenieWindowState(hwnd)) {
          LogDebug(L"App", L"HealLeftoverWindows: restoring leftover window hwnd=0x" +
                               std::to_wstring(reinterpret_cast<std::uintptr_t>(hwnd)));
          context->first->RestoreWindowFromGenieState(hwnd, false);
          ++*context->second;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&repair_context));
  startup_repair_status_ = repaired_count == 0
                               ? "No issues found"
                               : std::to_string(repaired_count) + " window(s) repaired";
  LogDebug(L"App", L"Startup repair result: " + std::to_wstring(repaired_count) +
                       L" suspicious window(s) repaired");
}

}  // namespace genie::app
