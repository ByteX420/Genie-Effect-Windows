#pragma once

#include <array>
#include <d3d11.h>
#include <dxgi.h>
#include <functional>
#include <windows.h>
#include <wrl/client.h>

#include "app/settings_store.hpp"

struct ImFont;

namespace genie::app {

enum class TemporaryPauseAction {
  kResume,
  kTenMinutes,
  kOneHour,
  kUntilRestart,
};

enum class HotkeyUpdateResult {
  kSuccess,
  kInvalid,
  kDuplicate,
  kUnavailable,
  kSaveFailed,
};

enum class DiagnosticsAction {
  kCopy,
  kOpenLogFolder,
  kRepairWindows,
  kRestartRenderer,
  kExitSafeMode,
};

struct DiagnosticsSnapshot {
  std::string effect;
  std::string hook;
  std::string renderer;
  std::string d3d_device;
  std::string active_animations;
  std::string watchdog;
  std::string display_refresh;
  std::string window_monitor;
  std::string taskbar;
  std::string startup_repair;
  std::string version;
  std::string windows_version;
  std::string graphics_adapter;
  std::string monitor_configuration;
  std::string log_folder_size;
  std::string report;
};

class SettingsWindow {
public:
  using ToggleCallback = std::function<bool(bool)>;
  using SpeedCallback =
      std::function<bool(float minimize_duration, float restore_duration, bool save)>;
  using LinkCallback = std::function<bool(bool)>;
  using FullscreenBehaviorCallback = std::function<bool(bool)>;
  using BatterySaverCallback = std::function<bool(bool)>;
  using EasingCallback = std::function<bool(const std::string&, const std::string&)>;
  using AnimationStyleCallback = std::function<bool(const std::string&)>;
  using StrengthCallback = std::function<bool(float, bool)>;
  using FadeCallback = std::function<bool(const std::string&)>;
  using TargetIndicatorCallback = std::function<bool(bool)>;
  using CloseBehaviorCallback = std::function<bool(const std::string&)>;
  using StartupCallback = std::function<bool(bool run_at_startup, bool start_minimized)>;
  using ExclusionCallback = std::function<bool(const std::string&, bool exclude)>;
  using PauseCallback = std::function<void(TemporaryPauseAction)>;
  using HotkeyUpdateCallback = std::function<HotkeyUpdateResult(HotkeyAction, HotkeyBinding)>;
  using HotkeyActionCallback = std::function<void(HotkeyAction)>;
  using DiagnosticsCallback = std::function<DiagnosticsSnapshot()>;
  using DiagnosticsActionCallback = std::function<bool(DiagnosticsAction)>;
  using HealCallback = std::function<void()>;
  using ExitCallback = std::function<void()>;

  SettingsWindow() = default;
  ~SettingsWindow();
  SettingsWindow(const SettingsWindow&) = delete;
  SettingsWindow& operator=(const SettingsWindow&) = delete;

  bool Initialize(HINSTANCE instance, ToggleCallback toggle_callback, SpeedCallback speed_callback,
                  LinkCallback link_callback,
                  FullscreenBehaviorCallback fullscreen_behavior_callback,
                  BatterySaverCallback battery_saver_callback, EasingCallback easing_callback,
                  AnimationStyleCallback animation_style_callback,
                  StrengthCallback strength_callback, FadeCallback fade_callback,
                  TargetIndicatorCallback target_indicator_callback, CloseBehaviorCallback close_behavior_callback,
                  StartupCallback startup_callback,
                  ExclusionCallback exclusion_callback, PauseCallback pause_callback,
                  HotkeyUpdateCallback hotkey_update_callback,
                  HotkeyActionCallback hotkey_action_callback,
                  DiagnosticsCallback diagnostics_callback,
                  DiagnosticsActionCallback diagnostics_action_callback, HealCallback heal_callback,
                  ExitCallback exit_callback);
  void Shutdown();
  void Show(bool show);
  void UpdateState(const AppSettings& settings);
  void UpdatePauseState(bool paused, bool until_restart);
  void SetHotkeyRegistrationStatus(HotkeyAction action, bool available);
  void ShowDiagnosticsPage();
  void Render();
  void ForceRender();
  [[nodiscard]] static bool ActivateExistingInstance(DWORD timeout_ms);
  [[nodiscard]] HWND hwnd() const { return hwnd_; }
  [[nodiscard]] bool tray_icon_available() const { return tray_icon_added_; }
  [[nodiscard]] bool WantsContinuousRendering() const;

private:
  enum class Page {
    kGeneral,
    kAnimation,
    kApplications,
    kWindowsIntegration,
    kHotkeys,
    kDiagnostics,
    kAbout,
  };

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
  static LRESULT CALLBACK PreviewWindowProc(HWND hwnd, UINT message, WPARAM w_param,
                                            LPARAM l_param);
  bool CreateRenderWindow(HINSTANCE instance);
  bool CreateDeviceResources();
  bool CreateRenderTarget();
  void ReleaseDeviceResources();
  void HandleDeviceLost();
  bool TryRecoverDeviceResources();
  [[nodiscard]] static bool IsDeviceLostError(HRESULT hr);
  void CleanupRenderTarget();
  void ApplyStyle();
  void RebuildFonts(UINT dpi);
  void ApplyWindowShape(int width, int height);
  void UpdateDpi(UINT dpi);
  void UpdateTrayTooltip();
  bool AddTrayIcon();
  void FlushPendingSpeedSave();
  void RecordSaveResult(bool saved);
  void HandleCloseRequest();
  void StartAnimationPreview();
  void UpdateAnimationPreview();
  void CloseAnimationPreview();
  void RenderContents();
  std::vector<std::string> GetActiveApplications();

