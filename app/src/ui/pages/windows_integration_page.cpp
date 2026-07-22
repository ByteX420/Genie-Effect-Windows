#include "pch.hpp"

#include "ui/pages/windows_integration_page.hpp"

#include "ui/components/controls.hpp"
#include "ui/settings_window.hpp"

namespace minimize::ui::pages {
namespace {

constexpr float kPageTitleTextSize = 22.0f;
constexpr float kPageSubtitleTextSize = 13.0f;
constexpr float kLabelTextSize = 15.0f;
constexpr float kHelperTextSize = 13.0f;
constexpr float kCaptionTextSize = 12.0f;
constexpr ImU32 kPrimaryTextColor = ::minimize::ui::theme::kText;
constexpr ImU32 kSecondaryTextColor = ::minimize::ui::theme::kMutedText;

}  // namespace

void WindowsIntegrationPage::Render(::minimize::ui::SettingsWindow& window,
                                    components::PageLayout& layout,
                                    const ::minimize::ui::motion::MotionContext& motion, float scale,
                                    float alpha) {
  const float toggle_width = ::minimize::ui::theme::Metrics::kToggleWidth * scale;
  const float toggle_height = (::minimize::ui::theme::Metrics::kToggleHeight + 4.0f) * scale;
  auto& model = window.controller_->view_model();

  layout.Title(window.font_title_, kPageTitleTextSize, "System", window.font_small_,
               kPageSubtitleTextSize, "Fullscreen and power behavior");
  layout.SectionCaption(window.font_small_, kCaptionTextSize, "WINDOWS");
  layout.BeginGroup();
  layout.BeginRow(::minimize::ui::theme::Metrics::kRowHeightTall);
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

  layout.BeginRow(::minimize::ui::theme::Metrics::kRowHeightTall);
  layout.ReserveControl(toggle_width);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Smart skip under load", kPrimaryTextColor);
  layout.RowSubtitle(window.font_small_, kHelperTextSize,
                     "Fall back to native minimize when capture/GPU load is high",
                     kSecondaryTextColor);
  const bool previous_smart_skip = model.smart_skip_under_load;
  cursor = layout.ControlCursor(toggle_width, toggle_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (ui::components::Toggle(motion, "##smart_skip_under_load", &model.smart_skip_under_load,
                             scale, alpha)) {
    const bool saved =
        window.controller_->actions().SetSmartSkipUnderLoad(model.smart_skip_under_load);
    if (!saved) model.smart_skip_under_load = previous_smart_skip;
    window.RecordSaveResult(saved);
  }
  layout.EndRow();
  layout.EndGroup();

  layout.SectionCaption(window.font_small_, kCaptionTextSize, "POWER");
  layout.BeginGroup();
  layout.BeginRow(::minimize::ui::theme::Metrics::kRowHeight);
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

}  // namespace minimize::ui::pages
