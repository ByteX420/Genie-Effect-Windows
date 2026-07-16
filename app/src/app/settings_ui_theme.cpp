#include "pch.hpp"

#include "app/settings_ui_theme.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "menu/theme.hpp"

namespace genie::app::settings_ui {
namespace {

ImU32 Alpha(ImU32 color, float alpha) {
  const auto value = static_cast<ImU32>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
  return (color & 0x00ffffffu) | (value << 24u);
}

ImU32 Mix(ImU32 from, ImU32 to, float amount) {
  const ImVec4 a = ImGui::ColorConvertU32ToFloat4(from);
  const ImVec4 b = ImGui::ColorConvertU32ToFloat4(to);
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount, a.z + (b.z - a.z) * amount,
             a.w + (b.w - a.w) * amount));
}

// 1:1 from 795f55b2 — center-out arms, AA off (prevents double-blend halo / asymmetry).
void DrawSymmetricX(ImDrawList* draw, const ImVec2& min, float size, ImU32 color, float scale) {
  const ImVec2 center(min.x + size * 0.5f - 0.5f, min.y + size * 0.5f - 0.5f);
  const float arm_length = 4.0f * scale;
  const float thickness = 1.0f;

  const ImDrawListFlags old_flags = draw->Flags;
  draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;

  draw->AddLine(center, ImVec2(center.x - arm_length, center.y - arm_length), color, thickness);
  draw->AddLine(center, ImVec2(center.x + arm_length, center.y + arm_length), color, thickness);
  draw->AddLine(center, ImVec2(center.x + arm_length, center.y - arm_length), color, thickness);
  draw->AddLine(center, ImVec2(center.x - arm_length, center.y + arm_length), color, thickness);

  draw->Flags = old_flags;
}

}  // namespace

void ApplyStyle(float scale) {
  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle();
  ImGui::StyleColorsDark(&style);
  style.WindowPadding = ImVec2(0.0f, 0.0f);
  style.WindowRounding = Metrics::kWindowRounding * scale;
  style.WindowBorderSize = 0.0f;
  style.ChildRounding = Metrics::kControlRounding * scale;
  style.ChildBorderSize = 0.0f;
  style.PopupRounding = Metrics::kControlRounding * scale;
  style.PopupBorderSize = 1.0f * scale;
  style.FramePadding = ImVec2(12.0f * scale, 8.0f * scale);
  style.FrameRounding = Metrics::kControlRounding * scale;
  style.FrameBorderSize = 1.0f * scale;
  style.ItemSpacing = ImVec2(10.0f * scale, 8.0f * scale);
  style.ItemInnerSpacing = ImVec2(8.0f * scale, 6.0f * scale);
  // Slim edge scrollbar — sits flush on the content pane.
  style.ScrollbarSize = 8.0f * scale;
  style.ScrollbarRounding = 4.0f * scale;
  style.GrabMinSize = 8.0f * scale;
  style.GrabRounding = 3.0f * scale;
  style.TabRounding = Metrics::kControlRounding * scale;
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;
  style.Colors[ImGuiCol_WindowBg] = colors::main;
  style.Colors[ImGuiCol_ChildBg] = colors::panel;
  style.Colors[ImGuiCol_PopupBg] = colors::comboBg;
  style.Colors[ImGuiCol_Text] = colors::text;
  style.Colors[ImGuiCol_TextDisabled] = colors::textDim;
  style.Colors[ImGuiCol_Border] = colors::border;
  style.Colors[ImGuiCol_FrameBg] = colors::comboBg;
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
  style.Colors[ImGuiCol_FrameBgActive] = colors::panelHeader;
  style.Colors[ImGuiCol_Button] = colors::panelHeader;
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
  style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
  style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
  style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
  style.Colors[ImGuiCol_CheckMark] = colors::accent;
  style.Colors[ImGuiCol_SliderGrab] = colors::accent;
  style.Colors[ImGuiCol_SliderGrabActive] = colors::text;
  style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
  style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.24f);
  style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.34f);
  // Soft dark veil behind license / other modals (not the default washed-out grey).
  style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
}

void DrawGradientShadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float radius, float alpha,
                        float scale) {
  (void)radius;
  (void)scale;
  draw->AddRect(ImVec2(min.x - 0.5f, min.y - 0.5f), ImVec2(max.x + 0.5f, max.y + 0.5f),
                IM_COL32(0, 0, 0, static_cast<int>(alpha * 40.0f)), 0.0f, 0, 1.0f);
}

void DrawCard(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, float alpha) {
  const float rounding = Metrics::kCardRounding * scale;
  ImVec4 panel = colors::panel;
  panel.w *= alpha;
  ImVec4 border = colors::border;
  border.w *= 0.85f * alpha;
  draw->AddRectFilled(min, max, ImGui::GetColorU32(panel), rounding);
  draw->AddRect(min, max, ImGui::GetColorU32(border), rounding, 0, std::max(1.0f, scale));
}

void DrawSeparator(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha) {
  draw->AddLine(min, max, Alpha(kSeparator, alpha * 0.7f), 1.0f);
}

TrafficLightAction DrawTrafficLights(const MotionContext& motion, ImVec2 window_origin, float scale,
                                     float alpha) {
  constexpr const char* ids[] = {"##traffic_close", "##traffic_minimize", "##traffic_zoom"};
  // Compact macOS-style dots, left-aligned with the nav column.
  const float inset = Metrics::kSidebarMargin * scale;
  const float half_size = 6.0f * scale;
  const float spacing = 20.0f * scale;
  const ImVec2 base(window_origin.x + inset + half_size + 2.0f * scale,
                    window_origin.y + 18.0f * scale);
  bool clicked[3]{};
  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (int index = 0; index < 3; ++index) {
    const ImVec2 center(base.x + spacing * static_cast<float>(index), base.y);
    const float hit_pad = 5.0f * scale;
    const ImVec2 hit_min(center.x - half_size - hit_pad, center.y - half_size - hit_pad);
    ImGui::SetCursorScreenPos(hit_min);
    ImGui::InvisibleButton(ids[index],
                           ImVec2((half_size + hit_pad) * 2.0f, (half_size + hit_pad) * 2.0f));
    // Allow hover even if a (legacy) popup is up — combo itself is non-modal now.
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup |
                                              ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool pressed = ImGui::IsItemActive();
    clicked[index] = ImGui::IsItemClicked();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const float hover =
        motion.system.value(::ui::motion::MotionKey("window_controls", ids[index], "hover"),
                            hovered ? 1.0f : 0.0f, motion.tokens.hoverFast);
    const float press =
        motion.system.value(::ui::motion::MotionKey("window_controls", ids[index], "press"),
                            pressed ? 1.0f : 0.0f, motion.tokens.pressFast);
    const float grow = 1.0f + 0.08f * hover - 0.06f * press;
    const float r = half_size * grow;
    // Quiet idle dots; only tint on hover (close → red, others → slight lift).
    const ImU32 idle = IM_COL32(58, 58, 62, 255);
    const ImU32 hover_col =
        index == 0 ? IM_COL32(200, 72, 72, 255)
                   : (index == 1 ? IM_COL32(200, 160, 70, 255) : IM_COL32(90, 170, 100, 255));
    draw->AddCircleFilled(center, r, Alpha(Mix(idle, hover_col, hover), alpha), 24);
    if (hover > 0.4f) {
      // Glyphs only appear on hover — keeps the rail calm at rest.
      const ImU32 icon = Alpha(IM_COL32(30, 30, 32, 255), alpha * hover);
      if (index == 0) {
        const float icon_size = 10.0f * scale;
        DrawSymmetricX(draw, ImVec2(center.x - icon_size * 0.5f, center.y - icon_size * 0.5f),
                       icon_size, icon, scale * 0.7f);
      } else if (index == 1) {
        const ImDrawListFlags old_flags = draw->Flags;
        draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;
        draw->AddLine(ImVec2(center.x - 3.0f * scale, center.y),
                      ImVec2(center.x + 3.0f * scale, center.y), icon, 1.0f);
        draw->Flags = old_flags;
      } else {
        draw->AddRect(ImVec2(center.x - 2.4f * scale, center.y - 2.4f * scale),
                      ImVec2(center.x + 2.4f * scale, center.y + 2.4f * scale), icon, 1.0f * scale, 0,
                      1.0f);
      }
    }
  }
  if (clicked[0]) return TrafficLightAction::kClose;
  if (clicked[1]) return TrafficLightAction::kMinimize;
  if (clicked[2]) return TrafficLightAction::kZoom;
  return TrafficLightAction::kNone;
}

