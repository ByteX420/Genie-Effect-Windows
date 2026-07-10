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
  void RequestShutdown();
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

  struct AnimationSlot {
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
  };

  int FindSlotForWindow(HWND window) const;
  int FindFreeSlot() const;

  bool OnMinimizeStart(HWND window);
  bool OnRestoreAttempt(HWND window);
  void OnWindowSeen(HWND window, DWORD event);
  void UpdatePreMinimizeSnapshot(HWND window);
  void CompletePendingNativeMinimize(int slot_index);
  void FinishActiveAnimation(int slot_index);
  void PruneSnapshots();
  bool PreserveRestorePlacementAndMarkOffscreen(HWND window, CachedSnapshot* snapshot);
  bool IsGenieWindowRestored(HWND window) const;
  void RestoreWindowFromGenieState(HWND window, bool force_show_if_iconic = true);
  void SetEnabled(bool enabled);
  void SetAnimationDuration(float duration_seconds);
  bool InstallCbtHook();
  void UninstallCbtHook();
  void ResetAnimationFramePacing(int slot_index, HWND window, const RECT& animation_bounds);
  void UpdateAnimationFramePacingMonitor(int slot_index);
  [[nodiscard]] bool IsAnimationFrameDue(int slot_index) const;
  void AdvanceAnimationFrameDeadline(int slot_index);
  void WaitForAnimationFrameOrMessage();
  void BeginFallbackTimerResolution();
  void EndFallbackTimerResolution();
  bool CreateAnimationRenderer();
  void BeginAnimationRendererRecovery();
  bool TryRecoverAnimationRenderer();
  [[nodiscard]] bool AnimationRendererDeviceLost() const;

  std::unique_ptr<rendering::D3dDevice> d3d_device_;
  std::unique_ptr<rendering::DesktopCapture> desktop_capture_;
  AnimationSlot slots_[2];
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
  bool is_enabled_ = true;
  float animation_duration_seconds_ = 0.70f;
  std::atomic<bool> shutting_down_{false};
  SettingsWindow settings_window_;
};

}  // namespace genie::app
