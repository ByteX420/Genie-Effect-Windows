#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <windows.h>

#include "app/settings_window.hpp"
#include "app/settings_store.hpp"
#include "platform/native_animation_blocker.hpp"
#include "platform/taskbar_target_provider.hpp"
#include "platform/window_event_monitor.hpp"
#include "rendering/d3d_device.hpp"
#include "rendering/desktop_capture.hpp"
#include "rendering/overlay_window.hpp"

namespace genie::app {

class Application {
public:
  Application() = default;
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool Initialize(HINSTANCE instance);
  int Run();
  void RequestShutdown();
  void CleanupAndRestoreAll();
  void HealLeftoverWindows();

private:
  enum class RunState {
    kIdle,
    kCapturing,
    kWaitingForNativeMinimize,
    kAnimating,
    kRestoring,
    kAborting,
    kCleaningUp,
  };

  struct CachedSnapshot {
    HWND window = nullptr;
    RECT bounds{};
    rendering::CapturedTexture texture;
    platform::TaskbarTarget target{};
    bool was_maximized = false;
    bool moved_offscreen = false;
    RECT original_placement{};
    DWORD process_id = 0;
    ULONGLONG captured_at_ms = 0;
  };

  struct AnimationRun {
    rendering::OverlayWindow overlay;
    HWND animating_window = nullptr;
    HWND pending_native_minimize_window = nullptr;
    bool animating_restore = false;
    ULONGLONG minimize_start_time_ms = 0;
    RECT live_animation_bounds{};
    ULONGLONG last_animation_texture_refresh_ms = 0;
    HMONITOR animation_monitor = nullptr;
    std::chrono::steady_clock::duration animation_frame_interval{};
    std::chrono::steady_clock::time_point next_animation_frame_time{};
    bool live_animation_capture_enabled = false;
    RunState state = RunState::kIdle;
    ULONGLONG state_entered_ms = 0;
  };

  int FindRunForWindow(HWND window) const;
  [[nodiscard]] bool IsOverlayWindow(HWND window) const;
  int FindAvailableRun();
  bool InitializeRun(AnimationRun& slot);
  void SetRunState(int run_index, RunState state);
  void CheckAnimationTimeouts();
  [[nodiscard]] static const char* RunStateName(RunState state);

  bool OnMinimizeStart(HWND window);
  bool OnRestoreAttempt(HWND window);
  void OnWindowSeen(HWND window, DWORD event);
  void UpdatePreMinimizeSnapshot(HWND window);
  void CompletePendingNativeMinimize(int run_index);
  void FinishActiveAnimation(int run_index);
  void PruneSnapshots();
  bool PreserveRestorePlacementAndMarkOffscreen(HWND window, CachedSnapshot* snapshot);
  bool IsGenieWindowRestored(HWND window) const;
  void RestoreWindowFromGenieState(HWND window, bool force_show_if_iconic = true);
  bool SetEnabled(bool enabled);
  bool SetAnimationDurations(float minimize_duration, float restore_duration, bool save);
  bool SetLinkSpeeds(bool linked);
  bool SetAdaptiveDuration(bool enabled);
  bool SetDisableAnimationsFullscreen(bool enabled);
  bool SetPowerBehavior(bool reduce_on_battery, bool disable_in_saver);
  bool SetEasing(const std::string& minimize_easing, const std::string& restore_easing);
  bool SetAnimationStyle(const std::string& style);
  bool SetGenieStrength(float strength, bool save);
  bool SetFadeStrength(const std::string& strength);
  bool SetTargetIndicator(bool enabled);
  bool SetFollowWindowsAnimationPreference(bool enabled);
  bool SetCloseBehavior(const std::string& close_behavior);
  bool SetStartupOptions(bool run_at_startup, bool start_minimized);
  bool SetApplicationExcluded(const std::string& executable_name, bool excluded);
  void SetTemporaryPause(TemporaryPauseAction action);
  void UpdateTemporaryPause();
  void RegisterConfiguredHotkeys();
  void UnregisterAllHotkeys();
  HotkeyUpdateResult SetHotkey(HotkeyAction action, HotkeyBinding binding);
  void ExecuteHotkeyAction(HotkeyAction action);
  [[nodiscard]] DiagnosticsSnapshot BuildDiagnosticsSnapshot() const;
  bool ExecuteDiagnosticsAction(DiagnosticsAction action);
  [[nodiscard]] std::string ReadSessionState() const;
  bool WriteSessionState(std::string_view state) const;
  bool ExitSafeMode();
  [[nodiscard]] bool IsTemporarilyPaused() const;
  [[nodiscard]] bool IsEffectActive() const;
  [[nodiscard]] bool IsFullscreenApplicationActive() const;
  void UpdateFullscreenSuppression(bool force = false);
  void UpdatePowerState(bool force = false);
  void UpdateWindowsAnimationPreference(bool force = false);
  void EnableEffectRuntime();
  void DisableEffectRuntime();
  void RefreshEffectRuntimeState();
  [[nodiscard]] float CalculateAnimationDuration(float base_duration, const RECT& source,
                                                 const animation::RectF& target) const;
  [[nodiscard]] bool IsWindowExcluded(HWND window) const;
  [[nodiscard]] HWND GetOverlayWindow() const;
  void ApplyExclusionTransitionOverrides();
  bool InstallCbtHook();
  void UninstallCbtHook();
  void ResetAnimationFramePacing(int run_index, HWND window, const RECT& animation_bounds);
  void UpdateAnimationFramePacingMonitor(int run_index);
  [[nodiscard]] bool IsAnimationFrameDue(int run_index) const;
  void AdvanceAnimationFrameDeadline(int run_index);
  void WaitForAnimationFrameOrMessage();
  void BeginFallbackTimerResolution();
  void EndFallbackTimerResolution();
  bool CreateAnimationRenderer();
  void BeginAnimationRendererRecovery();
  bool TryRecoverAnimationRenderer();
  [[nodiscard]] bool AnimationRendererDeviceLost() const;

