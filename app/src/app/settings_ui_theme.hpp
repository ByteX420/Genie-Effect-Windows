#pragma once

#include <cmath>

#include "imgui.h"
#include "menu/motion/motion_context.hpp"

namespace genie::app::settings_ui {

struct MotionContext {
  ::ui::motion::MotionSystem& system;
  const ::ui::motion::MotionTokens& tokens;
};

// Layout metrics — logical (1x DPI) units. Everything scales via ui_scale.
//
// Typography weight rule (Inter):
//   Regular  — labels, helpers, captions, buttons, combos, nav, values
//   SemiBold — page titles, hero/section titles, product name, active emphasis
struct Metrics final {
  static constexpr float kWindowWidth = 800.0f;
  static constexpr float kWindowHeight = 580.0f;
  static constexpr float kWindowRounding = 12.0f;
  static constexpr float kTitlebarHeight = 46.0f;

  static constexpr float kSidebarWidth = 172.0f;
  // Single left inset for brand, nav, and status (one vertical column).
  static constexpr float kSidebarMargin = 12.0f;
  static constexpr float kSidebarContentWidth = 148.0f;
  static constexpr float kSidebarBrandY = 52.0f;
  static constexpr float kNavigationY = 78.0f;
  static constexpr float kNavigationRowHeight = 32.0f;
  static constexpr float kNavigationSpacing = 1.0f;
  // Mirror traffic-light top edge inset (ampel top ≈ kSidebarMargin) at the bottom.
  static constexpr float kSidebarStatusBottom = kSidebarMargin;

  static constexpr float kPageInset = 22.0f;
  static constexpr float kCardInnerInset = 14.0f;
  static constexpr float kContentInset = kPageInset + kCardInnerInset;
  static constexpr float kLabelControlGap = 16.0f;
  static constexpr float kRelatedSpacing = 8.0f;
  static constexpr float kGroupSpacing = 16.0f;
  // No artificial gutter — scrollbar sits on the content pane's right edge.
  static constexpr float kScrollGutter = 0.0f;
  static constexpr float kScrollBottomPadding = 28.0f;
  static constexpr float kScrollFadeHeight = 18.0f;
  static constexpr float kMainInset = kPageInset;
  static constexpr float kMainTop = 48.0f;
  static constexpr float kCardWidth = 600.0f;
  static constexpr float kCardRounding = 14.0f;
  static constexpr float kControlRounding = 10.0f;
  static constexpr float kCardSpacing = kGroupSpacing;

  // Standard rows (label | control side-by-side).
  static constexpr float kRowHeight = 48.0f;
  static constexpr float kRowHeightTall = 62.0f;
  static constexpr float kRowHeightHero = 72.0f;
  static constexpr float kRowHeightDense = 38.0f;

  // Stack rows: title on top, full-width control below.
  static constexpr float kStackPadTop = 10.0f;
  static constexpr float kStackPadBottom = 10.0f;
  static constexpr float kStackTitleGap = 8.0f;

  static constexpr float kSectionCaptionGap = 6.0f;
  static constexpr float kTitleBlockHeight = 48.0f;

  // Control sizes — compact next to larger type.
  static constexpr float kToggleWidth = 42.0f;
  static constexpr float kToggleHeight = 26.0f;
  static constexpr float kButtonHeight = 32.0f;
  static constexpr float kComboHeight = 30.0f;
  static constexpr float kSegmentHeight = 32.0f;
  static constexpr float kSliderHeight = 22.0f;
  static constexpr float kMinLabelWidth = 132.0f;
};

inline constexpr ImU32 kMainBackground = IM_COL32(11, 11, 12, 255);
inline constexpr ImU32 kSidebarBackground = IM_COL32(13, 13, 14, 255);
inline constexpr ImU32 kCardBackground = IM_COL32(20, 20, 22, 255);
inline constexpr ImU32 kPanelHeader = IM_COL32(26, 26, 28, 255);
inline constexpr ImU32 kActiveItem = IM_COL32(232, 232, 236, 255);
inline constexpr ImU32 kText = IM_COL32(236, 236, 238, 255);
inline constexpr ImU32 kMutedText = IM_COL32(148, 148, 156, 255);
inline constexpr ImU32 kBorder = IM_COL32(42, 42, 45, 255);
inline constexpr ImU32 kSeparator = IM_COL32(38, 38, 42, 255);

inline ImU32 WithAlpha(ImU32 color, float alpha) {
  alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
  const auto value = static_cast<ImU32>(alpha * 255.0f + 0.5f);
  return (color & 0x00ffffffu) | (value << 24u);
}

inline ImU32 Blend(ImU32 from, ImU32 to, float amount) {
  amount = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
  const ImVec4 a = ImGui::ColorConvertU32ToFloat4(from);
  const ImVec4 b = ImGui::ColorConvertU32ToFloat4(to);
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount, a.z + (b.z - a.z) * amount,
             a.w + (b.w - a.w) * amount));
}

// Vertical top for ImGui AddText so the glyph ink is optically centered in a box.
// ImGui places the baseline at pos.y + Ascent; Descent is typically negative.
inline float CenteredTextTop(ImFont* font, float box_min_y, float box_height) {
  if (!font) return box_min_y;
  const float center = box_min_y + box_height * 0.5f;
  const float top = center - (font->Ascent - font->Descent) * 0.5f;
  return std::floor(top + 0.5f);
}

inline float CenteredTextTop(ImFont* font, float font_size, float box_min_y, float box_height) {
  if (!font || font->FontSize <= 0.0f) return CenteredTextTop(font, box_min_y, box_height);
  const float scale = font_size / font->FontSize;
  const float center = box_min_y + box_height * 0.5f;
  const float top = center - (font->Ascent - font->Descent) * scale * 0.5f;
  return std::floor(top + 0.5f);
}

