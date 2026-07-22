#include "pch.hpp"

#include "ui/components/page_layout.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "ui/theme/theme_tokens.hpp"

namespace minimize::ui::components {
using namespace ::minimize::ui::theme;
using ::minimize::ui::motion::MotionContext;

PageLayout::PageLayout(ImDrawList* draw, ImVec2 origin, float page_width, float scale, float alpha,
                       float y_start, MotionContext* motion, const char* page_scope)
    : draw_(draw),
      origin_(origin),
      page_width_(page_width),
      scale_(scale),
      alpha_(alpha),
      y_(y_start),
      motion_(motion),
      page_scope_(page_scope ? page_scope : "page") {}

float PageLayout::Reveal(const char* id, float delay, float from_offset_px) {
  if (!motion_) {
    local_alpha_ = 1.0f;
    motion_y_shift_ = 0.0f;
    return 1.0f;
  }
  const ui::motion::MotionSpec spec =
      ui::motion::MotionSpec::Timed(0.40f, ui::motion::MotionEasing::kSmootherStep, delay);
  const float reveal = motion_->system.AnimateValue(
      ui::motion::MotionKey("layout", page_scope_, id), 1.0f, spec, 0.0f);
  local_alpha_ = std::clamp(reveal, 0.0f, 1.0f);
  motion_y_shift_ = (1.0f - local_alpha_) * from_offset_px * scale_;
  return local_alpha_;
}

void PageLayout::Gap(float logical_px) { y_ += logical_px * scale_; }

void PageLayout::AdvanceScaled(float scaled_px) { y_ += scaled_px; }

void PageLayout::Title(ImFont* title_font, float title_size, const char* title, ImFont* sub_font,
                       float sub_size, const char* subtitle, float right_reserve) {
  (void)title_size;
  (void)sub_size;
  Reveal("title", 0.0f, 14.0f);
  const float x = Metrics::kPageInset * scale_;
  const float max_w = std::max(40.0f * scale_, group_right() - x - right_reserve);
  // Always draw at the font's baked size — scaling a glyph atlas is what makes text soft.
  DrawClippedText(title_font, title_font ? title_font->FontSize : 0.0f, x, y_, max_w, kText, title);
  y_ += (title_font ? title_font->FontSize : 0.0f) + 4.0f * scale_;
  if (subtitle && sub_font) {
    DrawClippedText(sub_font, sub_font->FontSize, x, y_, max_w, kMutedText, subtitle);
    y_ += sub_font->FontSize + 14.0f * scale_;
  } else {
    y_ += 12.0f * scale_;
  }
  // Reset local shift so following content stages independently.
  local_alpha_ = 1.0f;
  motion_y_shift_ = 0.0f;
}

void PageLayout::SectionCaption(ImFont* font, float size, const char* text) {
  (void)size;
  if (in_group_) EndGroup();
  y_ += Metrics::kSectionCaptionGap * scale_;
  const std::string reveal_id = std::format("caption{}", reveal_serial_++);
  Reveal(reveal_id.c_str(), 0.02f * static_cast<float>(reveal_serial_), 8.0f);
  const float font_size = font ? font->FontSize : 0.0f;
  const ImVec2 pos = ToScreen(group_left() + 4.0f * scale_, y_);
  draw_->AddText(font, font_size, ImVec2(std::floor(pos.x + 0.5f), std::floor(pos.y + 0.5f)),
                 WithAlpha(kMutedText, alpha()), text);
  y_ += font_size + 7.0f * scale_;
  local_alpha_ = 1.0f;
  motion_y_shift_ = 0.0f;
}

void PageLayout::BeginGroup() {
  if (in_group_) EndGroup();
  in_group_ = true;
  group_start_ = y_;
  group_row_ = 0;
  const std::string reveal_id = std::format("group{}", reveal_serial_++);
  Reveal(reveal_id.c_str(), 0.045f * static_cast<float>(reveal_serial_), 16.0f);
  draw_->ChannelsSplit(2);
  draw_->ChannelsSetCurrent(1);
}

void PageLayout::EndGroup() {
  if (!in_group_) return;
  if (row_open_) EndRow();
  draw_->ChannelsSetCurrent(0);
  DrawCard(draw_, ToScreen(group_left(), group_start_), ToScreen(group_right(), y_), scale_,
           alpha());
  draw_->ChannelsMerge();
  in_group_ = false;
  group_row_ = 0;
  local_alpha_ = 1.0f;
  motion_y_shift_ = 0.0f;
  y_ += Metrics::kGroupSpacing * scale_;
}

void PageLayout::BeginRow(float logical_height) {
  if (!in_group_) BeginGroup();
  if (row_open_) EndRow();
  row_top_ = y_;
  row_height_ = logical_height * scale_;
  row_open_ = true;
  stack_row_ = false;
  reserved_control_w_ = 0.0f;
  dual_line_ = false;
  dual_title_box_y_ = 0.0f;
  dual_title_box_h_ = 0.0f;
  dual_sub_box_y_ = 0.0f;
  dual_sub_box_h_ = 0.0f;
  if (group_row_ > 0) {
    DrawSeparator(draw_, ToScreen(content_left(), row_top_), ToScreen(content_right(), row_top_),
                  alpha() * 0.85f);
  }
}

void PageLayout::BeginStackRow(float title_band_logical, float control_band_logical) {
  // No title band → no title gap. Equal top/bottom padding so cards don't look lopsided
  // (was: padTop + titleGap(8) even when title=0, then only padBottom → uneven).
  const bool has_title = title_band_logical > 0.01f;
  stack_pad_top_ = Metrics::kStackPadTop * scale_;
  stack_gap_ = has_title ? Metrics::kStackTitleGap * scale_ : 0.0f;
  stack_title_band_ = has_title ? title_band_logical * scale_ : 0.0f;
  stack_control_band_ = control_band_logical * scale_;
  const float total = Metrics::kStackPadTop + (has_title ? title_band_logical : 0.0f) +
                      (has_title ? Metrics::kStackTitleGap : 0.0f) + control_band_logical +
                      Metrics::kStackPadBottom;
  BeginRow(total);
  stack_row_ = true;
  // Full width available for titles in stack rows.
  reserved_control_w_ = 0.0f;
}

void PageLayout::ReserveControl(float control_width_scaled) {
  reserved_control_w_ = std::max(0.0f, control_width_scaled);
}

void PageLayout::EndRow() {
  if (!row_open_) return;
  y_ = row_top_ + row_height_;
  row_open_ = false;
  stack_row_ = false;
  reserved_control_w_ = 0.0f;
  ++group_row_;
}

float PageLayout::LabelMaxWidth() const {
  if (stack_row_ || reserved_control_w_ <= 0.0f) {
    return content_width();
  }
  const float gap = Metrics::kLabelControlGap * scale_;
  return std::max(40.0f * scale_, content_width() - reserved_control_w_ - gap);
}

float PageLayout::StackControlY() const {
  return row_top_ + stack_pad_top_ + stack_title_band_ + stack_gap_;
}

float PageLayout::StackControlHeight() const { return stack_control_band_; }

void PageLayout::DrawClippedText(ImFont* font, float size, float x, float y, float max_w,
                                 ImU32 color, const char* text) const {
  if (!font || !text) return;
  // Prefer the baked atlas size; only use `size` when it matches (avoids soft scale).
  const float draw_size =
      (size > 0.0f && std::abs(size - font->FontSize) < 0.01f) ? size : font->FontSize;
  const ImVec2 min = ToScreen(x, y);
  const ImVec2 snapped(std::floor(min.x + 0.5f), std::floor(min.y + 0.5f));
  const ImVec2 max(snapped.x + max_w + 1.0f, snapped.y + draw_size + 4.0f * scale_);
  draw_->PushClipRect(snapped, max, true);
  draw_->AddText(font, draw_size, snapped, WithAlpha(color, alpha()), text);
  draw_->PopClipRect();
}

void PageLayout::RowTitle(ImFont* font, float size, const char* text, ImU32 color) {
  (void)size;
  if (!font) return;
  const float text_h = font->FontSize;
  const float max_w = LabelMaxWidth();
  float text_y;
  if (stack_row_) {
    text_y = CenteredTextTop(font, row_top_ + stack_pad_top_, stack_title_band_);
  } else {
    // Tall/hero rows with a subtitle: pack title+subtitle as one block and
    // center that block in the row (not title at top / subtitle at bottom).
    const bool dual = row_height_ >= Metrics::kRowHeightTall * scale_ * 0.92f;
    if (dual) {
      dual_line_ = true;
      const float sub_h = text_h * 0.87f;  // helper is typically ~13 vs 15 label
      const float line_gap = 3.0f * scale_;
      const float block_h = text_h + line_gap + sub_h;
      const float block_top = row_top_ + (row_height_ - block_h) * 0.5f;
      dual_title_box_y_ = block_top;
      dual_title_box_h_ = text_h;
      dual_sub_box_y_ = block_top + text_h + line_gap;
      dual_sub_box_h_ = sub_h;
      text_y = CenteredTextTop(font, dual_title_box_y_, dual_title_box_h_);
    } else {
      dual_line_ = false;
      text_y = CenteredTextTop(font, row_top_, row_height_);
    }
  }
  DrawClippedText(font, text_h, content_left(), text_y, max_w, color, text);
}

void PageLayout::RowSubtitle(ImFont* font, float size, const char* text, ImU32 color) {
  (void)size;
  if (stack_row_ || !font) return;  // stack rows only use the title band
  const float text_h = font->FontSize;
  const float max_w = LabelMaxWidth();
  float text_y;
  if (dual_line_) {
    // Same block as RowTitle — do not re-center independently.
    text_y = CenteredTextTop(font, dual_sub_box_y_, dual_sub_box_h_);
  } else {
    // Fallback if subtitle is drawn without a dual title pass.
    const float band_top = row_top_ + row_height_ - text_h - 10.0f * scale_;
    text_y = CenteredTextTop(font, band_top, text_h + 4.0f * scale_);
  }
  DrawClippedText(font, text_h, content_left(), text_y, max_w, color, text);
}

void PageLayout::RowValue(ImFont* font, float size, const char* text, ImU32 color) {
  (void)size;
  if (!font || !text) return;
  const float text_h = font->FontSize;
  const float max_w = reserved_control_w_ > 0.0f ? reserved_control_w_ : content_width() * 0.58f;
  const ImVec2 measured = font->CalcTextSizeA(text_h, FLT_MAX, 0.0f, text);
  const float draw_w = std::min(measured.x, max_w);
  const float x = content_right() - draw_w;
  const float y = CenteredTextTop(font, row_top_, row_height_);
  const ImVec2 clip_min = ToScreen(content_right() - max_w, row_top_);
  const ImVec2 clip_max = ToScreen(content_right(), row_top_ + row_height_);
  const ImVec2 pos = ToScreen(x, y);
  const ImVec2 snapped(std::floor(pos.x + 0.5f), std::floor(pos.y + 0.5f));
  draw_->PushClipRect(ImVec2(std::floor(clip_min.x), std::floor(clip_min.y)),
                      ImVec2(std::ceil(clip_max.x), std::ceil(clip_max.y)), true);
  draw_->AddText(font, text_h, snapped, WithAlpha(color, alpha()), text);
  draw_->PopClipRect();
}

ImVec2 PageLayout::ControlCursor(float control_width, float control_height) const {
  if (stack_row_) {
    return ImVec2(content_left(), StackControlY() + (StackControlHeight() - control_height) * 0.5f);
  }
  const float x = content_right() - control_width;
  const float y = row_top_ + (row_height_ - control_height) * 0.5f;
  return ImVec2(x, y);
}

float PageLayout::ControlMaxWidth(float preferred_logical) const {
  if (stack_row_) return content_width();
  const float preferred = preferred_logical * scale_;
  const float min_label = Metrics::kMinLabelWidth * scale_;
  const float gap = Metrics::kLabelControlGap * scale_;
  const float available = std::max(80.0f * scale_, content_width() - min_label - gap);
  return std::min(preferred, available);
}

ImVec2 PageLayout::ToScreen(float x, float y) const {
  return ImVec2(origin_.x + x, origin_.y + y + motion_y_shift_ - ImGui::GetScrollY());
}

void PageLayout::SetCursor(float x, float y) const {
  // Keep interactive widgets locked to the animated group offset.
  ImGui::SetCursorPos(ImVec2(x, y + motion_y_shift_));
}

}  // namespace minimize::ui::components
