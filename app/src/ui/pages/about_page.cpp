#include "pch.hpp"

#include "ui/pages/about_page.hpp"

#include <algorithm>

#include "app/resource.hpp"
#include "core/embedded_resource.hpp"
#include "ui/components/controls.hpp"
#include "ui/settings_window.hpp"
#include "ui/theme/theme_tokens.hpp"

namespace genie::ui::pages {
namespace {

constexpr float kPageTitleTextSize = 22.0f;
constexpr float kPageSubtitleTextSize = 13.0f;
constexpr float kSectionTitleTextSize = 15.0f;
constexpr float kLabelTextSize = 15.0f;
constexpr float kValueTextSize = 13.0f;
constexpr float kHelperTextSize = 13.0f;
constexpr float kCaptionTextSize = 12.0f;
constexpr ImU32 kPrimaryTextColor = ::genie::ui::theme::kText;
constexpr ImU32 kSecondaryTextColor = ::genie::ui::theme::kMutedText;

}  // namespace

void AboutPage::Render(::genie::ui::SettingsWindow& window, components::PageLayout& layout,
                       const ::genie::ui::motion::MotionContext& motion, float scale, float alpha) {
  auto px = [scale](float value) { return value * scale; };
  const ULONGLONG now = GetTickCount64();
  auto& diagnostics = window.controller_->view_model().diagnostics;
  if (diagnostics.version.empty() || now - window.last_diagnostics_refresh_ms_ >= 500) {
    diagnostics = window.controller_->actions().GetDiagnostics();
    window.last_diagnostics_refresh_ms_ = now;
  }
  layout.Title(window.font_title_, kPageTitleTextSize, "About", window.font_small_,
               kPageSubtitleTextSize, "Product info and open-source licenses");
  const std::string version = diagnostics.version.empty() ? "—" : diagnostics.version;

  layout.SectionCaption(window.font_small_, kCaptionTextSize, "PRODUCT");
  layout.BeginGroup();
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeightHero);
  layout.RowTitle(window.font_medium_, kSectionTitleTextSize, "Genie Effect", kPrimaryTextColor);
  layout.RowSubtitle(window.font_small_, kHelperTextSize, "Native genie minimize for Windows",
                     kSecondaryTextColor);
  layout.EndRow();
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeight);
  layout.ReserveControl(layout.content_width() * 0.55f);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Build", kPrimaryTextColor);
  layout.RowValue(window.font_small_, kValueTextSize, version.c_str(), kSecondaryTextColor);
  layout.EndRow();
  layout.EndGroup();

  const float button_height = ::genie::ui::theme::Metrics::kButtonHeight * scale;
  layout.SectionCaption(window.font_small_, kCaptionTextSize, "FONTS");
  layout.BeginGroup();
  layout.BeginRow(::genie::ui::theme::Metrics::kRowHeightTall);
  const float license_width = px(112.0f);
  layout.ReserveControl(license_width);
  layout.RowTitle(window.font_body_, kLabelTextSize, "Inter", kPrimaryTextColor);
  layout.RowSubtitle(window.font_small_, kHelperTextSize, "SIL Open Font License 1.1",
                     kSecondaryTextColor);
  const ImVec2 cursor = layout.ControlCursor(license_width, button_height);
  layout.SetCursor(cursor.x, cursor.y);
  if (ui::components::CompactButton(motion, "##font_license", "License",
                                    ImVec2(license_width, button_height), window.font_body_, scale,
                                    alpha)) {
    ImGui::OpenPopup("Inter License");
  }
  layout.EndRow();
  layout.EndGroup();

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 modal_size(std::min(px(560.0f), viewport->WorkSize.x - px(48.0f)),
                          std::min(px(460.0f), viewport->WorkSize.y - px(48.0f)));
  ImGui::SetNextWindowSize(modal_size, ImGuiCond_Appearing);
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                 viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(22.0f), px(20.0f)));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, px(::genie::ui::theme::Metrics::kCardRounding));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, std::max(1.0f, scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(px(8.0f), px(8.0f)));
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::theme::kPanelColor);
  ImGui::PushStyleColor(ImGuiCol_Border, ui::theme::kBorderColor);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));

  if (ImGui::BeginPopupModal("Inter License", nullptr,
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoScrollWithMouse)) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImFont* title_font = window.font_medium_ ? window.font_medium_ : window.font_body_;
    ImGui::PushFont(title_font);
    ImGui::TextUnformatted("Inter");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, ui::theme::kTextDimColor);
    if (window.font_small_) ImGui::PushFont(window.font_small_);
    ImGui::TextUnformatted("SIL Open Font License 1.1");
    if (window.font_small_) ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, px(4.0f)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, px(4.0f)));

    const float footer_gap = px(14.0f);
    const float body_height =
        std::max(px(120.0f), ImGui::GetContentRegionAvail().y - button_height - footer_gap);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::BeginChild("##font_license_text", ImVec2(0.0f, body_height), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground)) {
      static const std::string license = core::LoadEmbeddedText(IDR_UI_FONT_LICENSE);
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, px(6.0f)));
      ImGui::PushStyleColor(ImGuiCol_Text, ui::theme::kTextDimColor);
      if (window.font_small_) ImGui::PushFont(window.font_small_);
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
      ImGui::TextUnformatted(license.empty() ? "The embedded font license could not be loaded."
                                             : license.c_str());
      ImGui::PopTextWrapPos();
      if (window.font_small_) ImGui::PopFont();
      ImGui::PopStyleColor();
      ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.0f, footer_gap - px(4.0f)));
    const float close_width = px(120.0f);
    const float available = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (available - close_width) * 0.5f));
    if (ui::components::CompactButton(motion, "##close_font_license", "Close",
                                      ImVec2(close_width, button_height), window.font_body_, scale,
                                      1.0f)) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleColor(4);
  ImGui::PopStyleVar(4);
}

}  // namespace genie::ui::pages
