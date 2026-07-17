#include "pch.hpp"

#include "ui/pages/windows_integration_page.hpp"

#include "ui/components/controls.hpp"
#include "ui/settings_window.hpp"

namespace genie::ui::pages {
namespace {

constexpr float kPageTitleTextSize = 22.0f;
constexpr float kPageSubtitleTextSize = 13.0f;
constexpr float kLabelTextSize = 15.0f;
constexpr float kHelperTextSize = 13.0f;
constexpr float kCaptionTextSize = 12.0f;
constexpr ImU32 kPrimaryTextColor = ::genie::ui::theme::kText;
constexpr ImU32 kSecondaryTextColor = ::genie::ui::theme::kMutedText;

}  // namespace

void WindowsIntegrationPage::Render(::genie::ui::SettingsWindow& window,
                                    components::PageLayout& layout,
                                    const ::genie::ui::motion::MotionContext& motion, float scale,
                                    float alpha) {
  const float toggle_width = ::genie::ui::theme::Metrics::kToggleWidth * scale;
  const float toggle_height = (::genie::ui::theme::Metrics::kToggleHeight + 4.0f) * scale;
  auto& model = window.controller_->view_model();

  layout.Title(window.font_title_, kPageTitleTextSize, "System", window.font_small_,
               kPageSubtitleTextSize, "Fullscreen and power behavior");
  layout.SectionCaption(window.font_small_, kCaptionTextSize, "WINDOWS");
  layout.BeginGroup();
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeightTall);
  layout.ReserveControl(toggle_width);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Pause in fullscreen", kPrimaryTextColor);
  layout.RowSubtitle(window.font_small_, kHelperTextSize,
                     "Use native transitions during exclusive fullscreen", kSecondaryTextColor);
  const bool previous_fullscreen = model.disable_animations_fullscreen;
  ImVec2 cursor = layout.ControlCursor(toggle_width, toggle_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (ui::components::Toggle(motion, "##disable_fullscreen_animations",
                             &model.disable_animations_fullscreen, scale, alpha)) {
    const bool saved = window.controller_->actions().SetDisableAnimationsFullscreen(
        model.disable_animations_fullscreen);
    if (!saved) model.disable_animations_fullscreen = previous_fullscreen;
    window.RecordSaveResult(saved);
  }
  layout.EndRow();
  layout.EndGroup();

  layout.SectionCaption(window.font_small_, kCaptionTextSize, "POWER");
  layout.BeginGroup();
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeight);
  layout.ReserveControl(toggle_width);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Disable in battery saver", kPrimaryTextColor);
  bool proposed_battery_saver = model.disable_effects_battery_saver;
  cursor = layout.ControlCursor(toggle_width, toggle_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (ui::components::Toggle(motion, "##disable_in_battery_saver", &proposed_battery_saver, scale,
                             alpha)) {
    const bool saved =
        window.controller_->actions().SetDisableEffectsBatterySaver(proposed_battery_saver);
    if (saved) model.disable_effects_battery_saver = proposed_battery_saver;
    window.RecordSaveResult(saved);
  }
  layout.EndRow();
  layout.EndGroup();
}

}  // namespace genie::ui::pages
