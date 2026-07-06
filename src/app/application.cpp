#include "pch.hpp"

#include "app/application.hpp"

#include <atomic>
#include <dwmapi.h>
#include <iostream>
#include <sstream>
#include <string>

#include "animation/geometry.hpp"
#include "common/debug_log.hpp"
#include "platform/window_util.hpp"

namespace genie::app {
namespace {

constexpr wchar_t kHookDllName[] = L"GenieHookPost.dll";
constexpr char kCbtProcName[] = "CBTProc";
constexpr char kDecoratedCbtProcName[] = "_CBTProc@12";
constexpr wchar_t kAllowMinimizeProperty[] = L"GenieAllowMinimize";
constexpr wchar_t kAllowRestoreProperty[] = L"GenieAllowRestore";
constexpr wchar_t kIsMinimizingProperty[] = L"GenieIsMinimizing";
constexpr wchar_t kOriginalPlacementLeftProperty[] = L"GenieOriginalPlacementLeft";
constexpr wchar_t kOriginalPlacementTopProperty[] = L"GenieOriginalPlacementTop";
constexpr wchar_t kOriginalPlacementRightProperty[] = L"GenieOriginalPlacementRight";
constexpr wchar_t kOriginalPlacementBottomProperty[] = L"GenieOriginalPlacementBottom";
constexpr wchar_t kMovedOffscreenProperty[] = L"GenieMovedOffscreen";
constexpr wchar_t kWasMaximizedProperty[] = L"GenieWasMaximized";
constexpr wchar_t kOriginalExStyleProperty[] = L"GenieOriginalExStyle";
constexpr wchar_t kWasLayeredProperty[] = L"GenieWasLayered";
constexpr wchar_t kOriginalAlphaProperty[] = L"GenieOriginalAlpha";
constexpr wchar_t kOriginalFlagsProperty[] = L"GenieOriginalFlags";
constexpr ULONGLONG kDesktopRefreshIntervalMs = 16;
constexpr ULONGLONG kAnimationFrameIntervalMs = 8;

using CbtProc = LRESULT(CALLBACK*)(int, WPARAM, LPARAM);

void MakeWindowTransparent(HWND window);

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
         GetPropW(window, kOriginalExStyleProperty) != nullptr ||
         GetPropW(window, kOriginalPlacementLeftProperty) != nullptr;
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

void BringWindowForwardForCapture(HWND window) {
  if (!IsWindow(window) || IsIconic(window) != FALSE) {
    return;
  }

  TraceWindowEvent(L"BringWindowForwardForCapture begin", window);
  const BOOL foreground_ok = SetForegroundWindow(window);
  const BOOL top_ok =
      SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  BringWindowToTop(window);
  DwmFlush();
  LogTrace(L"App", L"BringWindowForwardForCapture foreground_ok=" +
                       std::to_wstring(foreground_ok != FALSE) + L" top_ok=" +
                       std::to_wstring(top_ok != FALSE) + L" window " + WindowTraceString(window));
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
  if (GetPropW(window, kOriginalExStyleProperty) != nullptr) {
    TraceWindowEvent(L"MakeWindowTransparent skipped: already transparent", window);
    return;
  }

  TraceWindowEvent(L"MakeWindowTransparent begin", window);
  LONG_PTR ex_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
  SetPropW(window, kOriginalExStyleProperty, reinterpret_cast<HANDLE>(ex_style));

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
  HANDLE ex_style_prop = GetPropW(window, kOriginalExStyleProperty);
  if (ex_style_prop == nullptr) {
    TraceWindowEvent(L"RestoreWindowTransparency skipped: no original style", window);
    return;
  }

  TraceWindowEvent(L"RestoreWindowTransparency begin", window);
  LONG_PTR original_ex_style = reinterpret_cast<LONG_PTR>(ex_style_prop);
  BOOL was_layered = GetPropW(window, kWasLayeredProperty) != nullptr &&
                     reinterpret_cast<INT_PTR>(GetPropW(window, kWasLayeredProperty)) != 0;
  BYTE alpha =
      static_cast<BYTE>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalAlphaProperty)));
  DWORD flags =
      static_cast<DWORD>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalFlagsProperty)));

  if (was_layered) {
    SetLayeredWindowAttributes(window, 0, alpha, flags);
    SetWindowLongPtrW(window, GWL_EXSTYLE, original_ex_style);
  } else {
    SetWindowLongPtrW(window, GWL_EXSTYLE, original_ex_style & ~WS_EX_LAYERED);
  }

  RemovePropW(window, kOriginalExStyleProperty);
  RemovePropW(window, kWasLayeredProperty);
  RemovePropW(window, kOriginalAlphaProperty);
  RemovePropW(window, kOriginalFlagsProperty);
  TraceWindowEvent(L"RestoreWindowTransparency end", window);
}

}  // namespace

