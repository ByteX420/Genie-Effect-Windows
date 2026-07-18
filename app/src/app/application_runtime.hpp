#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <windows.h>

#include "app/message_loop.hpp"
#include "features/animation_configuration.hpp"
#include "features/diagnostics_service.hpp"
#include "features/effect_controller.hpp"
#include "features/effect_policy.hpp"
#include "features/hotkey_controller.hpp"
#include "features/minimize_feature.hpp"
#include "features/pause_controller.hpp"
#include "features/restore_feature.hpp"
#include "features/settings_mutation_service.hpp"
#include "features/window_recovery_service.hpp"
#include "platform/windows/cbt_hook_manager.hpp"
#include "platform/windows/global_hotkey_manager.hpp"
#include "platform/windows/native_animation_blocker.hpp"
#include "platform/windows/taskbar_target_provider.hpp"
#include "rendering/d3d_device.hpp"
#include "rendering/desktop_capture.hpp"
#include "rendering/overlay_window.hpp"
#include "runtime/animation_run.hpp"
#include "runtime/animation_run_pool.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/renderer_recovery.hpp"
#include "runtime/run_state.hpp"
#include "runtime/snapshot_cache.hpp"
#include "settings/settings_service.hpp"
#include "ui/settings_window.hpp"

namespace genie::app {

class ApplicationRuntime : public ui::SettingsActions {
public:
  ApplicationRuntime() = default;
  ~ApplicationRuntime();

  ApplicationRuntime(const ApplicationRuntime&) = delete;
  ApplicationRuntime& operator=(const ApplicationRuntime&) = delete;

  bool Initialize(HINSTANCE instance);
  int Run();
  void RequestShutdown();
  void CleanupAndRestoreAll();
  void HealLeftoverWindows();
  bool SetEnabled(bool enabled) override;
  bool SetAnimationDurations(float minimize_duration, float restore_duration, bool save) override;
  bool SetLinkSpeeds(bool linked) override;
  bool SetDisableAnimationsFullscreen(bool enabled) override;
  bool SetDisableEffectsBatterySaver(bool enabled) override;
  bool SetEasing(const std::string& minimize_easing, const std::string& restore_easing) override;
  bool SetCustomEasingBezier(bool is_minimize, animation::CubicBezier bezier, bool save) override;
  bool SetAnimationStyle(const std::string& style) override;
  bool SetQualityMode(const std::string& mode) override;
  bool SetGenieStrength(float strength, bool save) override;
  bool SetFadeStrength(const std::string& strength) override;
  bool SetTargetIndicator(bool enabled) override;
  bool SetSmartSkipUnderLoad(bool enabled) override;
  bool SetCloseBehavior(const std::string& close_behavior) override;
  bool SetStartupOptions(bool run_at_startup, bool start_minimized) override;
  bool SetApplicationExcluded(const std::string& executable_name, bool excluded) override;
  bool ExportSettings() override;
  bool ImportSettings() override;
  void SetTemporaryPause(ui::TemporaryPauseAction action) override;
  ui::HotkeyUpdateResult SetHotkey(settings::HotkeyAction action,
                                   settings::HotkeyBinding binding) override;
  void ExecuteHotkeyAction(settings::HotkeyAction action) override;
  [[nodiscard]] features::DiagnosticsSnapshot GetDiagnostics() const override;
  bool ExecuteDiagnosticsAction(features::DiagnosticsAction action) override;
  void HealWindows() override { HealLeftoverWindows(); }
  void RequestExit() override { RequestShutdown(); }

private:
  enum class RunCleanupOutcome {
    kCompleted,
    kAborted,
  };

  int FindRunForWindow(HWND window) const;
  [[nodiscard]] bool IsOverlayWindow(HWND window) const;
  int FindAvailableRun();
  bool InitializeRun(runtime::AnimationRun& slot);
  void SetRunState(int run_index, runtime::RunState state);
  void CleanupRun(int run_index, RunCleanupOutcome outcome);
  void CheckAnimationTimeouts();
  void UpdateRuntime();
  void HandleDisplayChange();
  [[nodiscard]] MessageLoopWait TickRuntime();