PageLayout::PageLayout(ImDrawList* draw, ImVec2 origin, float page_width, float scale, float alpha,
                       float y_start, MotionContext* motion, const char* page_scope)
    : draw_(draw), origin_(origin), page_width_(page_width), scale_(scale), alpha_(alpha),
      y_(y_start), motion_(motion), page_scope_(page_scope ? page_scope : "page") {}

float PageLayout::Reveal(const char* id, float delay, float from_offset_px) {
  if (!motion_) {
    local_alpha_ = 1.0f;
    motion_y_shift_ = 0.0f;
    return 1.0f;
  }
  const ::ui::motion::MotionSpec spec =
      ::ui::motion::MotionSpec::Timed(0.40f, ::ui::motion::MotionEasing::SmootherStep, delay);
  const float reveal = motion_->system.value(
      ::ui::motion::MotionKey("layout", page_scope_, id), 1.0f, spec, 0.0f);
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
  draw_->AddText(font, font_size,
                 ImVec2(std::floor(pos.x + 0.5f), std::floor(pos.y + 0.5f)),
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
  const float max_w =
      reserved_control_w_ > 0.0f ? reserved_control_w_ : content_width() * 0.58f;
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

bool SidebarItem(const MotionContext& motion, const char* id, const char* label, bool selected,
                 ImVec2 position, ImVec2 size, ImFont* regular, ImFont* emphasis, float scale,
                 float alpha) {
  if (!regular) regular = ImGui::GetFont();
  if (!emphasis) emphasis = regular;
  ImGui::SetCursorScreenPos(position);
  const bool pressed = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool focused = ImGui::IsItemFocused();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  ImDrawList* draw = ImGui::GetWindowDrawList();

  const float hover =
      motion.system.value(::ui::motion::MotionKey("sidebar-main", id, "hover"),
                          hovered || focused ? 1.0f : 0.0f, motion.tokens.hoverFast, 0.0f);
  const float select =
      motion.system.value(::ui::motion::MotionKey("sidebar-main", id, "select"),
                          selected ? 1.0f : 0.0f, motion.tokens.selectSharp, 0.0f);
  const float rounding = 8.0f * scale;
  if (hover > 0.001f || select > 0.001f) {
    // Quieter pill — selected a bit stronger, hover barely there.
    // Selection is the pill + weight only (no sliding rail between items).
    const float fill_alpha = 0.10f * select + 0.04f * hover * (1.0f - select);
    draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
                        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, fill_alpha * alpha)), rounding);
  }

  // Weight rule: selected nav uses SemiBold (emphasis), idle uses Regular.
  ImFont* font = selected ? emphasis : regular;
  const float font_size = font->FontSize;
  const ImVec4 text_target = selected || hovered || focused ? colors::text : colors::textDim;
  ImVec4 text_color = motion.system.color(
      ::ui::motion::MotionKey("sidebar-main", id, "text"), text_target,
      selected ? motion.tokens.selectSharp : motion.tokens.hoverFast, colors::textDim);
  text_color.w *= alpha;
  draw->AddText(font, font_size,
                ImVec2(std::floor(position.x + 14.0f * scale + 0.5f),
                       CenteredTextTop(font, position.y, size.y)),
                ImGui::GetColorU32(text_color), label);
  return pressed;
}

}  // namespace genie::app::settings_ui