  std::unique_ptr<rendering::D3dDevice> d3d_device_;
  std::unique_ptr<rendering::DesktopCapture> desktop_capture_;
  std::deque<AnimationRun> runs_;
  platform::NativeAnimationBlocker native_animation_blocker_;
  platform::WindowEventMonitor window_event_monitor_;
  platform::TaskbarTargetProvider taskbar_target_provider_;
  HINSTANCE instance_ = nullptr;
  DWORD main_thread_id_ = 0;
  HMODULE hook_dll_ = nullptr;
  HHOOK cbt_hook_ = nullptr;
  std::unordered_map<HWND, CachedSnapshot> pre_minimize_snapshots_;
  std::unordered_map<HWND, CachedSnapshot> restore_snapshots_;
  ULONGLONG last_snapshot_refresh_ms_ = 0;
  HANDLE animation_frame_timer_ = nullptr;
  bool animation_frame_timer_is_high_resolution_ = false;
  bool fallback_timer_resolution_active_ = false;
  UINT fallback_timer_period_ms_ = 0;
  bool animation_renderer_recovery_pending_ = false;
  ULONGLONG next_animation_renderer_recovery_ms_ = 0;
  DWORD animation_renderer_recovery_delay_ms_ = 0;
  bool in_restore_window_state_ = false;
  HWND last_foreground_window_ = nullptr;
  bool is_enabled_ = true;
  bool effect_runtime_active_ = false;
  bool paused_until_restart_ = false;
  ULONGLONG paused_until_tick_ms_ = 0;
  bool fullscreen_suppressed_ = false;
  ULONGLONG last_fullscreen_check_ms_ = 0;
  bool running_on_battery_ = false;
  bool battery_saver_active_ = false;
  bool battery_saver_suppressed_ = false;
  ULONGLONG last_power_check_ms_ = 0;
  bool windows_animations_suppressed_ = false;
  ULONGLONG last_windows_animation_check_ms_ = 0;
  bool safe_mode_ = false;
  bool session_started_ = false;
  std::string startup_repair_status_ = "Not checked";
  float minimize_duration_seconds_ = 0.70f;
  float restore_duration_seconds_ = 0.70f;
  AppSettings settings_;
  std::atomic<bool> shutting_down_{false};
  SettingsWindow settings_window_;
};

}  // namespace genie::app
