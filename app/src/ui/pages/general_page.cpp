#include "pch.hpp"

#include "ui/pages/general_page.hpp"

#include <array>

#include "ui/components/controls.hpp"
#include "ui/settings_window.hpp"

namespace genie::ui::pages {
namespace {

constexpr float kPageTitleTextSize = 22.0f;
constexpr float kPageSubtitleTextSize = 13.0f;
constexpr float kSectionTitleTextSize = 15.0f;
constexpr float kLabelTextSize = 15.0f;
constexpr float kHelperTextSize = 13.0f;
constexpr float kCaptionTextSize = 12.0f;
constexpr ImU32 kPrimaryTextColor = ::genie::ui::theme::kText;
constexpr ImU32 kSecondaryTextColor = ::genie::ui::theme::kMutedText;

}  // namespace

void GeneralPage::Render(::genie::ui::SettingsWindow& window, components::PageLayout& layout,
                         const ::genie::ui::motion::MotionContext& motion, float scale,
                         float alpha) {
  using ui::components::SegmentSelector;
  using ui::components::Toggle;
  const float toggle_width = ::genie::ui::theme::Metrics::kToggleWidth * scale;
  const float toggle_height = (::genie::ui::theme::Metrics::kToggleHeight + 4.0f) * scale;

  layout.Title(window.font_title_, kPageTitleTextSize, "Effect", window.font_small_,
               kPageSubtitleTextSize, "Master switch and how the app behaves");

  layout.BeginGroup();
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeightHero);
  layout.ReserveControl(toggle_width);
  layout.RowTitle(window.font_medium_, kSectionTitleTextSize, "Genie animations",
                  kPrimaryTextColor);
  layout.RowSubtitle(window.font_small_, kHelperTextSize,
                     "Replace minimize and restore transitions", kSecondaryTextColor);
  const bool previous_enabled = window.controller_->view_model().enabled;
  ImVec2 cursor = layout.ControlCursor(toggle_width, toggle_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (Toggle(motion, "##animations_enabled", &window.controller_->view_model().enabled, scale,
             alpha)) {
    const bool saved =
        window.controller_->actions().SetEnabled(window.controller_->view_model().enabled);
    if (!saved) window.controller_->view_model().enabled = previous_enabled;
    window.RecordSaveResult(saved);
  }
  layout.EndRow();
  layout.EndGroup();

  layout.SectionCaption(window.font_small_, kCaptionTextSize, "WHEN CLOSING THIS WINDOW");
  layout.BeginGroup();
  layout.BeginStackRow(20.0f, ::genie::ui::theme::Metrics::kSegmentHeight);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Close action", kPrimaryTextColor);
  constexpr std::array close_labels = {"Quit app", "Keep in tray"};
  int close_behavior_segment = window.controller_->view_model().close_behavior == "tray" ? 1 : 0;
  const float segment_width = layout.content_width();
  layout.SetCursor(layout.content_left(), layout.StackControlY());
  if (SegmentSelector(motion, "##close_behavior", close_labels, &close_behavior_segment,
                      segment_width, window.font_body_, scale, alpha)) {
    const std::string previous = window.controller_->view_model().close_behavior;
    window.controller_->view_model().close_behavior = close_behavior_segment == 1 ? "tray" : "exit";
    const bool saved = window.controller_->actions().SetCloseBehavior(
        window.controller_->view_model().close_behavior);
    if (!saved) window.controller_->view_model().close_behavior = previous;
    window.RecordSaveResult(saved);
  }
  layout.EndRow();
  layout.EndGroup();

  layout.SectionCaption(window.font_small_, kCaptionTextSize, "STARTUP");
  layout.BeginGroup();
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeight);
  layout.ReserveControl(toggle_width);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Launch at login", kPrimaryTextColor);
  bool proposed_startup = window.controller_->view_model().run_at_startup;
  cursor = layout.ControlCursor(toggle_width, toggle_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (Toggle(motion, "##run_at_startup", &proposed_startup, scale, alpha)) {
    const bool saved = window.controller_->actions().SetStartupOptions(
        proposed_startup, window.controller_->view_model().start_minimized);
    if (saved) window.controller_->view_model().run_at_startup = proposed_startup;
    window.RecordSaveResult(saved);
  }
  layout.EndRow();

  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeight);
  layout.ReserveControl(toggle_width);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Start in tray", kPrimaryTextColor);
  bool proposed_minimized = window.controller_->view_model().start_minimized;
  cursor = layout.ControlCursor(toggle_width, toggle_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (Toggle(motion, "##start_minimized", &proposed_minimized, scale, alpha)) {
    const bool saved = window.controller_->actions().SetStartupOptions(
        window.controller_->view_model().run_at_startup, proposed_minimized);
    if (saved) window.controller_->view_model().start_minimized = proposed_minimized;
    window.RecordSaveResult(saved);
  }
  layout.EndRow();
  layout.EndGroup();

  layout.SectionCaption(window.font_small_, kCaptionTextSize, "BACKUP");
  layout.BeginGroup();
  const float button_width = 108.0f * scale;
  const float button_height = ::genie::ui::theme::Metrics::kButtonHeight * scale;
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeightTall);
  layout.ReserveControl(button_width);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Export settings", kPrimaryTextColor);
  layout.RowSubtitle(window.font_small_, kHelperTextSize, "Save a JSON profile you can share",
                     kSecondaryTextColor);
  cursor = layout.ControlCursor(button_width, button_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (ui::components::CompactButton(motion, "##export_settings", "Export",
                                    ImVec2(button_width, button_height), window.font_body_, scale,
                                    alpha)) {
    window.RecordSaveResult(window.controller_->actions().ExportSettings());
  }
  layout.EndRow();
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeightTall);
  layout.ReserveControl(button_width);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Import settings", kPrimaryTextColor);
  layout.RowSubtitle(window.font_small_, kHelperTextSize,
                     "Load a profile (current file is backed up first)", kSecondaryTextColor);
  cursor = layout.ControlCursor(button_width, button_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (ui::components::CompactButton(motion, "##import_settings", "Import",
                                    ImVec2(button_width, button_height), window.font_body_, scale,
                                    alpha)) {
    window.RecordSaveResult(window.controller_->actions().ImportSettings());
  }
  layout.EndRow();
  layout.EndGroup();
}

}  // namespace genie::ui::pages