Application::~Application() { CleanupAndRestoreAll(); }

bool Application::Initialize(HINSTANCE instance) {
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

  d3d_device_ = rendering::D3dDevice::Create();
  if (d3d_device_ == nullptr) {
    return false;
  }

  if (!overlay_window_.Initialize(
          instance, d3d_device_.get(), [this](HWND window) { return OnMinimizeStart(window); },
          [this](HWND window) { return OnRestoreAttempt(window); })) {
    return false;
  }

  desktop_capture_ = std::make_unique<rendering::DesktopCapture>(d3d_device_.get());
  desktop_capture_->RefreshFrames(120);
  native_animation_blocker_.Enable(overlay_window_.window());
  const bool cbt_hook_installed = InstallCbtHook();
  if (cbt_hook_installed) {
    std::wcout << L"Global CBT hook installed.\n";
  } else {
    std::wcerr << L"Global CBT hook unavailable; using WinEvent fallback.\n";
  }

  if (!window_event_monitor_.Start([this](HWND window) { OnMinimizeStart(window); },
                                   [this](HWND window) { OnRestoreAttempt(window); },
                                   [this](HWND window) { OnWindowSeen(window); })) {
    return false;
  }

  std::wcout << L"Genie minimize monitor is running.\n";
  LogTrace(L"App", L"Application::Initialize completed");
  std::wcout << L"Set GENIE_TASKBAR_RECT=left,top,right,bottom to aim at a "
                L"custom taskbar rectangle.\n";
  std::wcout << L"Close this console window to restore the previous Windows "
                L"animation setting.\n";
  return true;
}

