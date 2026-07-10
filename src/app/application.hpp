#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <windows.h>

#include "app/settings_window.hpp"
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
  void CleanupAndRestoreAll();
  void HealLeftoverWindows();

private:
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

  bool OnMinimizeStart(HWND window);
  bool OnRestoreAttempt(HWND window);
  void OnWindowSeen(HWND window, DWORD event);
  void UpdatePreMinimizeSnapshot(HWND window);
  void CompletePendingNativeMinimize();
  void FinishActiveAnimation();
  void PruneSnapshots();
  bool PreserveRestorePlacementAndMarkOffscreen(HWND window, CachedSnapshot* snapshot);
  bool IsGenieWindowRestored(HWND window) const;
  void RestoreWindowFromGenieState(HWND window, bool force_show_if_iconic = true);
  void SetEnabled(bool enabled);
  void SetAnimationDuration(float duration_seconds);
  bool InstallCbtHook();
  void UninstallCbtHook();
  void ResetAnimationFramePacing(HWND window, const RECT& animation_bounds);
  void UpdateAnimationFramePacingMonitor();
  [[nodiscard]] bool IsAnimationFrameDue() const;
  void AdvanceAnimationFrameDeadline();
  void WaitForAnimationFrameOrMessage();
  void BeginFallbackTimerResolution();
  void EndFallbackTimerResolution();

  std::unique_ptr<rendering::D3dDevice> d3d_device_;
  std::unique_ptr<rendering::DesktopCapture> desktop_capture_;
  rendering::OverlayWindow overlay_window_;
  platform::NativeAnimationBlocker native_animation_blocker_;
  platform::WindowEventMonitor window_event_monitor_;
  platform::TaskbarTargetProvider taskbar_target_provider_;
  HWND animating_window_ = nullptr;
  HWND pending_native_minimize_window_ = nullptr;
  bool animating_restore_ = false;
  ULONGLONG minimize_start_time_ms_ = 0;
  HMODULE hook_dll_ = nullptr;
  HHOOK cbt_hook_ = nullptr;
  std::unordered_map<HWND, CachedSnapshot> pre_minimize_snapshots_;
  std::unordered_map<HWND, CachedSnapshot> restore_snapshots_;
  RECT live_animation_bounds_{};
  ULONGLONG last_snapshot_refresh_ms_ = 0;
  ULONGLONG last_animation_texture_refresh_ms_ = 0;
  HANDLE animation_frame_timer_ = nullptr;
  bool animation_frame_timer_is_high_resolution_ = false;
  bool fallback_timer_resolution_active_ = false;
  UINT fallback_timer_period_ms_ = 0;
  HMONITOR animation_monitor_ = nullptr;
  std::chrono::steady_clock::duration animation_frame_interval_{};
  std::chrono::steady_clock::time_point next_animation_frame_time_{};
  bool live_animation_capture_enabled_ = false;
  bool in_restore_window_state_ = false;
  bool is_enabled_ = true;
  float animation_duration_seconds_ = 0.70f;
  std::atomic<bool> shutting_down_{false};
  SettingsWindow settings_window_;
};

}  // namespace genie::app