enum class TrafficLightAction {
  kNone,
  kClose,
  kMinimize,
  kZoom,
};

// Flow layout for grouped settings pages.
// Side-by-side rows reserve control width so labels never collide.
// Stack rows place a full-width control under the title.
// When a MotionContext is provided, titles/groups/captions stagger-reveal.
class PageLayout final {
public:
  PageLayout(ImDrawList* draw, ImVec2 origin, float page_width, float scale, float alpha,
             float y_start, MotionContext* motion = nullptr, const char* page_scope = "page");

  [[nodiscard]] float scale() const { return scale_; }
  [[nodiscard]] float alpha() const { return alpha_ * local_alpha_; }
  [[nodiscard]] float page_width() const { return page_width_; }
  [[nodiscard]] float y() const { return y_; }
  [[nodiscard]] float content_bottom() const { return y_; }
  [[nodiscard]] float content_left() const { return Metrics::kContentInset * scale_; }
  [[nodiscard]] float content_right() const {
    return page_width_ - Metrics::kContentInset * scale_;
  }
  [[nodiscard]] float content_width() const { return content_right() - content_left(); }
  [[nodiscard]] float group_left() const { return Metrics::kPageInset * scale_; }
  [[nodiscard]] float group_right() const { return page_width_ - Metrics::kPageInset * scale_; }
  // Layout-space Y shift currently applied by the active group enter animation.
  [[nodiscard]] float motion_y_shift() const { return motion_y_shift_; }

  // Logical gap (multiplied by scale).
  void Gap(float logical_px);
  // Already-scaled advance (for measured child heights).
  void AdvanceScaled(float scaled_px);

  // right_reserve: keep free space on the title line for header actions.
  void Title(ImFont* title_font, float title_size, const char* title, ImFont* sub_font = nullptr,
             float sub_size = 0.0f, const char* subtitle = nullptr, float right_reserve = 0.0f);
  void SectionCaption(ImFont* font, float size, const char* text);

  void BeginGroup();
  void EndGroup();

  // Side-by-side row. Call ReserveControl before drawing titles.
  void BeginRow(float logical_height);
  void ReserveControl(float control_width_scaled);
  void EndRow();

  // Stacked row: title band then full-width control band.
  void BeginStackRow(float title_band_logical, float control_band_logical);
  [[nodiscard]] float StackControlY() const;
  [[nodiscard]] float StackControlHeight() const;

  void RowTitle(ImFont* font, float size, const char* text, ImU32 color);
  void RowSubtitle(ImFont* font, float size, const char* text, ImU32 color);
  // Right-aligned value (diagnostics). Clipped to the control zone.
  void RowValue(ImFont* font, float size, const char* text, ImU32 color);

  [[nodiscard]] ImVec2 ControlCursor(float control_width, float control_height) const;
  // Preferred logical width, clamped to free space after min label column.
  [[nodiscard]] float ControlMaxWidth(float preferred_logical) const;
  [[nodiscard]] float LabelMaxWidth() const;
  [[nodiscard]] float RowTop() const { return row_top_; }
  [[nodiscard]] float RowHeight() const { return row_height_; }
  [[nodiscard]] float RowCenterY() const { return row_top_ + row_height_ * 0.5f; }
  [[nodiscard]] bool IsStackRow() const { return stack_row_; }

  [[nodiscard]] ImVec2 ToScreen(float x, float y) const;
  void SetCursor(float x, float y) const;

private:
  void DrawClippedText(ImFont* font, float size, float x, float y, float max_w, ImU32 color,
                       const char* text) const;
  float Reveal(const char* id, float delay, float from_offset_px);

  ImDrawList* draw_ = nullptr;
  ImVec2 origin_{};
  float page_width_ = 0.0f;
  float scale_ = 1.0f;
  float alpha_ = 1.0f;
  float local_alpha_ = 1.0f;
  float motion_y_shift_ = 0.0f;
  float y_ = 0.0f;
  MotionContext* motion_ = nullptr;
  const char* page_scope_ = "page";
  int reveal_serial_ = 0;
  bool in_group_ = false;
  float group_start_ = 0.0f;
  int group_row_ = 0;
  float row_top_ = 0.0f;
  float row_height_ = 0.0f;
  bool row_open_ = false;
  bool stack_row_ = false;
  float stack_title_band_ = 0.0f;
  float stack_control_band_ = 0.0f;
  float stack_pad_top_ = 0.0f;
  float stack_gap_ = 0.0f;
  float reserved_control_w_ = 0.0f;
  // Dual-line rows: title+subtitle packed as a centered block (not pinned top/bottom).
  bool dual_line_ = false;
  float dual_title_box_y_ = 0.0f;
  float dual_title_box_h_ = 0.0f;
  float dual_sub_box_y_ = 0.0f;
  float dual_sub_box_h_ = 0.0f;
};

void ApplyStyle(float scale);
void DrawGradientShadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float radius, float alpha,
                        float scale);
void DrawCard(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, float alpha = 1.0f);
void DrawSeparator(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha = 1.0f);
TrafficLightAction DrawTrafficLights(const MotionContext& motion, ImVec2 window_origin, float scale,
                                     float alpha = 1.0f);
// regular = unselected, emphasis = selected (SemiBold). Pass the same font twice if unused.
bool SidebarItem(const MotionContext& motion, const char* id, const char* label, bool selected,
                 ImVec2 position, ImVec2 size, ImFont* regular, ImFont* emphasis, float scale,
                 float alpha = 1.0f);

}  // namespace genie::app::settings_ui
