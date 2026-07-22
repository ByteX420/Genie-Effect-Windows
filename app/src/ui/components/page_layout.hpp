#pragma once

#include "ui/theme/theme.hpp"

namespace minimize::ui::components {

class PageLayout final {
public:
  PageLayout(ImDrawList* draw, ImVec2 origin, float page_width, float scale, float alpha,
             float y_start, ::minimize::ui::motion::MotionContext* motion = nullptr,
             const char* page_scope = "page");

  [[nodiscard]] float scale() const { return scale_; }
  [[nodiscard]] float alpha() const { return alpha_ * local_alpha_; }
  [[nodiscard]] float page_width() const { return page_width_; }
  [[nodiscard]] float y() const { return y_; }
  [[nodiscard]] float content_bottom() const { return y_; }
  [[nodiscard]] float content_left() const {
    return ::minimize::ui::theme::Metrics::kContentInset * scale_;
  }
  [[nodiscard]] float content_right() const {
    return page_width_ - ::minimize::ui::theme::Metrics::kContentInset * scale_;
  }
  [[nodiscard]] float content_width() const { return content_right() - content_left(); }
  [[nodiscard]] float group_left() const {
    return ::minimize::ui::theme::Metrics::kPageInset * scale_;
  }
  [[nodiscard]] float group_right() const {
    return page_width_ - ::minimize::ui::theme::Metrics::kPageInset * scale_;
  }
  [[nodiscard]] float motion_y_shift() const { return motion_y_shift_; }

  void Gap(float logical_px);
  void AdvanceScaled(float scaled_px);
  void Title(ImFont* title_font, float title_size, const char* title, ImFont* sub_font = nullptr,
             float sub_size = 0.0f, const char* subtitle = nullptr, float right_reserve = 0.0f);
  void SectionCaption(ImFont* font, float size, const char* text);
  void BeginGroup();
  void EndGroup();
  void BeginRow(float logical_height);
  void ReserveControl(float control_width_scaled);
  void EndRow();
  void BeginStackRow(float title_band_logical, float control_band_logical);
  [[nodiscard]] float StackControlY() const;
  [[nodiscard]] float StackControlHeight() const;
  void RowTitle(ImFont* font, float size, const char* text, ImU32 color);
  void RowSubtitle(ImFont* font, float size, const char* text, ImU32 color);
  void RowValue(ImFont* font, float size, const char* text, ImU32 color);
  [[nodiscard]] ImVec2 ControlCursor(float control_width, float control_height) const;
  [[nodiscard]] float ControlMaxWidth(float preferred_logical) const;
  [[nodiscard]] float LabelMaxWidth() const;
  [[nodiscard]] float RowTop() const { return row_top_; }
  [[nodiscard]] float RowHeight() const { return row_height_; }
  [[nodiscard]] float RowCenterY() const { return row_top_ + row_height_ * 0.5f; }
  [[nodiscard]] bool IsStackRow() const { return stack_row_; }
  [[nodiscard]] ImVec2 ToScreen(float x, float y) const;
  void SetCursor(float x, float y) const;

private:
  void DrawClippedText(ImFont* font, float size, float x, float y, float max_width, ImU32 color,
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
  ::minimize::ui::motion::MotionContext* motion_ = nullptr;
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
  bool dual_line_ = false;
  float dual_title_box_y_ = 0.0f;
  float dual_title_box_h_ = 0.0f;
  float dual_sub_box_y_ = 0.0f;
  float dual_sub_box_h_ = 0.0f;
};

}  // namespace minimize::ui::components