  bool OnMinimizeStart(HWND window);
  bool OnRestoreAttempt(HWND window);
  void FinishActiveAnimation(int run_index);
  void RestoreWindowFromGenieState(HWND window, bool force_show_if_iconic = true);
  void UpdateTemporaryPause();
  void RegisterConfiguredHotkeys();
  void UnregisterAllHotkeys();
  [[nodiscard]] features::DiagnosticsSnapshot BuildDiagnosticsSnapshot() const;
  [[nodiscard]] bool IsTemporarilyPaused() const;
  [[nodiscard]] bool IsEffectActive() const;
  void UpdateFullscreenSuppression(bool force = false);
  void UpdatePowerState(bool force = false);
  void EnableEffectRuntime();
  void DisableEffectRuntime();
  void RefreshEffectRuntimeState();
  [[nodiscard]] features::RenderingPressure GetRenderingPressure() const;
  [[nodiscard]] HWND GetOverlayWindow() const;
  void ResetAnimationFramePacing(int run_index, HWND window, const RECT& animation_bounds);
  void UpdateAnimationFramePacingMonitor(int run_index);
  [[nodiscard]] bool IsAnimationFrameDue(int run_index) const;
  void AdvanceAnimationFrameDeadline(int run_index);
  void WaitForAnimationFrameOrMessage();
  bool CreateAnimationRenderer();
  void BeginAnimationRendererRecovery();
  bool TryRecoverAnimationRenderer();
  [[nodiscard]] bool AnimationRendererDeviceLost() const;

  std::unique_ptr<rendering::D3dDevice> d3d_device_;
  std::unique_ptr<rendering::DesktopCapture> desktop_capture_;
  runtime::AnimationRunPool runs_;
  runtime::FrameScheduler frame_scheduler_;
  platform::NativeAnimationBlocker native_animation_blocker_;
  platform::TaskbarTargetProvider taskbar_target_provider_;
  HINSTANCE instance_ = nullptr;
  DWORD main_thread_id_ = 0;
  platform::windows::CbtHookManager cbt_hook_manager_;
  platform::windows::GlobalHotkeyManager hotkey_manager_;
  runtime::SnapshotCache snapshot_cache_;
  ULONGLONG last_snapshot_refresh_ms_ = 0;
  runtime::RendererRecovery renderer_recovery_;
  bool effect_runtime_active_ = false;
  ULONGLONG last_fullscreen_check_ms_ = 0;
  ULONGLONG last_power_check_ms_ = 0;
  unsigned int recent_missed_frames_ = 0;
  unsigned int recent_device_failures_ = 0;
  float last_capture_duration_ms_ = 0.0f;
  bool device_recovery_test_pending_ = false;
  std::string startup_repair_status_ = "Not checked";
  genie::settings::SettingsService settings_service_;
  genie::features::HotkeyController hotkey_controller_{settings_service_, hotkey_manager_};
  genie::features::SettingsMutationService settings_mutations_{settings_service_};
  genie::features::EffectPolicy effect_policy_;
  genie::features::AnimationConfiguration animation_configuration_{settings_service_,
                                                                   effect_policy_};
  genie::features::DiagnosticsService diagnostics_service_;
  genie::features::PauseController pause_controller_;
  genie::features::WindowRecoveryService window_recovery_service_{snapshot_cache_};
  genie::features::MinimizeFeature minimize_feature_{effect_policy_, window_recovery_service_,
                                                     runs_, snapshot_cache_};
  genie::features::RestoreFeature restore_feature_{effect_policy_, window_recovery_service_,
                                                   snapshot_cache_, runs_, minimize_feature_};
  genie::features::EffectController effect_controller_{effect_policy_, pause_controller_,
                                                       minimize_feature_, restore_feature_};
  std::atomic<bool> shutting_down_{false};
  std::atomic<bool> cleaned_up_{false};
  ui::SettingsWindow settings_window_;
  MessageLoop message_loop_;
};

}  // namespace genie::app
