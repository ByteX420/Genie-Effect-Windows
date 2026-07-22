#include "pch.hpp"

#include "ui/pages/applications_page.hpp"

#include <algorithm>
#include <cctype>
#include <format>

#include "settings/exclusion_rules.hpp"
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

struct ApplicationItem {
  std::string name;
  bool excluded = false;
  bool active = false;
};

std::string Lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

}  // namespace

void ApplicationsPage::Render(::minimize::ui::SettingsWindow& window, components::PageLayout& layout,
                              const ::minimize::ui::motion::MotionContext& motion, float scale,
                              float alpha) {
  auto px = [scale](float value) { return value * scale; };
  layout.Title(window.font_title_, kPageTitleTextSize, "Apps", window.font_small_,
               kPageSubtitleTextSize, "Skip the effect for selected programs");
  const ULONGLONG now = GetTickCount64();
  if (now - window.last_active_apps_refresh_ms_ > 2000 || window.cached_active_apps_.empty()) {
    window.cached_active_apps_ = window.application_list_provider_.GetActiveApplications();
    window.last_active_apps_refresh_ms_ = now;
  }

  auto& excluded_applications = window.controller_->view_model().excluded_applications;
  std::vector<ApplicationItem> items;
  for (const std::string& excluded : excluded_applications)
    items.push_back({excluded, true, false});
  for (const std::string& active : window.cached_active_apps_) {
    const auto existing =
        std::find_if(items.begin(), items.end(), [&active](const ApplicationItem& item) {
          return settings::ExecutableNamesEqual(item.name, active);
        });
    if (existing != items.end())
      existing->active = true;
    else
      items.push_back({active, false, true});
  }
  std::sort(items.begin(), items.end(),
            [](const ApplicationItem& left, const ApplicationItem& right) {
              return left.name < right.name;
            });
  const std::string filter = Lowercase(window.exclusion_input_.data());
  std::vector<ApplicationItem> filtered;
  for (const ApplicationItem& item : items) {
    if (filter.empty() || Lowercase(item.name).find(filter) != std::string::npos)
      filtered.push_back(item);
  }

  layout.BeginGroup();
  layout.BeginStackRow(0.0f, 36.0f);
  const float field_width = layout.content_width();
  const float field_height = layout.StackControlHeight();
  const float font_pixels = window.font_body_ ? window.font_body_->FontSize : px(15.0f);
  const float padding_y = std::max(6.0f * scale, (field_height - font_pixels) * 0.5f);
  layout.SetCursor(layout.content_left(), layout.StackControlY());
  if (window.font_body_) ImGui::PushFont(window.font_body_);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(12.0f), padding_y));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, px(1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                      ::minimize::ui::theme::Metrics::kControlRounding * scale);
  ImGui::PushStyleColor(ImGuiCol_FrameBg,
                        ImGui::ColorConvertU32ToFloat4(::minimize::ui::theme::kPanelHeader));
  ImGui::PushStyleColor(ImGuiCol_Border,
                        ImGui::ColorConvertU32ToFloat4(::minimize::ui::theme::kBorder));
  ImGui::SetNextItemWidth(field_width);
  ImGui::InputTextWithHint("##app_search", "Filter apps...", window.exclusion_input_.data(),
                           window.exclusion_input_.size());
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(3);
  if (window.font_body_) ImGui::PopFont();
  layout.EndRow();
  layout.EndGroup();

  const std::string caption = std::format("{} APPS", filtered.size());
  layout.SectionCaption(window.font_small_, kCaptionTextSize, caption.c_str());
  const float toggle_width = ::minimize::ui::theme::Metrics::kToggleWidth * scale;
  const float toggle_height = (::minimize::ui::theme::Metrics::kToggleHeight + 4.0f) * scale;
  layout.BeginGroup();
  if (filtered.empty()) {
    layout.BeginRow(::minimize::ui::theme::Metrics::kRowHeight);
    layout.RowTitle(window.font_small_, kHelperTextSize,
                    filter.empty() ? "No applications found" : "No matches", kSecondaryTextColor);
    layout.EndRow();
  }
  for (size_t index = 0; index < filtered.size(); ++index) {
    const bool inactive = !filtered[index].active;
    layout.BeginRow(inactive ? ::minimize::ui::theme::Metrics::kRowHeightTall
                             : ::minimize::ui::theme::Metrics::kRowHeight);
    layout.ReserveControl(toggle_width);
    layout.RowTitle(window.font_body_, kLabelTextSize, filtered[index].name.c_str(),
                    inactive ? kSecondaryTextColor : kPrimaryTextColor);
    if (inactive)
      layout.RowSubtitle(window.font_small_, kHelperTextSize, "inactive", kSecondaryTextColor);
    const ImVec2 toggle_cursor = layout.ControlCursor(toggle_width, toggle_height);
    layout.SetCursor(toggle_cursor.x, toggle_cursor.y);
    const std::string id = std::format("##toggle_exclude_{}", index);
    bool excluded = filtered[index].excluded;
    if (ui::components::Toggle(motion, id.c_str(), &excluded, scale, alpha)) {
      if (window.controller_->actions().SetApplicationExcluded(filtered[index].name, excluded)) {
        window.exclusion_error_.clear();
        if (excluded) {
          if (std::find(excluded_applications.begin(), excluded_applications.end(),
                        filtered[index].name) == excluded_applications.end())
            excluded_applications.push_back(filtered[index].name);
        } else {
          std::erase(excluded_applications, filtered[index].name);
        }
      } else {
        window.exclusion_error_ = "Could not update exclusion.";
      }
    }
    layout.EndRow();
  }
  layout.EndGroup();

  const std::string& error =
      window.persistence_error_.empty() ? window.exclusion_error_ : window.persistence_error_;
  if (error.empty()) return;
  const ImVec2 position = layout.ToScreen(layout.content_left(), layout.y());
  ImGui::GetWindowDrawList()->AddText(
      window.font_small_, window.font_small_->FontSize,
      ImVec2(std::floor(position.x + 0.5f), std::floor(position.y + 0.5f)),
      ::minimize::ui::theme::WithAlpha(IM_COL32(235, 120, 120, 255), alpha), error.c_str());
  layout.Gap(22.0f);
}

}  // namespace minimize::ui::pages