int Application::Run() {
  MSG message{};
  bool running = true;

  while (running) {
    if (shutting_down_.load(std::memory_order_acquire)) {
      break;
    }

    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      if (message.message == WM_QUIT) {
        running = false;
        break;
      }
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    if (!running || shutting_down_.load(std::memory_order_acquire)) {
      break;
    }

    if (pending_native_minimize_window_ != nullptr) {
      TraceWindowEvent(L"Run pending_native_minimize before CompletePendingNativeMinimize",
                       pending_native_minimize_window_);
      CompletePendingNativeMinimize();
      TraceWindowEvent(L"Run pending_native_minimize after CompletePendingNativeMinimize",
                       pending_native_minimize_window_);
    }

    const ULONGLONG loop_now_ms = GetTickCount64();
    if (!overlay_window_.active() &&
        loop_now_ms - last_desktop_refresh_ms_ >= kDesktopRefreshIntervalMs) {
      last_desktop_refresh_ms_ = loop_now_ms;
      desktop_capture_->RefreshFrames();
    }

    if (overlay_window_.active() && !overlay_window_.restoring() && animating_window_ != nullptr) {
      if (IsTraceLoggingEnabled()) {
        LogTrace(L"App", L"Run overlay active clock_started=" +
                             std::to_wstring(overlay_window_.clock_started()) + L" restoring=" +
                             std::to_wstring(animating_restore_) + L" pending=" +
                             std::to_wstring(pending_native_minimize_window_ != nullptr) +
                             L" window " + WindowTraceString(animating_window_));
      }
      if (!overlay_window_.clock_started()) {
        const bool is_iconic = IsIconic(animating_window_) != FALSE;
        const bool is_moved = GetPropW(animating_window_, kMovedOffscreenProperty) != nullptr;
        if (is_iconic || is_moved) {
          if (IsTraceLoggingEnabled()) {
            LogTrace(L"App", L"Run starting animation clock is_iconic=" +
                                 std::to_wstring(is_iconic) + L" moved_offscreen=" +
                                 std::to_wstring(is_moved) + L" window " +
                                 WindowTraceString(animating_window_));
          }
          overlay_window_.StartAnimationClock();
          if (pending_native_minimize_window_ == animating_window_) {
            pending_native_minimize_window_ = nullptr;
          }
          std::wcout << L"Target is minimized, starting animation clock.\n";
        } else {
          const ULONGLONG now = GetTickCount64();
          if (now - minimize_start_time_ms_ >= 500) {
            if (pending_native_minimize_window_ != nullptr) {
              LogTrace(L"App", L"Run minimize timeout reached; retrying pending native minimize");
              CompletePendingNativeMinimize();
            } else {
              HWND stalled_window = animating_window_;
              TraceWindowEvent(L"Run minimize timeout canceling stalled animation", stalled_window);
              std::wcerr << L"Genie minimize event timeout before native "
                            L"minimize completed; canceling animation.\n";
              if (IsWindow(stalled_window)) {
                ClearGenieWindowProperties(stalled_window);
                native_animation_blocker_.SetTransitionsDisabledForWindow(stalled_window, false);
              }
              overlay_window_.CancelAnimation();
              live_animation_capture_enabled_ = false;
              restore_snapshots_.erase(stalled_window);
              animating_window_ = nullptr;
              animating_restore_ = false;
            }
          }
        }
      }
    }

    const bool was_active = overlay_window_.active();
    const bool was_restoring = animating_restore_;
    if (was_active && live_animation_capture_enabled_) {
      if (animating_window_ == nullptr || !IsWindow(animating_window_) ||
          IsIconic(animating_window_) || !IsWindowVisible(animating_window_)) {
        live_animation_capture_enabled_ = false;
        std::wcout << L"Live texture refresh stopped because the target "
                      L"window is no longer visible.\n";
      } else {
        const ULONGLONG now_ms = GetTickCount64();
        if (now_ms - last_animation_texture_refresh_ms_ >= 16) {
          last_animation_texture_refresh_ms_ = now_ms;
          const bool refreshed = desktop_capture_->RefreshCapturedTexture(
              live_animation_bounds_, overlay_window_.mutable_captured_texture());
          if (IsTraceLoggingEnabled()) {
            LogTrace(L"App", L"Run live texture refresh refreshed=" + std::to_wstring(refreshed) +
                                 L" bounds=" + RectTraceString(live_animation_bounds_) +
                                 L" window " + WindowTraceString(animating_window_));
          }
          (void)refreshed;
        }
      }
    }

    bool animation_active = false;
    if (was_active) {
      const ULONGLONG now_ms = GetTickCount64();
      const bool should_tick = !overlay_window_.clock_started() || last_animation_tick_ms_ == 0 ||
                               now_ms - last_animation_tick_ms_ >= kAnimationFrameIntervalMs;
      if (should_tick) {
        last_animation_tick_ms_ = now_ms;
        animation_active = overlay_window_.Tick();
      } else {
        animation_active = true;
      }
    }
    if ((was_active || animation_active || animating_window_ != nullptr) &&
        IsTraceLoggingEnabled()) {
      LogTrace(L"App", L"Run after overlay Tick was_active=" + std::to_wstring(was_active) +
                           L" animation_active=" + std::to_wstring(animation_active) +
                           L" animating_restore=" + std::to_wstring(animating_restore_) +
                           L" window " + WindowTraceString(animating_window_));
    }
    if (was_active && !animation_active && animating_window_ != nullptr) {
      last_animation_tick_ms_ = 0;
      live_animation_capture_enabled_ = false;
      if (was_restoring) {
        TraceWindowEvent(L"Run restore animation completed before RestoreWindowFromGenieState",
                         animating_window_);
        RestoreWindowFromGenieState(animating_window_);
        DwmFlush();
        DwmFlush();
        overlay_window_.FinishRestoreAnimation();
        restore_snapshots_.erase(animating_window_);
        std::wcout << L"Restore animation completed.\n";
      } else {
        TraceWindowEvent(L"Run minimize animation completed before SetWindowRgn",
                         animating_window_);
        RemovePropW(animating_window_, kAllowMinimizeProperty);
        HRGN hidden_region = CreateRectRgn(0, 0, 0, 0);
        platform::SetOwnedWindowRegion(animating_window_, hidden_region, true);
        std::wcout << L"Minimize animation completed.\n";
      }
      animating_window_ = nullptr;
      animating_restore_ = false;
    }

    if (animation_active) {
      MsgWaitForMultipleObjects(0, nullptr, FALSE, 1, QS_ALLINPUT);
    } else {
      const ULONGLONG now_ms = GetTickCount64();
      if (now_ms - last_snapshot_refresh_ms_ >= 120) {
        last_snapshot_refresh_ms_ = now_ms;
        UpdatePreMinimizeSnapshot(GetForegroundWindow());
      }

      // Poll for "surprise restores": some external apps (Electron/Chromium)
      // don't fire EVENT_SYSTEM_MINIMIZEEND or any other WinEvent when
      // restored. Periodically check if any genie-minimized window is no
      // longer iconic and trigger the restore animation.
      if (animating_window_ == nullptr && !overlay_window_.active()) {
        for (auto& [hwnd, snapshot] : restore_snapshots_) {
          (void)snapshot;
          if (IsWindow(hwnd) && IsWindowVisible(hwnd) && IsGenieWindowRestored(hwnd)) {
            std::wcout << L"Poll: detected restore for hwnd=0x" << std::hex
                       << reinterpret_cast<std::uintptr_t>(hwnd) << std::dec << std::endl;
            OnRestoreAttempt(hwnd);
            break;  // Only handle one at a time
          }
        }
      }

      MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
    }
  }

  return static_cast<int>(message.wParam);
}