  HWND hwnd_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view_;
  ToggleCallback toggle_callback_;
  SpeedCallback speed_callback_;
  LinkCallback link_callback_;
  FullscreenBehaviorCallback fullscreen_behavior_callback_;
  BatterySaverCallback battery_saver_callback_;
  EasingCallback easing_callback_;
  AnimationStyleCallback animation_style_callback_;
  StrengthCallback strength_callback_;
  FadeCallback fade_callback_;
  TargetIndicatorCallback target_indicator_callback_;
  CloseBehaviorCallback close_behavior_callback_;
  StartupCallback startup_callback_;
  ExclusionCallback exclusion_callback_;
  PauseCallback pause_callback_;
  HotkeyUpdateCallback hotkey_update_callback_;
  HotkeyActionCallback hotkey_action_callback_;
  DiagnosticsCallback diagnostics_callback_;
  DiagnosticsActionCallback diagnostics_action_callback_;
  HealCallback heal_callback_;
  ExitCallback exit_callback_;
  bool is_enabled_ = true;
  bool temporarily_paused_ = false;
  bool paused_until_restart_ = false;
  std::array<HotkeyBinding, static_cast<size_t>(HotkeyAction::kCount)> hotkeys_{};
  std::array<bool, static_cast<size_t>(HotkeyAction::kCount)> hotkey_available_{};
  int editing_hotkey_ = -1;
  std::string hotkey_feedback_;
  DiagnosticsSnapshot diagnostics_;
  ULONGLONG last_diagnostics_refresh_ms_ = 0;
  std::string diagnostics_feedback_;
  float minimize_duration_seconds_ = 0.70f;
  float restore_duration_seconds_ = 0.70f;
  bool link_speeds_ = false;
  bool disable_animations_fullscreen_ = false;
  bool disable_effects_battery_saver_ = false;
  std::string minimize_easing_ = "Linear";
  std::string restore_easing_ = "Linear";
  std::string animation_style_ = "Classic Genie";
  float genie_strength_ = 1.0f;
  std::string fade_strength_ = "Subtle";
  bool show_target_indicator_ = false;
  bool strength_slider_active_ = false;
  bool strength_slider_dirty_ = false;
  std::string close_behavior_ = "exit";
  bool run_at_startup_ = false;
  bool start_minimized_ = false;
  std::vector<std::string> excluded_applications_;
  std::array<char, 260> exclusion_input_{};
  std::string exclusion_error_;
  ULONGLONG last_active_apps_refresh_ms_ = 0;
  std::vector<std::string> cached_active_apps_;
  std::string persistence_error_;
  std::string save_feedback_;
  ULONGLONG save_feedback_until_ms_ = 0;
  bool save_feedback_error_ = false;
  Page selected_page_ = Page::kGeneral;
  bool reset_page_scroll_ = false;
  bool minimize_slider_active_ = false;
  bool minimize_slider_dirty_ = false;
  bool restore_slider_active_ = false;
  bool restore_slider_dirty_ = false;
  bool preview_active_ = false;
  HWND preview_window_ = nullptr;
  int preview_phase_ = 0;
  ULONGLONG preview_phase_started_ms_ = 0;
  bool preview_dragging_ = false;
  POINT preview_drag_offset_{};
  bool window_dragging_ = false;
  POINT window_drag_offset_{};
  bool imgui_ready_ = false;
  bool tray_icon_added_ = false;
  UINT taskbar_created_message_ = 0;
  bool imgui_context_ready_ = false;
  bool imgui_win32_ready_ = false;
  bool imgui_dx11_ready_ = false;
  bool device_recovery_pending_ = false;
  ULONGLONG next_device_recovery_ms_ = 0;
  DWORD device_recovery_delay_ms_ = 0;
  bool device_recovery_test_pending_ = false;
  bool render_requested_ = false;
  ULONGLONG shown_at_ms_ = 0;
  UINT current_dpi_ = USER_DEFAULT_SCREEN_DPI;
  float ui_scale_ = 1.0f;
  ImFont* font_small_ = nullptr;
  ImFont* font_body_ = nullptr;
  ImFont* font_medium_ = nullptr;
  ImFont* font_title_ = nullptr;
};

}  // namespace genie::app
