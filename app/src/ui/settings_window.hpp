#pragma once

#include <array>
#include <memory>
#include <windows.h>

#include "features/diagnostics_service.hpp"
#include "settings/app_settings.hpp"
#include "ui/animation_preview.hpp"
#include "ui/application_list_provider.hpp"
#include "ui/motion/motion.hpp"
#include "ui/motion/motion_tokens.hpp"
#include "ui/rendering/imgui_renderer.hpp"
#include "ui/settings_actions.hpp"
#include "ui/settings_controller.hpp"
#include "ui/settings_view_model.hpp"
#include "ui/tray_icon.hpp"

struct ImFont;

namespace genie::ui::pages {
class AboutPage;
class AnimationPage;
class ApplicationsPage;
class DiagnosticsPage;
class GeneralPage;
class HotkeysPage;
class WindowsIntegrationPage;
}  // namespace genie::ui::pages

namespace genie::ui {
class SettingsShell;

class SettingsWindow {
public:
  SettingsWindow() = default;
  ~SettingsWindow();
  SettingsWindow(const SettingsWindow&) = delete;
  SettingsWindow& operator=(const SettingsWindow&) = delete;

  bool Initialize(HINSTANCE instance, ui::SettingsActions& actions);
  void Shutdown();
  void Show(bool show);
  void UpdateState(const genie::settings::AppSettings& settings);
  void UpdatePauseState(bool paused, bool until_restart);
  void SetHotkeyRegistrationStatus(genie::settings::HotkeyAction action, bool available);
  void Render();
  void ForceRender();
  [[nodiscard]] HWND hwnd() const { return hwnd_; }
  [[nodiscard]] bool WantsContinuousRendering() const;

private:
  friend class SettingsShell;
  friend class ui::pages::AboutPage;
  friend class ui::pages::AnimationPage;
  friend class ui::pages::ApplicationsPage;
  friend class ui::pages::DiagnosticsPage;
  friend class ui::pages::GeneralPage;
  friend class ui::pages::HotkeysPage;
  friend class ui::pages::WindowsIntegrationPage;
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
  bool CreateRenderWindow(HINSTANCE instance);
  void ApplyWindowShape(int width, int height);
  void UpdateDpi(UINT dpi);
  void UpdateReducedMotion();
  void FlushPendingSpeedSave();
  void RecordSaveResult(bool saved);
  void HandleCloseRequest();

  HWND hwnd_ = nullptr;
  ui::rendering::ImguiRenderer renderer_;
  std::unique_ptr<ui::SettingsController> controller_;
  int editing_hotkey_ = -1;
  std::string hotkey_feedback_;
  ULONGLONG last_diagnostics_refresh_ms_ = 0;
  std::string diagnostics_feedback_;
  bool minimize_bezier_dirty_ = false;
  bool restore_bezier_dirty_ = false;
  bool minimize_bezier_active_ = false;
  bool restore_bezier_active_ = false;
  bool strength_slider_active_ = false;
  bool strength_slider_dirty_ = false;
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
  ui::AnimationPreview animation_preview_;
  ui::ApplicationListProvider application_list_provider_;
  bool window_dragging_ = false;
  POINT window_drag_offset_{};
  ui::TrayIcon tray_icon_;
  bool render_requested_ = false;
  ULONGLONG shown_at_ms_ = 0;
  UINT current_dpi_ = USER_DEFAULT_SCREEN_DPI;
  float ui_scale_ = 1.0f;
  ImFont* font_small_ = nullptr;
  ImFont* font_body_ = nullptr;
  ImFont* font_medium_ = nullptr;
  ImFont* font_title_ = nullptr;
  ui::motion::MotionSystem motion_system_;
  ui::motion::MotionTokens motion_tokens_ = ui::motion::MotionTokens::Default();
};

}  // namespace genie::ui