bool Application::InstallCbtHook() {
  if (cbt_hook_ != nullptr) {
    return true;
  }

  const std::wstring exec_dir = GetExecutableDirectory();
  const std::wstring hook_path = exec_dir + kHookDllName;

  // Programmatically grant AppContainer permissions
  platform::GrantAppContainerPermissions(exec_dir);
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
  if (shutting_down_.load(std::memory_order_acquire)) {
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

  if (window == overlay_window_.window() || !IsWindow(window)) {
    LogDebug(L"App", L"OnMinimizeStart: Ignored because window is overlay or invalid");
    return false;
  }

  if (!platform::IsInterestingTopLevelWindow(window, overlay_window_.window())) {
    LogDebug(L"App", L"OnMinimizeStart: Ignored because window is not interesting");
    return false;
  }

  if (overlay_window_.active() || animating_window_ != nullptr) {
    if (animating_window_ == window) {
      if (pending_native_minimize_window_ == window ||
          GetPropW(window, kAllowMinimizeProperty) != nullptr) {
        LogDebug(L"App", L"OnMinimizeStart: Allow minimize because already pending/allowed");
        return true;
      }
      animating_restore_ = false;
      overlay_window_.ContinueMinimizeAnimation();
      live_animation_capture_enabled_ = false;
      std::wcout << L"Minimize requested during active animation; continuing "
                    L"toward taskbar.\n";
      LogDebug(L"App", L"OnMinimizeStart: Minimize during active animation, continuing to taskbar");
      return true;
    }

    std::wcout << L"Ignoring minimize for background window during active "
                  L"Genie animation: hwnd=0x"
               << std::hex << reinterpret_cast<std::uintptr_t>(window) << std::dec << L"\n";
    LogDebug(L"App",
             L"OnMinimizeStart: Ignored minimize for background window during active animation");
    return false;
  }

  if (restore_snapshots_.count(window) > 0 || GetPropW(window, kIsMinimizingProperty) != nullptr) {
    return true;
  }

  std::wcout << L"Minimize detected: hwnd=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(window) << std::dec << L"\n";

  BringWindowForwardForCapture(window);

  const std::optional<RECT> animation_bounds = ResolveAnimationBounds(window);
  if (!animation_bounds.has_value()) {
    TraceWindowEvent(L"OnMinimizeStart failed: no animation bounds", window);
    std::wcerr << L"Minimized window bounds are unavailable for hwnd=0x" << std::hex
               << reinterpret_cast<std::uintptr_t>(window) << std::dec << L" class=\"" << class_name
               << L"\" title=\"" << title << L"\".\n";
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
  RECT source_bounds = *animation_bounds;
  const bool window_is_already_minimized = IsIconic(window) != FALSE;
  RECT captured_window_bounds{};

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
             desktop_capture_->CaptureWindow(window, *animation_bounds, &captured_texture,
                                             &captured_window_bounds)) {
    source_bounds = captured_window_bounds;
    std::wcout << L"Using live target-window capture.\n";
    LogTrace(L"App", L"OnMinimizeStart capture=window bounds=" + RectTraceString(source_bounds) +
                         L" texture_size=" + std::to_wstring(captured_texture.size.width) + L"x" +
                         std::to_wstring(captured_texture.size.height));
  } else if (!window_is_already_minimized &&
             desktop_capture_->CaptureRegion(*animation_bounds, &captured_texture)) {
    std::wcout << L"Using live desktop-region capture fallback.\n";
    LogTrace(L"App", L"OnMinimizeStart capture=desktop_fallback bounds=" +
                         RectTraceString(*animation_bounds) + L" texture_size=" +
                         std::to_wstring(captured_texture.size.width) + L"x" +
                         std::to_wstring(captured_texture.size.height));
  } else if (has_pre_minimize_snapshot) {
    source_bounds = pre_min_it->second.bounds;
    captured_texture = pre_min_it->second.texture;
    std::wcout << L"Using cached snapshot after live capture failed.\n";
    LogTrace(L"App", L"OnMinimizeStart capture=cached_after_live_failed bounds=" +
                         RectTraceString(source_bounds));
  }

  if (captured_texture.shader_resource_view == nullptr) {
    TraceWindowEvent(L"OnMinimizeStart failed: captured texture missing", window);
    std::wcerr << L"Could not capture window texture; iconic=" << window_is_already_minimized
               << L", cached=" << has_pre_minimize_snapshot << L".\n";
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

  PruneSnapshots();
  restore_snapshots_[window] = std::move(snapshot);

  animating_window_ = window;
  animating_restore_ = false;
  live_animation_bounds_ = source_bounds;
  last_animation_texture_refresh_ms_ = 0;
  live_animation_capture_enabled_ = false;

  if (!overlay_window_.StartAnimation(captured_texture, ToRectF(source_bounds), target.rect,
                                      target.edge)) {
    TraceWindowEvent(L"OnMinimizeStart failed: overlay StartAnimation", window);
    std::wcerr << L"Genie animation did not start because overlay start "
                  L"failed.\n";
    restore_snapshots_.erase(window);
    animating_window_ = nullptr;
    animating_restore_ = false;
    live_animation_capture_enabled_ = false;
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
  minimize_start_time_ms_ = GetTickCount64();
  pending_native_minimize_window_ = window;
  TraceWindowEvent(L"OnMinimizeStart completed pending native minimize", window);
  std::wcout << L"Genie overlay visible; native minimize scheduled.\n";
  return true;
}

void Application::CompletePendingNativeMinimize() {
  HWND window = pending_native_minimize_window_;
  TraceWindowEvent(L"CompletePendingNativeMinimize begin", window);

  auto abort_pending_minimize = [this, window]() {
    TraceWindowEvent(L"CompletePendingNativeMinimize abort", window);
    if (window != nullptr && IsWindow(window)) {
      ClearGenieWindowProperties(window);
    }
    live_animation_capture_enabled_ = false;
    if (overlay_window_.active() && !overlay_window_.restoring()) {
      overlay_window_.CancelAnimation();
    }
    if (animating_window_ == window) {
      restore_snapshots_.erase(window);
      animating_window_ = nullptr;
      animating_restore_ = false;
    }
  };

  if (window == nullptr || !IsWindow(window) || animating_window_ != window ||
      !overlay_window_.active() || overlay_window_.restoring()) {
    TraceWindowEvent(L"CompletePendingNativeMinimize early exit state mismatch", window);
    if (window != nullptr && animating_window_ == window && !overlay_window_.restoring()) {
      abort_pending_minimize();
    }
    pending_native_minimize_window_ = nullptr;
    return;
  }

  auto snap_it = restore_snapshots_.find(window);
  if (snap_it == restore_snapshots_.end()) {
    TraceWindowEvent(L"CompletePendingNativeMinimize abort: snapshot missing", window);
    abort_pending_minimize();
    pending_native_minimize_window_ = nullptr;
    return;
  }

  SetPropW(window, kAllowMinimizeProperty, reinterpret_cast<HANDLE>(1));
  SetPropW(window, kIsMinimizingProperty, reinterpret_cast<HANDLE>(1));

  if (IsIconic(window) == FALSE) {
    TraceWindowEvent(L"CompletePendingNativeMinimize before ShowWindow SW_MINIMIZE", window);
    ShowWindow(window, SW_MINIMIZE);
    TraceWindowEvent(L"CompletePendingNativeMinimize after ShowWindow SW_MINIMIZE", window);
  }
  DwmFlush();
  TraceWindowEvent(L"CompletePendingNativeMinimize after DwmFlush", window);

  if (IsIconic(window) == FALSE) {
    // Let the event loop retry or wait for it to become iconic on next ticks.
    TraceWindowEvent(L"CompletePendingNativeMinimize waiting: window not iconic yet", window);
    return;
  }

  pending_native_minimize_window_ = nullptr;
  live_animation_capture_enabled_ = false;

  WINDOWPLACEMENT wp{};
  wp.length = sizeof(wp);
  if (!GetWindowPlacement(window, &wp)) {
    TraceWindowEvent(L"CompletePendingNativeMinimize abort: GetWindowPlacement failed", window);
    std::wcerr << L"Could not read minimized window placement; canceling "
                  L"Genie minimize animation.\n";
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
  overlay_window_.StartAnimationClock();
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

  if (IsIconic(window) == FALSE) {
    return true;
  }

  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  if (GetWindowPlacement(window, &placement) && !IsMinimizedShowCommand(placement.showCmd)) {
    return true;
  }
  return ForegroundIsExactWindow(window, overlay_window_.window());
}

bool Application::OnRestoreAttempt(HWND window) {
  if (shutting_down_.load(std::memory_order_acquire)) {
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
                       std::to_wstring(is_moved_offscreen) + L" active=" +
                       std::to_wstring(overlay_window_.active()) + L" animating=" +
                       std::to_wstring(animating_window_ != nullptr));

  if (in_restore_window_state_) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because in_restore_window_state_ is true");
    return false;
  }

  if (window == overlay_window_.window() || !IsWindow(window)) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because window is overlay or invalid");
    return false;
  }

  if (!platform::IsInterestingTopLevelWindow(window, overlay_window_.window())) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because window is not interesting");
    return false;
  }

  std::wcout << L"OnRestoreAttempt: hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(window)
             << std::dec << L" iconic=" << (IsIconic(window) != FALSE) << L" genie_minimized="
             << window_was_genie_minimized << L" offscreen=" << is_moved_offscreen << L" active="
             << overlay_window_.active() << L" animating=" << (animating_window_ != nullptr)
             << L"\n";

  if (overlay_window_.active() || animating_window_ != nullptr) {
    if (animating_window_ == window) {
      if (pending_native_minimize_window_ == window) {
        pending_native_minimize_window_ = nullptr;
      }
      animating_restore_ = true;
      overlay_window_.ReverseAnimation();
      live_animation_capture_enabled_ = false;
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
            return true;
          }
        }
      }
      return true;
    }

    std::wcout << L"Restoring background window immediately during active "
                  L"Genie animation: hwnd=0x"
               << std::hex << reinterpret_cast<std::uintptr_t>(window) << std::dec << L"\n";
    RestoreWindowFromGenieState(window, false);
    restore_snapshots_.erase(window);
    return false;
  }

  const bool window_is_iconic = IsIconic(window) != FALSE;
  if (!window_was_genie_minimized && !window_is_iconic) {
    return false;
  }

  // For unofficial windows, the OS has already un-minimized the window by the
  // time this event fires. Immediately force it off-screen so the user never
  // sees a flash of the original window content. This must happen BEFORE any
  // animation setup to minimize the number of visible frames.
  if (!window_is_iconic && !is_moved_offscreen) {
    TraceWindowEvent(L"OnRestoreAttempt before pre-animation cloak transparent", window);
    platform::SetWindowCloaked(window, true);
    MakeWindowTransparent(window);
    TraceWindowEvent(L"OnRestoreAttempt after pre-animation cloak transparent", window);
    CachedSnapshot* snapshot = has_snapshot ? &snapshot_it->second : nullptr;
    if (!PreserveRestorePlacementAndMarkOffscreen(window, snapshot)) {
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
    std::wcerr << L"Could not restore with Genie animation; cached snapshot is "
                  L"missing.\n";
    RestoreWindowFromGenieState(window, false);
    return false;
  }
  const CachedSnapshot& current_snapshot = it->second;

  animating_window_ = window;
  animating_restore_ = true;
  live_animation_capture_enabled_ = false;

  native_animation_blocker_.SetTransitionsDisabledForWindow(window, true);

  if (!overlay_window_.StartAnimation(current_snapshot.texture, ToRectF(current_snapshot.bounds),
                                      current_snapshot.target.rect, current_snapshot.target.edge,
                                      1.0f, 0.0f)) {
    TraceWindowEvent(L"OnRestoreAttempt failed: overlay StartAnimation", window);
    std::wcerr << L"Restore animation did not start because overlay start "
                  L"failed.\n";
    animating_window_ = nullptr;
    animating_restore_ = false;
    return false;
  }

  TraceWindowEvent(L"OnRestoreAttempt before StartAnimationClock", window);
  overlay_window_.StartAnimationClock();

  std::wcout << L"Restore animation started.\n";
  return true;
}

