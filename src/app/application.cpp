#include "pch.hpp"

#include "app/application.hpp"

#include <atomic>
#include <dwmapi.h>
#include <iostream>
#include <sstream>
#include <string>
#include <timeapi.h>

#include "animation/geometry.hpp"
#include "common/debug_log.hpp"
#include "platform/window_util.hpp"

#pragma comment(lib, "winmm.lib")

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
constexpr std::size_t kMaxPreMinimizeSnapshots = 4;
constexpr DWORD kInitialRendererRecoveryDelayMs = 250;
constexpr DWORD kMaximumRendererRecoveryDelayMs = 4000;

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

Application::~Application() {
  CleanupAndRestoreAll();
  if (animation_frame_timer_ != nullptr) {
    CloseHandle(animation_frame_timer_);
    animation_frame_timer_ = nullptr;
  }
}

bool Application::Initialize(HINSTANCE instance) {
  instance_ = instance;
  main_thread_id_ = GetCurrentThreadId();
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

  if (!CreateAnimationRenderer()) return false;

  if (!settings_window_.Initialize(
          instance, [this](bool enabled) { SetEnabled(enabled); },
          [this](float min_dur, float rest_dur) { SetAnimationDurations(min_dur, rest_dur); },
          [this]() { HealLeftoverWindows(); }, [this]() { RequestShutdown(); })) {
    return false;
  }
  settings_window_.UpdateState(is_enabled_, minimize_duration_seconds_, restore_duration_seconds_);
  settings_window_.Show(true);

  native_animation_blocker_.Enable(slots_[0].overlay.window());
  const bool cbt_hook_installed = InstallCbtHook();
  if (cbt_hook_installed) {
    std::wcout << L"Global CBT hook installed.\n";
  } else {
    std::wcerr << L"Global CBT hook unavailable; using WinEvent fallback.\n";
  }

  if (!window_event_monitor_.Start(
          [this](HWND window) { OnMinimizeStart(window); },
          [this](HWND window) { OnRestoreAttempt(window); },
          [this](HWND window, DWORD event) { OnWindowSeen(window, event); })) {
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

int Application::FindSlotForWindow(HWND window) const {
  for (int i = 0; i < 2; ++i) {
    if (slots_[i].animating_window == window) {
      return i;
    }
  }
  return -1;
}

int Application::FindFreeSlot() const {
  for (int i = 0; i < 2; ++i) {
    if (slots_[i].animating_window == nullptr && !slots_[i].overlay.active()) {
      return i;
    }
  }
  return -1;
}

bool Application::CreateAnimationRenderer() {
  for (int i = 0; i < 2; ++i) {
    slots_[i].overlay.Shutdown();
  }
  desktop_capture_.reset();
  d3d_device_.reset();

  d3d_device_ = rendering::D3dDevice::Create();
  if (d3d_device_ == nullptr) {
    return false;
  }
  for (int i = 0; i < 2; ++i) {
    if (!slots_[i].overlay.Initialize(
            instance_, d3d_device_.get(), [this](HWND window) { return OnMinimizeStart(window); },
            [this](HWND window) { return OnRestoreAttempt(window); })) {
      for (int j = 0; j <= i; ++j) {
        slots_[j].overlay.Shutdown();
      }
      d3d_device_.reset();
      return false;
    }
    slots_[i].overlay.SetAnimationDuration(minimize_duration_seconds_);
  }
  desktop_capture_ = std::make_unique<rendering::DesktopCapture>(d3d_device_.get());
  return true;
}

bool Application::AnimationRendererDeviceLost() const {
  return slots_[0].overlay.device_lost() ||
         slots_[1].overlay.device_lost() ||
         (desktop_capture_ != nullptr && desktop_capture_->device_lost()) ||
         (d3d_device_ != nullptr && d3d_device_->IsDeviceLost());
}

void Application::BeginAnimationRendererRecovery() {
  if (animation_renderer_recovery_pending_) {
    return;
  }

  LogDebug(L"App", L"Animation renderer device lost; rebuilding D3D resources");
  native_animation_blocker_.Disable();

  for (int i = 0; i < 2; ++i) {
    auto& slot = slots_[i];
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
    if (is_enabled_) {
      native_animation_blocker_.Enable(slots_[0].overlay.window());
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

    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      if (message.message == WM_QUIT) {
        running = false;
        break;
      }
      if (message.message == WM_DISPLAYCHANGE) {
        for (int i = 0; i < 2; ++i) {
          slots_[i].animation_monitor = nullptr;
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

    for (int i = 0; i < 2; ++i) {
      auto& slot = slots_[i];
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
            if (slot.pending_native_minimize_window == slot.animating_window) {
              slot.pending_native_minimize_window = nullptr;
            }
            std::wcout << L"Target is minimized, starting animation clock.\n";
          } else {
            const ULONGLONG now = GetTickCount64();
            if (now - slot.minimize_start_time_ms >= 800) {
              HWND stalled_window = slot.animating_window;
              TraceWindowEvent(L"Run minimize timeout aborting stalled animation", stalled_window);
              std::wcerr << L"Genie minimize event timeout before native minimize completed; aborting animation.\n";
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
          if (now_ms - slot.last_animation_texture_refresh_ms >= 16) {
            slot.last_animation_texture_refresh_ms = now_ms;
            desktop_capture_->RefreshCapturedTexture(
                slot.live_animation_bounds, slot.overlay.mutable_captured_texture());
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
      }
    }

    bool any_active = false;
    for (int i = 0; i < 2; ++i) {
      if (slots_[i].overlay.active()) {
        any_active = true;
      }
    }

    const ULONGLONG now_ms = GetTickCount64();
    if (is_enabled_ && now_ms - last_snapshot_refresh_ms_ >= 120) {
      last_snapshot_refresh_ms_ = now_ms;
      UpdatePreMinimizeSnapshot(GetForegroundWindow());
    }

    if (is_enabled_ && FindFreeSlot() != -1) {
      for (auto& [hwnd, snapshot] : restore_snapshots_) {
        (void)snapshot;
        if (FindSlotForWindow(hwnd) != -1) {
          continue;
        }
        if (IsWindow(hwnd) && IsWindowVisible(hwnd) && IsGenieWindowRestored(hwnd)) {
          std::wcout << L"Poll: detected restore for hwnd=0x" << std::hex
                     << reinterpret_cast<std::uintptr_t>(hwnd) << std::dec << std::endl;
          OnRestoreAttempt(hwnd);
          break; // Only handle one at a time
        }
      }
    }

    any_active = false;
    for (int i = 0; i < 2; ++i) {
      if (slots_[i].overlay.active()) {
        any_active = true;
      }
    }

    if (!any_active) {
      EndFallbackTimerResolution();
    }

    if (any_active) {
      WaitForAnimationFrameOrMessage();
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
  if (main_thread_id_ != 0) {
    PostThreadMessageW(main_thread_id_, WM_QUIT, 0, 0);
  }
}

void Application::ResetAnimationFramePacing(int slot_index, HWND window, const RECT& animation_bounds) {
  auto& slot = slots_[slot_index];
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
  UpdateAnimationFramePacingMonitor(slot_index);
}

void Application::UpdateAnimationFramePacingMonitor(int slot_index) {
  auto& slot = slots_[slot_index];
  RECT monitor_bounds = slot.live_animation_bounds;
  if (slot.animating_window != nullptr && IsWindow(slot.animating_window) &&
      IsIconic(slot.animating_window) == FALSE) {
    const std::optional<RECT> current_bounds = platform::GetExtendedFrameBounds(slot.animating_window);
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

bool Application::IsAnimationFrameDue(int slot_index) const {
  const auto& slot = slots_[slot_index];
  return slot.animation_frame_interval <= std::chrono::steady_clock::duration::zero() ||
         std::chrono::steady_clock::now() >= slot.next_animation_frame_time;
}

void Application::AdvanceAnimationFrameDeadline(int slot_index) {
  auto& slot = slots_[slot_index];
  if (slot.animation_frame_interval <= std::chrono::steady_clock::duration::zero()) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (slot.next_animation_frame_time == std::chrono::steady_clock::time_point{}) {
    slot.next_animation_frame_time = now + slot.animation_frame_interval;
    return;
  }

  if (slot.next_animation_frame_time <= now) {
    const auto missed_intervals = (now - slot.next_animation_frame_time) / slot.animation_frame_interval;
    slot.next_animation_frame_time += slot.animation_frame_interval * (missed_intervals + 1);
  }
}

void Application::WaitForAnimationFrameOrMessage() {
  bool has_valid_interval = false;
  auto earliest_next = std::chrono::steady_clock::time_point::max();

  for (int i = 0; i < 2; ++i) {
    auto& slot = slots_[i];
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

void Application::SetEnabled(bool enabled) {
  if (is_enabled_ == enabled) {
    settings_window_.UpdateState(enabled, minimize_duration_seconds_, restore_duration_seconds_);
    return;
  }

  if (!enabled) {
    for (int i = 0; i < 2; ++i) {
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
    if (desktop_capture_ != nullptr) {
      desktop_capture_->ClearHistory();
    }
  } else {
    InstallCbtHook();
    if (!animation_renderer_recovery_pending_ && slots_[0].overlay.window() != nullptr) {
      native_animation_blocker_.Enable(slots_[0].overlay.window());
    }
  }

  is_enabled_ = enabled;
  settings_window_.UpdateState(enabled, minimize_duration_seconds_, restore_duration_seconds_);
}

void Application::SetAnimationDurations(float minimize_duration, float restore_duration) {
  minimize_duration_seconds_ = std::clamp(minimize_duration, 0.10f, 2.00f);
  restore_duration_seconds_ = std::clamp(restore_duration, 0.10f, 2.00f);
  for (int i = 0; i < 2; ++i) {
    if (slots_[i].overlay.active()) {
      if (slots_[i].animating_restore) {
        slots_[i].overlay.SetAnimationDuration(restore_duration_seconds_);
      } else {
        slots_[i].overlay.SetAnimationDuration(minimize_duration_seconds_);
      }
    }
  }
  settings_window_.UpdateState(is_enabled_, minimize_duration_seconds_, restore_duration_seconds_);
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
  if (shutting_down_.load(std::memory_order_acquire) || !is_enabled_ ||
      animation_renderer_recovery_pending_ || desktop_capture_ == nullptr ||
      slots_[0].overlay.window() == nullptr) {
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

  if (window == slots_[0].overlay.window() || window == slots_[1].overlay.window() || !IsWindow(window)) {
    LogDebug(L"App", L"OnMinimizeStart: Ignored because window is overlay or invalid");
    return false;
  }

  if (!platform::IsInterestingTopLevelWindow(window, slots_[0].overlay.window())) {
    LogDebug(L"App", L"OnMinimizeStart: Ignored because window is not interesting");
    return false;
  }

  int slot_index = FindSlotForWindow(window);
  if (slot_index != -1) {
    auto& slot = slots_[slot_index];
    if (slot.pending_native_minimize_window == window ||
        GetPropW(window, kAllowMinimizeProperty) != nullptr) {
      LogDebug(L"App", L"OnMinimizeStart: Allow minimize because already pending/allowed");
      return true;
    }
    slot.animating_restore = false;
    slot.overlay.ContinueMinimizeAnimation();
    slot.live_animation_capture_enabled = false;
    std::wcout << L"Minimize requested during active animation; continuing "
                  L"toward taskbar.\n";
    LogDebug(L"App", L"OnMinimizeStart: Minimize during active animation, continuing to taskbar");
    return true;
  }

  slot_index = FindFreeSlot();
  if (slot_index == -1) {
    // Both slots occupied, fall back to native minimize behavior
    LogDebug(L"App", L"OnMinimizeStart: Both slots occupied; fallback to native minimize");
    return false;
  }

  auto& slot = slots_[slot_index];

  if (restore_snapshots_.count(window) > 0 || GetPropW(window, kIsMinimizingProperty) != nullptr) {
    return true;
  }

  std::wcout << L"Minimize detected: hwnd=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(window) << std::dec << L"\n";

  struct TopmostRestorer {
    HWND wnd;
    bool active;
    ~TopmostRestorer() {
      if (wnd != nullptr && IsWindow(wnd) && !active) {
        SetWindowPos(wnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      }
    }
  } restorer{window, BringWindowForwardForCapture(window)};

  desktop_capture_->ClearHistory();

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
  snapshot.process_id = WindowProcessId(window);
  snapshot.captured_at_ms = GetTickCount64();

  restore_snapshots_[window] = std::move(snapshot);

  slot.animating_window = window;
  slot.animating_restore = false;
  slot.live_animation_bounds = source_bounds;
  ResetAnimationFramePacing(slot_index, window, source_bounds);
  slot.last_animation_texture_refresh_ms = 0;
  slot.live_animation_capture_enabled = false;

  slot.overlay.SetAnimationDuration(minimize_duration_seconds_);
  if (!slot.overlay.StartAnimation(captured_texture, ToRectF(source_bounds), target.rect,
                                      target.edge)) {
    TraceWindowEvent(L"OnMinimizeStart failed: overlay StartAnimation", window);
    std::wcerr << L"Genie animation did not start because overlay start failed.\n";
    restore_snapshots_.erase(window);
    slot.animating_window = nullptr;
    slot.animating_restore = false;
    slot.live_animation_capture_enabled = false;
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
      return false;
    }
    slot.pending_native_minimize_window = window;
  } else {
    slot.pending_native_minimize_window = nullptr;
    slot.overlay.StartAnimationClock();
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

void Application::FinishActiveAnimation(int slot_index) {
  auto& slot = slots_[slot_index];
  HWND finished_window = slot.animating_window;
  if (finished_window == nullptr) {
    return;
  }

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
  for (int i = 0; i < 2; ++i) {
    if (slots_[i].overlay.active()) {
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
}

void Application::CompletePendingNativeMinimize(int slot_index) {
  auto& slot = slots_[slot_index];
  HWND window = slot.pending_native_minimize_window;
  TraceWindowEvent(L"CompletePendingNativeMinimize begin", window);

  auto abort_pending_minimize = [this, slot_index, window]() {
    TraceWindowEvent(L"CompletePendingNativeMinimize abort", window);
    if (window != nullptr && IsWindow(window)) {
      platform::SetWindowCloaked(window, false);
      RestoreWindowTransparency(window);
      platform::SetOwnedWindowRegion(window, nullptr, true);
      ClearGenieWindowProperties(window);
    }
    slots_[slot_index].live_animation_capture_enabled = false;
    if (slots_[slot_index].overlay.active() && !slots_[slot_index].overlay.restoring()) {
      slots_[slot_index].overlay.CancelAnimation();
    }
    if (slots_[slot_index].animating_window == window) {
      restore_snapshots_.erase(window);
      slots_[slot_index].animating_window = nullptr;
      slots_[slot_index].animating_restore = false;
    }
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
    std::wcerr << L"Could not read minimized window placement; canceling Genie minimize animation.\n";
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
  if (!is_enabled_ || animation_renderer_recovery_pending_ || slots_[0].overlay.window() == nullptr) {
    return false;
  }
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
                       std::to_wstring(is_moved_offscreen));

  if (in_restore_window_state_) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because in_restore_window_state_ is true");
    return false;
  }

  if (window == slots_[0].overlay.window() || window == slots_[1].overlay.window() || !IsWindow(window)) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because window is overlay or invalid");
    return false;
  }

  if (!platform::IsInterestingTopLevelWindow(window, slots_[0].overlay.window())) {
    LogDebug(L"App", L"OnRestoreAttempt: Ignored because window is not interesting");
    return false;
  }

  std::wcout << L"OnRestoreAttempt: hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(window)
             << std::dec << L" iconic=" << (IsIconic(window) != FALSE) << L" genie_minimized="
             << window_was_genie_minimized << L" offscreen=" << is_moved_offscreen << L"\n";

  int slot_index = FindSlotForWindow(window);
  if (slot_index != -1) {
    auto& slot = slots_[slot_index];
    if (slot.pending_native_minimize_window == window) {
      slot.pending_native_minimize_window = nullptr;
    }
    slot.animating_restore = true;
    slot.overlay.ReverseAnimation();
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

  slot_index = FindFreeSlot();
  if (slot_index == -1) {
    // Both slots occupied, fall back to native restore behavior
    LogDebug(L"App", L"OnRestoreAttempt: Both slots occupied; fallback to native restore");
    RestoreWindowFromGenieState(window, false);
    restore_snapshots_.erase(window);
    return false;
  }

  auto& slot = slots_[slot_index];
  slot.animating_window = window;
  slot.animating_restore = true;
  slot.live_animation_capture_enabled = false;
  ResetAnimationFramePacing(slot_index, window, current_snapshot.bounds);

  native_animation_blocker_.SetTransitionsDisabledForWindow(window, true);

  slot.overlay.SetAnimationDuration(restore_duration_seconds_);
  if (!slot.overlay.StartAnimation(current_snapshot.texture, ToRectF(current_snapshot.bounds),
                                      current_snapshot.target.rect, current_snapshot.target.edge,
                                      1.0f, 0.0f)) {
    TraceWindowEvent(L"OnRestoreAttempt failed: overlay StartAnimation", window);
    std::wcerr << L"Restore animation did not start because overlay start failed.\n";
    RestoreWindowFromGenieState(window, true);
    restore_snapshots_.erase(window);
    slot.animating_window = nullptr;
    slot.animating_restore = false;
    return false;
  }

  TraceWindowEvent(L"OnRestoreAttempt before StartAnimationClock", window);
  slot.overlay.StartAnimationClock();

  std::wcout << L"Restore animation started.\n";
  return true;
}

void Application::OnWindowSeen(HWND window, DWORD event) {
  if (shutting_down_.load(std::memory_order_acquire) || !is_enabled_ ||
      animation_renderer_recovery_pending_) {
    return;
  }
  if (window == slots_[0].overlay.window() || window == slots_[1].overlay.window()) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    if (window == slots_[i].animating_window) {
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
      window == slots_[0].overlay.window() || window == slots_[1].overlay.window() ||
      !IsWindow(window) || IsIconic(window) || !IsWindowVisible(window)) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    if (window == slots_[i].animating_window) {
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
  window_event_monitor_.Stop();
  UninstallCbtHook();
  native_animation_blocker_.Disable();

  // Post WM_QUIT so the main message loop exits if it's still running.
  if (slots_[0].overlay.window() != nullptr) {
    PostMessageW(slots_[0].overlay.window(), WM_CLOSE, 0, 0);
  }

  // Take ownership of the maps so the main thread can't see them anymore.
  auto restore_copy = std::move(restore_snapshots_);
  auto pre_minimize_copy = std::move(pre_minimize_snapshots_);
  
  HWND animating_copies[2];
  for (int i = 0; i < 2; ++i) {
    animating_copies[i] = slots_[i].animating_window;
    slots_[i].animating_window = nullptr;
    slots_[i].animating_restore = false;
    slots_[i].pending_native_minimize_window = nullptr;
  }

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
  for (int i = 0; i < 2; ++i) {
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

  for (int i = 0; i < 2; ++i) {
    slots_[i].overlay.Shutdown();
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