void Application::OnWindowSeen(HWND window) {
  if (shutting_down_.load(std::memory_order_acquire)) {
    return;
  }
  if (window == overlay_window_.window() || window == animating_window_) {
    return;
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

  UpdatePreMinimizeSnapshot(window);
}

void Application::UpdatePreMinimizeSnapshot(HWND window) {
  if (window == overlay_window_.window() || !IsWindow(window) || IsIconic(window) ||
      !IsWindowVisible(window)) {
    return;
  }

  const std::optional<RECT> animation_bounds = ResolveAnimationBounds(window);
  if (!animation_bounds.has_value()) {
    TraceWindowEvent(L"UpdatePreMinimizeSnapshot skipped: no bounds", window);
    return;
  }

  rendering::CapturedTexture captured_texture;
  RECT captured_window_bounds{};
  RECT snapshot_bounds = *animation_bounds;
  if (desktop_capture_->CaptureWindow(window, *animation_bounds, &captured_texture,
                                      &captured_window_bounds)) {
    snapshot_bounds = captured_window_bounds;
  } else if (!desktop_capture_->CaptureRegion(*animation_bounds, &captured_texture)) {
    LogTrace(L"App", L"UpdatePreMinimizeSnapshot capture failed bounds=" +
                         RectTraceString(*animation_bounds) + L" window " +
                         WindowTraceString(window));
    return;
  }

  CachedSnapshot snapshot;
  snapshot.window = window;
  snapshot.bounds = snapshot_bounds;
  snapshot.texture = captured_texture;
  snapshot.target = taskbar_target_provider_.GetTargetForWindow(window, snapshot_bounds);
  const std::optional<WINDOWPLACEMENT> placement = GetPlacement(window);
  snapshot.original_placement =
      placement.has_value() ? placement->rcNormalPosition : *animation_bounds;
  if (!IsUsableRect(snapshot.original_placement)) {
    snapshot.original_placement = snapshot_bounds;
  }
  snapshot.was_maximized = IsCurrentlyMaximized(window);

  pre_minimize_snapshots_[window] = std::move(snapshot);
  LogTrace(L"App", L"UpdatePreMinimizeSnapshot stored bounds=" + RectTraceString(snapshot_bounds) +
                       L" texture_size=" + std::to_wstring(captured_texture.size.width) + L"x" +
                       std::to_wstring(captured_texture.size.height) + L" window " +
                       WindowTraceString(window));
}

void Application::PruneSnapshots() {
  for (auto it = restore_snapshots_.begin(); it != restore_snapshots_.end();) {
    if (!IsWindow(it->first)) {
      it = restore_snapshots_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = pre_minimize_snapshots_.begin(); it != pre_minimize_snapshots_.end();) {
    if (!IsWindow(it->first)) {
      it = pre_minimize_snapshots_.erase(it);
    } else {
      ++it;
    }
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

  const bool iconic_before_placement = IsIconic(window) != FALSE;
  if (has_restore_rect) {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(window, &placement)) {
      placement.rcNormalPosition = restore_rect;
      if (!was_maximized) {
        placement.flags &= ~WPF_RESTORETOMAXIMIZED;
        if (!iconic_before_placement || force_show_if_iconic) {
          placement.showCmd = SW_SHOWNORMAL;
        }
      }
      SetWindowPlacement(window, &placement);
      LogTrace(L"App", L"RestoreWindowFromGenieState applied restore_rect=" +
                           RectTraceString(restore_rect) + L" was_maximized=" +
                           std::to_wstring(was_maximized) + L" window " +
                           WindowTraceString(window));
    }
  }

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
  window_event_monitor_.Stop();
  UninstallCbtHook();
  native_animation_blocker_.Disable();

  // Post WM_QUIT so the main message loop exits if it's still running.
  if (overlay_window_.window() != nullptr) {
    PostMessageW(overlay_window_.window(), WM_CLOSE, 0, 0);
  }

  // Take ownership of the maps so the main thread can't see them anymore.
  auto restore_copy = std::move(restore_snapshots_);
  auto pre_minimize_copy = std::move(pre_minimize_snapshots_);
  HWND animating_copy = animating_window_;
  animating_window_ = nullptr;
  animating_restore_ = false;
  pending_native_minimize_window_ = nullptr;

  // Enumerate and heal all windows in the system
  EnumWindows(
      [](HWND hwnd, LPARAM) -> BOOL {
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
  if (animating_copy != nullptr && IsWindow(animating_copy)) {
    RestoreWindowFromGenieState(animating_copy);
  }
  for (const auto& [hwnd, snapshot] : restore_copy) {
    RestoreWindowFromGenieState(hwnd);
  }
  for (const auto& [hwnd, snapshot] : pre_minimize_copy) {
    RestoreWindowFromGenieState(hwnd);
  }

  overlay_window_.Shutdown();
  LogDebug(L"App", L"CleanupAndRestoreAll completed");
}

void Application::HealLeftoverWindows() {
  LogDebug(L"App", L"HealLeftoverWindows checking for leftover Genie windows");
  EnumWindows(
      [](HWND hwnd, LPARAM lParam) -> BOOL {
        if (HasGenieWindowState(hwnd)) {
          auto* app = reinterpret_cast<Application*>(lParam);
          LogDebug(L"App", L"HealLeftoverWindows: restoring leftover window hwnd=0x" +
                               std::to_wstring(reinterpret_cast<std::uintptr_t>(hwnd)));
          app->RestoreWindowFromGenieState(hwnd, false);
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(this));
}

}  // namespace genie::app
