#include "pch.hpp"

#include "app/settings_ui_widgets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#include "app/settings_ui_theme.hpp"
#include "menu/motion/motion_context.hpp"
#include "menu/theme.hpp"

namespace genie::app::settings_ui {
namespace {

std::unordered_map<ImGuiID, std::array<char, 64>> g_slider_value_buffers;
std::unordered_map<ImGuiID, bool> g_combo_was_open;
std::unordered_map<ImGuiID, bool> g_combo_closing;

::ui::motion::MotionKey ReferenceMotionKey(const char* scope, const char* id, const char* channel) {
  return ::ui::motion::MotionKey(scope, id ? id : "", channel);
}

ImVec4 MixColor(const ImVec4& from, const ImVec4& to, float amount) {
  return ImVec4(from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount,
                from.z + (to.z - from.z) * amount, from.w + (to.w - from.w) * amount);
}

float ControlRounding(float scale) { return Metrics::kControlRounding * scale; }

bool ReferenceButton(const char* id, const char* label, const ImVec2& size, ImFont* font,
                     float alpha, bool active) {
  if (!font) font = ImGui::GetFont();
  const ImVec2 position = ImGui::GetCursorScreenPos();
  ImGui::PushID(id);
  const bool clicked = ImGui::InvisibleButton("##button", size);
  const bool hovered = ImGui::IsItemHovered();
  const bool pressed = ImGui::IsItemActive();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

  auto& motion = WindowMotion::System();
  const auto& tokens = WindowMotion::Tokens();
  // Hover + press only — press drives a centered shrink via the motion system.
  const float hover = motion.value(ReferenceMotionKey("menu.button", id, "hover"),
                                   hovered ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
  const float press = motion.value(ReferenceMotionKey("menu.button", id, "press"),
                                   pressed ? 1.0f : 0.0f, tokens.pressFast, 0.0f);
  const float selected = motion.value(ReferenceMotionKey("menu.button", id, "on_state"),
                                      active ? 1.0f : 0.0f, tokens.selectSharp, 0.0f);

  // Quiet idle surface; selected reads as a solid lifted pill (not a faint tint).
  const ImVec4 base =
      MixColor(ImVec4(0.10f, 0.10f, 0.11f, 1.0f), ImVec4(0.22f, 0.22f, 0.24f, 1.0f), selected);
  const ImVec4 hovered_background =
      MixColor(base, ImVec4(0.15f, 0.15f, 0.16f, 1.0f), hover * (1.0f - selected * 0.5f));
  ImVec4 background = MixColor(hovered_background, ImVec4(0.18f, 0.18f, 0.19f, 1.0f), press);
  background.w *= alpha;

  // Smooth press shrink (hit target stays full size; only the drawn pill scales).
  const float visual_scale = 1.0f + 0.015f * hover - 0.06f * press;
  const ImVec2 center(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
  const float half_w = size.x * 0.5f * visual_scale;
  const float half_h = size.y * 0.5f * visual_scale;
  const ImVec2 visual_min(center.x - half_w, center.y - half_h);
  const ImVec2 visual_max(center.x + half_w, center.y + half_h);
  const float rounding =
      std::min(Metrics::kControlRounding * (size.y / 34.0f), size.y * 0.35f) * visual_scale;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(visual_min, visual_max, ImGui::GetColorU32(background), rounding);
  ImVec4 border = MixColor(ImVec4(0.20f, 0.20f, 0.22f, 1.0f), ImVec4(0.48f, 0.48f, 0.52f, 1.0f),
                           selected * 0.75f + hover * 0.25f * (1.0f - selected));
  border.w *= alpha;
  draw->AddRect(visual_min, visual_max, ImGui::GetColorU32(border), rounding, 0,
                std::max(1.0f, size.y / 30.0f));

  const float font_size = font->FontSize;
  ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, label);
  float final_size = font_size;
  if (text_size.x > size.x - 10.0f) {
    final_size = font_size * 0.9f;
    text_size = font->CalcTextSizeA(final_size, FLT_MAX, 0.0f, label);
  }
  final_size *= visual_scale;
  text_size = font->CalcTextSizeA(final_size, FLT_MAX, 0.0f, label);
  // Selected / hover → full text. Idle stays clearly dim so the active pill stands out.
  const float text_mix =
      std::clamp(selected * 1.0f + hover * 0.65f + (active ? 0.35f : 0.0f), 0.0f, 1.0f);
  ImVec4 text_color = MixColor(colors::textDim, colors::text, text_mix);
  text_color.w *= alpha * (1.0f - 0.08f * press);
  draw->PushClipRect(visual_min, visual_max, true);
  const float text_x = std::floor(center.x - text_size.x * 0.5f + 0.5f);
  const float text_y = CenteredTextTop(font, final_size, visual_min.y, visual_max.y - visual_min.y);
  draw->AddText(font, final_size, ImVec2(text_x, text_y), ImGui::GetColorU32(text_color), label);
  draw->PopClipRect();
  ImGui::PopID();
  return clicked;
}

}  // namespace

void DelayedTooltip(const char* text, float scale) {
  if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) || ImGui::IsAnyItemActive()) return;
  ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(360.0f * scale, FLT_MAX));
  ImGui::BeginTooltip();
  ImGui::PushTextWrapPos(340.0f * scale);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
  ImGui::EndTooltip();
}

bool Toggle(const MotionContext& motion, const char* id, bool* value, float scale, float alpha) {
  (void)motion;
  const float track_width = Metrics::kToggleWidth * scale;
  const float track_height = Metrics::kToggleHeight * scale;
  const float row_height = track_height + 4.0f * scale;
  const bool changed = ImGui::InvisibleButton(id, ImVec2(track_width, row_height));
  const bool hovered = ImGui::IsItemHovered();
  const bool pressed = ImGui::IsItemActive();
  if (changed && value) *value = !*value;
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

  const ImVec2 item_min = ImGui::GetItemRectMin();
  const float center_y = item_min.y + row_height * 0.5f;
  const ImVec2 track_min(item_min.x, center_y - track_height * 0.5f);
  const ImVec2 track_max(item_min.x + track_width, center_y + track_height * 0.5f);
  const float track_rounding = track_height * 0.5f;

  auto& reference_motion = WindowMotion::System();
  const auto& tokens = WindowMotion::Tokens();
  const float hover = reference_motion.value(ReferenceMotionKey("menu.checkbox", id, "hover"),
                                             hovered ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
  const float press = reference_motion.value(ReferenceMotionKey("menu.checkbox", id, "press"),
                                             pressed ? 1.0f : 0.0f, tokens.pressFast, 0.0f);
  // Snappy spring so the knob travel has weight; squash rides the same channel.
  const float fill =
      reference_motion.value(ReferenceMotionKey("menu.checkbox", id, "fill"),
                             value && *value ? 1.0f : 0.0f, tokens.springSnappy, 0.0f);
  const float t = std::clamp(fill, 0.0f, 1.0f);
  // Peak squash mid-travel (sin), stronger press squish while held.
  const float mid = std::sin(3.14159265f * t);
  const float travel_squash = mid * mid;  // 0 at ends, 1 at midpoint
  const float squash = std::clamp(travel_squash * 1.15f + press * 0.55f, 0.0f, 1.35f);

  // ON track stays mid-zinc so the white knob actually reads (was near-white-on-white).
  ImVec4 track_off(0.16f, 0.16f, 0.17f, alpha);
  ImVec4 track_on(0.48f, 0.48f, 0.52f, alpha);
  ImVec4 track_color = MixColor(track_off, track_on, t);
  track_color =
      MixColor(track_color,
               MixColor(ImVec4(0.22f, 0.22f, 0.24f, alpha), ImVec4(0.56f, 0.56f, 0.60f, alpha), t),
               hover * 0.5f);

  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(track_min, track_max, ImGui::GetColorU32(track_color), track_rounding);
  // Soft rim keeps the pill edged against dark cards.
  {
    ImVec4 rim =
        MixColor(ImVec4(0.28f, 0.28f, 0.30f, alpha), ImVec4(0.62f, 0.62f, 0.66f, alpha), t);
    rim.w *= 0.9f + 0.1f * hover;
    draw->AddRect(track_min, track_max, ImGui::GetColorU32(rim), track_rounding, 0,
                  std::max(1.0f, scale));
  }

  // White knob + dark contact shadow — high contrast on both OFF and ON tracks.
  const float padding = 3.0f * scale;
  const float knob_d = track_height - padding * 2.0f;
  const float knob_r = knob_d * 0.5f;
  const float travel = track_width - 2.0f * padding - knob_d;
  const float knob_x = track_min.x + padding + knob_r + travel * t;
  // Stretch wide + flatten on the move; clamp so it never leaves the track.
  const float rx = std::min(knob_r + squash * 2.4f * scale, knob_r + travel * 0.22f);
  const float ry = std::max(2.2f * scale, knob_r - squash * 1.35f * scale);
  const float knob_y = center_y + press * 0.4f * scale;
  draw->AddRectFilled(ImVec2(knob_x - rx, knob_y - ry + 1.0f * scale),
                      ImVec2(knob_x + rx, knob_y + ry + 1.0f * scale),
                      ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.35f * alpha)), ry);
  draw->AddRectFilled(ImVec2(knob_x - rx, knob_y - ry), ImVec2(knob_x + rx, knob_y + ry),
                      ImGui::GetColorU32(ImVec4(0.97f, 0.97f, 0.98f, alpha)), ry);
  // Thin graphite ring so the disc never blends into a mid-gray ON track.
  draw->AddRect(ImVec2(knob_x - rx, knob_y - ry), ImVec2(knob_x + rx, knob_y + ry),
                ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.18f * alpha)), ry, 0,
                std::max(1.0f, scale));
  return changed;
}

bool Slider(const MotionContext& motion, const char* id, const char* label, float* value,
            float minimum, float maximum, float width, float scale, float alpha, ImFont* label_font,
            float step, float display_multiplier, int display_precision,
            const char* display_suffix) {
  (void)motion;
  (void)step; 
  if (!value || maximum <= minimum) return false;
  if (display_multiplier == 0.0f) display_multiplier = 1.0f;
  display_precision = std::clamp(display_precision, 0, 6);
  if (!display_suffix) display_suffix = "";

  if (!label_font) label_font = ImGui::GetFont();
  const float value_at_frame_start = *value;
  const ImVec2 row_origin = ImGui::GetCursorScreenPos();
  const bool has_label = label != nullptr && label[0] != '\0';
  const float label_height = has_label ? 14.0f * scale : 0.0f;
  if (has_label) {
    ImGui::GetWindowDrawList()->AddText(
        label_font, label_font->FontSize, row_origin,
        ImGui::GetColorU32(ImVec4(colors::textDim.x, colors::textDim.y, colors::textDim.z,
                                  colors::textDim.w * alpha)),
        label);
  }

  // Slider: simple rail + pearl-gray capsule on the fill tip. Small value chip right.
  const float height = Metrics::kSliderHeight * scale;
  const ImVec2 origin(row_origin.x,
                      row_origin.y + (has_label ? label_height + 4.0f * scale : 0.0f));

  // Fixed chip from min/max glyphs — rail width never jumps with the value.
  // Use full baked font size so glyphs are never soft-scaled or clipped.
  const float value_font_sz = label_font->FontSize;
  char probe_lo[32]{};
  char probe_hi[32]{};
  std::snprintf(probe_lo, sizeof(probe_lo), "%.*f%s", display_precision,
                minimum * display_multiplier, display_suffix);
  std::snprintf(probe_hi, sizeof(probe_hi), "%.*f%s", display_precision,
                maximum * display_multiplier, display_suffix);
  const float probe_w =
      std::max(label_font->CalcTextSizeA(value_font_sz, FLT_MAX, 0.0f, probe_lo).x,
               label_font->CalcTextSizeA(value_font_sz, FLT_MAX, 0.0f, probe_hi).x);
  const float value_chip_w = std::max(36.0f * scale, probe_w + 8.0f * scale);
  const float value_gap = 6.0f * scale;
  const float track_end = origin.x + width - value_chip_w - value_gap;
  // Pearl sits on the rail; leave half-pearl pad so it never clips the ends.
  const float pearl_half_w = 7.0f * scale;
  const float start = origin.x + pearl_half_w;
  const float end = std::max(start + 16.0f * scale, track_end - pearl_half_w);

  ImGui::PushID(id);
  ImGui::SetCursorScreenPos(origin);
  ImGui::InvisibleButton("##slider", ImVec2(width - value_chip_w - value_gap, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool slider_active = ImGui::IsItemActive();
  const bool open_value_editor = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  const ImGuiIO& io = ImGui::GetIO();
  // Value changes only via left-click drag on the rail (no wheel / arrow keys).
  if (slider_active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    const float ratio = std::clamp((io.MousePos.x - start) / (end - start), 0.0f, 1.0f);
    *value = minimum + (maximum - minimum) * ratio;
  }

  // Value box tall enough for full ascent/descent + caret (no clipping).
  const float value_box_width = value_chip_w;
  const float value_box_height =
      std::max(height, (label_font->Ascent - label_font->Descent) + 2.0f * scale);
  const float value_box_y = origin.y + (height - value_box_height) * 0.5f;
  const ImVec2 value_box_min(origin.x + width - value_box_width, value_box_y);
  const ImVec2 value_box_max(value_box_min.x + value_box_width, value_box_min.y + value_box_height);
  const ImGuiID mode_id = ImGui::GetID("##value_mode");
  int* mode = ImGui::GetStateStorage()->GetIntRef(mode_id, 0);
  if (open_value_editor) *mode = 1;

  auto apply_parsed = [&](const char* text) -> bool {
    char* end_ptr = nullptr;
    const float parsed = std::strtof(text, &end_ptr);
    if (end_ptr == text || !std::isfinite(parsed)) return false;
    *value = std::clamp(parsed / display_multiplier, minimum, maximum);
    return true;
  };

  const ImGuiID input_id = ImGui::GetID("##value_input");
  auto& input_buffer = g_slider_value_buffers[input_id];
  bool editing = *mode > 0;
  if (editing) {
    if (*mode == 1) {
      std::snprintf(input_buffer.data(), input_buffer.size(), "%.*f", display_precision,
                    *value * display_multiplier);
      ImGui::SetKeyboardFocusHere();
      *mode = 2;
    }
    // Center glyphs: FrameHeight = FontSize + 2*pad_y must equal value_box_height.
    const float input_text_w =
        label_font->CalcTextSizeA(label_font->FontSize, FLT_MAX, 0.0f, input_buffer.data()).x;
    const float pad_x = std::max(0.0f, (value_box_width - input_text_w) * 0.5f);
    const float pad_y = std::max(0.0f, (value_box_height - label_font->FontSize) * 0.5f);
    ImGui::SetCursorScreenPos(value_box_min);
    ImGui::PushFont(label_font);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad_x, pad_y));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImVec4(colors::text.x, colors::text.y, colors::text.z, alpha));
    ImGui::SetNextItemWidth(value_box_width);
    const bool submitted = ImGui::InputText(
        "##value_input", input_buffer.data(), input_buffer.size(),
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_NoHorizontalScroll);
    // Live-apply so the rail follows the typed number immediately.
    apply_parsed(input_buffer.data());
    if (submitted) {
      apply_parsed(input_buffer.data());
      *mode = 0;
      editing = false;
    } else if (*mode == 2 && !ImGui::IsItemActive() &&
               (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
      apply_parsed(input_buffer.data());
      *mode = 0;
      editing = false;
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
    ImGui::PopFont();
  }

  *value = std::clamp(*value, minimum, maximum);

  auto& reference_motion = WindowMotion::System();
  const auto& tokens = WindowMotion::Tokens();
  const auto fill_key = ReferenceMotionKey("menu.slider", id, "fill");
  const float visual = std::clamp((*value - minimum) / (maximum - minimum), 0.0f, 1.0f);
  // Always spring the fill (same path for duration + strength). Snappy while scrubbing so the
  // pearl still tracks the pointer; soft spring when released or set externally.
  const ::ui::motion::MotionSpec& fill_spec =
      (slider_active || editing || *mode > 0) ? tokens.springSnappy : tokens.springSoft;
  const float animated = reference_motion.value(fill_key, visual, fill_spec, visual);
  const float track_y = origin.y + height * 0.5f;
  const float pearl_x = start + (end - start) * animated;
  ImDrawList* draw = ImGui::GetWindowDrawList();

  // Quiet empty rail, soft fill — pearl is the only bright element.
  const float track_h = 5.0f * scale;
  const float track_rounding = track_h * 0.5f;
  const float rail_left = origin.x;
  const float rail_right = track_end;
  draw->AddRectFilled(ImVec2(rail_left, track_y - track_h * 0.5f),
                      ImVec2(rail_right, track_y + track_h * 0.5f),
                      ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.06f * alpha)), track_rounding);
  if (pearl_x > rail_left + 0.5f) {
    draw->AddRectFilled(ImVec2(rail_left, track_y - track_h * 0.5f),
                        ImVec2(pearl_x, track_y + track_h * 0.5f),
                        ImGui::GetColorU32(ImVec4(0.38f, 0.38f, 0.41f, alpha)), track_rounding);
  }

  // Pearl bead on the fill tip — lighter than the fill so it actually reads.
  const float pearl_state =
      reference_motion.value(ReferenceMotionKey("menu.slider", id, "knob"),
                             (slider_active || hovered) ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
  const float pearl_h = track_h + 5.0f * scale + 1.0f * scale * pearl_state;
  const float pearl_w = pearl_h * 1.45f;
  const ImVec2 pearl_min(pearl_x - pearl_w * 0.5f, track_y - pearl_h * 0.5f);
  const ImVec2 pearl_max(pearl_x + pearl_w * 0.5f, track_y + pearl_h * 0.5f);
  const float pearl_r = pearl_h * 0.5f;
  draw->AddRectFilled(ImVec2(pearl_min.x, pearl_min.y + 0.7f * scale),
                      ImVec2(pearl_max.x, pearl_max.y + 0.7f * scale),
                      ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.30f * alpha)), pearl_r);
  const ImVec4 pearl_body =
      MixColor(ImVec4(0.78f, 0.78f, 0.81f, alpha), ImVec4(0.88f, 0.88f, 0.90f, alpha), pearl_state);
  draw->AddRectFilled(pearl_min, pearl_max, ImGui::GetColorU32(pearl_body), pearl_r);
  draw->AddRectFilled(ImVec2(pearl_min.x + 1.2f * scale, pearl_min.y + 1.0f * scale),
                      ImVec2(pearl_max.x - 1.2f * scale, track_y),
                      ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.18f * alpha)), pearl_r * 0.75f);
  draw->AddRect(pearl_min, pearl_max, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.16f * alpha)),
                pearl_r, 0, std::max(1.0f, scale));

  char display[32]{};
  std::snprintf(display, sizeof(display), "%.*f%s", display_precision, *value * display_multiplier,
                display_suffix);
  const ImVec2 display_size = label_font->CalcTextSizeA(value_font_sz, FLT_MAX, 0.0f, display);
  const bool value_hovered = ImGui::IsMouseHoveringRect(value_box_min, value_box_max, false);
  const float value_hover =
      reference_motion.value(ReferenceMotionKey("menu.slider", id, "value-box"),
                             (*mode > 0 || value_hovered) ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
  const float box_rounding = 3.0f * scale;
  if (value_hover > 0.01f || *mode > 0) {
    const float chip_a = std::max(value_hover, *mode > 0 ? 1.0f : 0.0f);
    draw->AddRectFilled(value_box_min, value_box_max,
                        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.06f * chip_a * alpha)),
                        box_rounding);
  }

  if (*mode == 0) {
    const ImVec4 value_color = MixColor(colors::textDim, colors::text, 0.35f + 0.65f * value_hover);
    // Horizontally + vertically centered — full baked size, no scale-down clip.
    const float text_x =
        std::floor(value_box_min.x + (value_box_width - display_size.x) * 0.5f + 0.5f);
    const float text_y = CenteredTextTop(label_font, value_box_min.y, value_box_height);
    draw->AddText(label_font, value_font_sz, ImVec2(text_x, text_y),
                  ImGui::GetColorU32(ImVec4(value_color.x, value_color.y, value_color.z, alpha)),
                  display);
    ImGui::SetCursorScreenPos(value_box_min);
    ImGui::InvisibleButton("##value_button", ImVec2(value_box_width, value_box_height));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) *mode = 1;
  }

  ImGui::SetCursorScreenPos(ImVec2(row_origin.x, origin.y + height + 4.0f * scale));
  ImGui::PopID();
  return slider_active || *mode > 0 || std::abs(*value - value_at_frame_start) > 0.0001f;
}

bool Combo(const MotionContext& motion_context, const char* id, const char* label, int* current,
           std::span<const char* const> items, const ImVec2& size, ImFont* label_font,
           ImFont* item_font, float scale, float alpha) {
  if (!current || items.empty()) return false;
  if (!label_font) label_font = ImGui::GetFont();
  if (!item_font) item_font = ImGui::GetFont();

  ImGui::PushID(id);
  ImGui::BeginGroup();
  auto& motion = motion_context.system;
  const auto& tokens = motion_context.tokens;
  const float rounding = ControlRounding(scale);

  if (label && label[0] != '\0') {
    ImGui::PushFont(label_font);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(colors::textDim.x, colors::textDim.y,
                                                colors::textDim.z, colors::textDim.w * alpha));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();
  }

  const ImVec2 frame_min = ImGui::GetCursorScreenPos();
  const float frame_width = std::max(1.0f, size.x);
  const float frame_height = std::max(1.0f, size.y);
  const bool pressed = ImGui::InvisibleButton("##combo_button", ImVec2(frame_width, frame_height));
  const bool hovered = ImGui::IsItemHovered();
  const bool item_active = ImGui::IsItemActive();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  const ImVec2 frame_max = ImGui::GetItemRectMax();
  const bool anchor_visible = ImGui::IsRectVisible(frame_min, frame_max);

  const ImGuiID popup_id = ImGui::GetID("##combo_dropdown");
  const auto popup_alpha_key = ReferenceMotionKey("menu.combo", id, "popup-alpha");
  const auto popup_slide_key = ReferenceMotionKey("menu.combo", id, "popup-slide");
  const auto arrow_open_key = ReferenceMotionKey("menu.combo", id, "arrow-open");
  const auto arrow_color_key = ReferenceMotionKey("menu.combo", id, "arrow-color");

  bool& was_open = g_combo_was_open[popup_id];
  bool& closing = g_combo_closing[popup_id];
  // Asymmetric open/close curves — shared cubic felt too linear on the height expand.
  const ::ui::motion::MotionSpec& open_spec = tokens.popupOpen;
  const ::ui::motion::MotionSpec& close_spec = tokens.popupClose;
  const ::ui::motion::MotionSpec& expand_spec = closing ? close_spec : open_spec;

  // Non-modal dropdown: no ImGui popup stack. Modal popups blocked ampel hover and ate the
  // first click on everything else — this keeps the rest of the UI live while open.
  if (pressed) {
    if (was_open && !closing) {
      closing = true;
    } else {
      motion.set(popup_alpha_key, 0.0f);
      motion.set(popup_slide_key, 0.0f);
      was_open = true;
      closing = false;
    }
  }
  if (was_open && !anchor_visible) closing = true;

  const bool session_active = was_open;
  const bool open_visual = session_active && !closing;
  const float open_amount =
      session_active
          ? std::clamp(motion.value(popup_slide_key, closing ? 0.0f : 1.0f, expand_spec, 0.0f),
                       0.0f, 1.0f)
          : 0.0f;
  (void)motion.value(popup_alpha_key, open_visual ? 1.0f : 0.0f, expand_spec, 0.0f);

  // Hover paint only while fully closed — open/closing uses open_amount tint only.
  const float hover_target = session_active ? 0.0f : (hovered ? 1.0f : 0.0f);
  const float frame_hover = motion.value(ReferenceMotionKey("menu.combo", id, "frame-hover"),
                                         hover_target, tokens.hoverSoft, 0.0f);
  const float frame_press =
      motion.value(ReferenceMotionKey("menu.combo", id, "frame-press"),
                   item_active && !session_active ? 1.0f : 0.0f, tokens.pressFast, 0.0f);

  ImVec4 frame_bg =
      MixColor(ImVec4(0.11f, 0.11f, 0.12f, 1.0f), ImVec4(0.15f, 0.15f, 0.16f, 1.0f), frame_hover);
  frame_bg = MixColor(frame_bg, ImVec4(0.13f, 0.13f, 0.14f, 1.0f), frame_press);
  ImVec4 frame_border =
      MixColor(ImVec4(0.22f, 0.22f, 0.24f, 1.0f), ImVec4(0.36f, 0.36f, 0.39f, 1.0f), frame_hover);
  if (open_amount > 0.001f) {
    frame_bg = MixColor(frame_bg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f), open_amount);
    frame_border = MixColor(frame_border, ImVec4(0.28f, 0.28f, 0.30f, 1.0f), open_amount);
  }
  frame_bg.w *= alpha;
  frame_border.w *= alpha;

  const float row_height = 30.0f * scale;
  const int item_count = static_cast<int>(items.size());
  const int visible_count = std::min(8, item_count);
  const float popup_pad_y = 4.0f * scale;
  const float popup_full_h = static_cast<float>(visible_count) * row_height + popup_pad_y * 2.0f;
  const float popup_height = popup_full_h * open_amount;

  bool opens_upward = false;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (frame_max.y + popup_full_h > viewport->Pos.y + viewport->Size.y) {
    opens_upward = true;
  }
  // Slight visual overlap at the join so the parent bg never peeks through (esp. upward).
  const float join_overlap = 1.5f * scale;
  ImVec2 popup_position(frame_min.x, frame_max.y - join_overlap);
  if (opens_upward) {
    popup_position.y = frame_min.y - popup_height + join_overlap;
  }

  // Full open rect for outside-click tests (stable, not tied to animated height).
  ImVec2 list_hit_min(frame_min.x, frame_max.y - join_overlap);
  ImVec2 list_hit_max(frame_min.x + frame_width, frame_max.y - join_overlap + popup_full_h);
  if (opens_upward) {
    list_hit_min = ImVec2(frame_min.x, frame_min.y - popup_full_h + join_overlap);
    list_hit_max = ImVec2(frame_min.x + frame_width, frame_min.y + join_overlap);
  }
  // Outside click closes without consuming the click — other widgets react on the first press.
  if (was_open && !closing && !pressed && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool over_field = mouse.x >= frame_min.x && mouse.x <= frame_max.x &&
                            mouse.y >= frame_min.y && mouse.y <= frame_max.y;
    const bool over_list = mouse.x >= list_hit_min.x && mouse.x <= list_hit_max.x &&
                           mouse.y >= list_hit_min.y && mouse.y <= list_hit_max.y;
    if (!over_field && !over_list) closing = true;
  }

  ImDrawList* draw = ImGui::GetWindowDrawList();
  // Connected expand = one growing shell. Idle field is fully rounded; once open, only the
  // outer corners stay rounded — join-side corners are square so parent bg never peeks through
  // the quarter-circle gaps (the classic free corner at a rounded field + square list join).
  const ImU32 frame_bg_u32 = ImGui::GetColorU32(frame_bg);
  const ImU32 frame_border_u32 = ImGui::GetColorU32(frame_border);
  const float th = std::max(1.0f, scale);
  const bool connected = open_amount > 0.001f && popup_height > 0.5f;

  if (!connected) {
    draw->AddRectFilled(frame_min, frame_max, frame_bg_u32, rounding, ImDrawFlags_RoundCornersAll);
    draw->AddRect(frame_min, frame_max, frame_border_u32, rounding, ImDrawFlags_RoundCornersAll,
                  th);
  } else {
    // Field fill only — border comes from the unified outer stroke after the list is drawn.
    // Down: round top only. Up: round bottom only. Join side is flat against the list.
    const ImDrawFlags field_corners =
        opens_upward ? ImDrawFlags_RoundCornersBottom : ImDrawFlags_RoundCornersTop;
    draw->AddRectFilled(frame_min, frame_max, frame_bg_u32, rounding, field_corners);
  }

  bool changed = false;
  const bool show_popup = session_active && popup_height > 0.5f;
  if (show_popup) {
    char win_name[96];
    std::snprintf(win_name, sizeof(win_name), "##combo_dd_%u", popup_id);
    ImGui::SetNextWindowPos(popup_position);
    ImGui::SetNextWindowSize(ImVec2(frame_width, popup_height));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);

    // Normal window (not a popup) so ampel / buttons / sliders stay hoverable and clickable.
    const ImGuiWindowFlags dd_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin(win_name, nullptr, dd_flags)) {
      ImDrawList* popup_draw = ImGui::GetWindowDrawList();
      const ImVec2 popup_min = ImGui::GetWindowPos();
      const ImVec2 popup_max(popup_min.x + frame_width, popup_min.y + popup_height);

      ImVec4 body = frame_bg;
      body.w = alpha;
      ImVec4 body_border = frame_border;
      body_border.w = alpha;
      const ImU32 body_u32 = ImGui::GetColorU32(body);
      const ImU32 border_u32 = ImGui::GetColorU32(body_border);

      // Unified shell bounds (field + list). Outer silhouette is fully rounded; join is square.
      const ImVec2 shell_min = opens_upward ? popup_min : ImVec2(frame_min.x, frame_min.y);
      const ImVec2 shell_max = opens_upward ? ImVec2(frame_max.x, frame_max.y) : popup_max;
      const float shell_h = shell_max.y - shell_min.y;
      const float shell_r = std::min(rounding, std::max(1.0f, shell_h * 0.5f - 0.5f));

      // List fill: round only the free end (matches outer stroke). Join side stays square.
      // Using shell_r (not 0) so outer corners aren't free triangles outside a rounded border.
      const ImDrawFlags list_corners =
          opens_upward ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersBottom;
      const float list_r =
          std::min(shell_r, std::max(1.0f, (popup_max.y - popup_min.y) * 0.5f - 0.5f));
      popup_draw->AddRectFilled(popup_min, popup_max, body_u32, list_r, list_corners);
      // Thin AA bridge at the join (full-width strip) so field/list never leave a hairline gap.
      const float seam = 2.0f * scale;
      if (opens_upward) {
        popup_draw->AddRectFilled(ImVec2(popup_min.x, popup_max.y - seam),
                                  ImVec2(popup_max.x, popup_max.y + seam), body_u32);
      } else {
        popup_draw->AddRectFilled(ImVec2(popup_min.x, popup_min.y - seam),
                                  ImVec2(popup_max.x, popup_min.y + seam), body_u32);
      }
      // Single outer outline around the whole shell (FG so it isn't clipped / double-stroked).
      ImGui::GetForegroundDrawList()->AddRect(shell_min, shell_max, border_u32, shell_r,
                                              ImDrawFlags_RoundCornersAll, th);

      popup_draw->PushClipRect(popup_min, popup_max, true);

      const bool popup_disabled = closing || open_amount < 0.92f;
      if (popup_disabled) ImGui::BeginDisabled();
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

      const float content_block_h = static_cast<float>(item_count) * row_height;
      const float content_top =
          opens_upward ? (popup_height - popup_pad_y - content_block_h) : popup_pad_y;
      const float text_pad_left = 11.0f * scale;
      const float text_pad_check = 24.0f * scale;
      for (int index = 0; index < item_count; ++index) {
        ImGui::PushID(index);
        const float row_y = content_top + static_cast<float>(index) * row_height;
        if (row_y + row_height < 0.0f || row_y > popup_height) {
          ImGui::PopID();
          continue;
        }
        const ImGuiID item_id = ImGui::GetID("##item_animation");
        ImGui::SetCursorPos(ImVec2(0.0f, row_y));
        const ImVec2 item_min = ImGui::GetCursorScreenPos();
        const float item_width = frame_width;
        float row_reveal = open_amount;
        if (!closing) {
          const float row_frac =
              (static_cast<float>(index) + 0.55f) / std::max(1.0f, static_cast<float>(item_count));
          const float row_t = std::clamp((open_amount - row_frac * 0.35f) / 0.65f, 0.0f, 1.0f);
          row_reveal = row_t * row_t * (3.0f - 2.0f * row_t);
        }
        bool item_clicked = false;
        bool item_hovered = false;
        if (!popup_disabled) {
          item_clicked = ImGui::InvisibleButton("##item", ImVec2(item_width, row_height));
          item_hovered = ImGui::IsItemHovered();
        } else {
          ImGui::Dummy(ImVec2(item_width, row_height));
        }
        if (item_clicked) {
          *current = index;
          changed = true;
          closing = true;
        }

        const bool is_selected = *current == index;
        const float item_hover = motion.value(
            ::ui::motion::MotionKey("menu.combo.item", std::to_string(item_id), "hover"),
            item_hovered ? 1.0f : 0.0f, tokens.hoverSoft, 0.0f);
        const float select_amt = motion.value(
            ::ui::motion::MotionKey("menu.combo.item", std::to_string(item_id), "select"),
            is_selected ? 1.0f : 0.0f, tokens.selectSharp, 0.0f);
        const float indent = motion.value(
            ::ui::motion::MotionKey("menu.combo.item", std::to_string(item_id), "indent"),
            (item_hovered || is_selected) ? 1.0f : 0.0f, tokens.hoverSoft, 0.0f);
        const float pad_x = 5.0f * scale;
        const float pad_y = 2.5f * scale;
        if ((item_hover > 0.001f || select_amt > 0.001f) && row_reveal > 0.04f) {
          const float fill =
              (0.10f * select_amt + 0.05f * item_hover * (1.0f - select_amt)) * row_reveal;
          popup_draw->AddRectFilled(
              ImVec2(item_min.x + pad_x, item_min.y + pad_y),
              ImVec2(item_min.x + item_width - pad_x, item_min.y + row_height - pad_y),
              ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, fill * alpha)), 6.0f * scale);
        }

        const float check_alpha =
            (select_amt * 1.0f + item_hover * 0.38f * (1.0f - select_amt)) * row_reveal * alpha;
        if (check_alpha > 0.02f) {
          const ImVec2 cc(item_min.x + 12.0f * scale, item_min.y + row_height * 0.5f);
          const float s = 3.0f * scale * (0.55f + 0.45f * std::max(select_amt, item_hover));
          popup_draw->PathLineTo(ImVec2(cc.x - s, cc.y));
          popup_draw->PathLineTo(ImVec2(cc.x - s * 0.25f, cc.y + s * 0.85f));
          popup_draw->PathLineTo(ImVec2(cc.x + s * 1.15f, cc.y - s * 0.85f));
          popup_draw->PathStroke(ImGui::GetColorU32(ImVec4(colors::text.x, colors::text.y,
                                                           colors::text.z, check_alpha)),
                                 0, 1.5f * scale);
        }

        const float text_x = item_min.x + text_pad_left + (text_pad_check - text_pad_left) * indent;
        ImVec4 item_color =
            MixColor(colors::textDim, colors::text, std::max(select_amt, item_hover * 0.85f));
        item_color.w *= alpha * row_reveal;
        popup_draw->AddText(item_font, item_font->FontSize,
                            ImVec2(text_x, CenteredTextTop(item_font, item_min.y, row_height)),
                            ImGui::GetColorU32(item_color), items[index]);
        ImGui::PopID();
      }
      ImGui::PopStyleVar(2);
      if (popup_disabled) ImGui::EndDisabled();
      popup_draw->PopClipRect();
    }
    ImGui::End();
    ImGui::PopStyleVar(4);

    if (closing && open_amount <= 0.02f) {
      motion.forget(popup_alpha_key);
      motion.forget(popup_slide_key);
      motion.forget(arrow_open_key);
      motion.forget(arrow_color_key);
      was_open = false;
      closing = false;
    }
  } else if (session_active && closing && open_amount <= 0.02f) {
    motion.forget(popup_alpha_key);
    motion.forget(popup_slide_key);
    motion.forget(arrow_open_key);
    motion.forget(arrow_color_key);
    was_open = false;
    closing = false;
  }

  const float chevron_zone = 26.0f * scale;
  const char* preview =
      *current >= 0 && *current < static_cast<int>(items.size()) ? items[*current] : "";
  if (preview && preview[0]) {
    draw->PushClipRect(frame_min, ImVec2(frame_max.x - chevron_zone, frame_max.y), true);
    draw->AddText(item_font, item_font->FontSize,
                  ImVec2(std::floor(frame_min.x + 11.0f * scale + 0.5f),
                         CenteredTextTop(item_font, frame_min.y, frame_height)),
                  ImGui::GetColorU32(ImVec4(colors::text.x, colors::text.y, colors::text.z, alpha)),
                  preview);
    draw->PopClipRect();
  }

  const float chevron_spin =
      motion.value(arrow_open_key, open_visual ? 1.0f : 0.0f, expand_spec, 0.0f);
  const float arrow_size = 5.0f * scale;
  const ImVec2 arrow_center(frame_max.x - 13.0f * scale, (frame_min.y + frame_max.y) * 0.5f);
  const ImVec4 arrow_target =
      open_visual || hovered ? colors::text : ImVec4(0.52f, 0.52f, 0.55f, 1.0f);
  ImVec4 arrow_color =
      motion.color(arrow_color_key, arrow_target, tokens.hoverSoft, colors::textDim);
  arrow_color.w *= alpha;

  const float angle = chevron_spin * 3.14159265f;
  const float cos_a = std::cos(angle);
  const float sin_a = std::sin(angle);
  auto rot = [&](float lx, float ly) -> ImVec2 {
    return ImVec2(arrow_center.x + lx * cos_a - ly * sin_a,
                  arrow_center.y + lx * sin_a + ly * cos_a);
  };
  draw->PathLineTo(rot(-arrow_size * 0.55f, -arrow_size * 0.2f));
  draw->PathLineTo(rot(0.0f, arrow_size * 0.3f));
  draw->PathLineTo(rot(arrow_size * 0.55f, -arrow_size * 0.2f));
  draw->PathStroke(ImGui::GetColorU32(arrow_color), 0, 1.5f * scale);

  ImGui::EndGroup();
  ImGui::PopID();
  return changed;
}

bool CompactButton(const MotionContext& motion, const char* id, const char* label,
                   const ImVec2& size, ImFont* font, float scale, float alpha, bool active) {
  (void)motion;
  (void)scale;
  return ReferenceButton(id, label, size, font, alpha, active);
}

bool SegmentSelector(const MotionContext& motion, const char* id,
                     std::span<const char* const> labels, int* selected, float width, ImFont* font,
                     float scale, float alpha) {
  if (!selected || labels.empty()) return false;
  if (!font) font = ImGui::GetFont();
  *selected = std::clamp(*selected, 0, static_cast<int>(labels.size()) - 1);

  const ImVec2 min = ImGui::GetCursorScreenPos();
  const float height = Metrics::kSegmentHeight * scale;
  const float segment_width = width / static_cast<float>(labels.size());
  const float rounding = ControlRounding(scale);
  bool changed = false;

  ImDrawList* draw = ImGui::GetWindowDrawList();

  const ImVec2 max(min.x + width, min.y + height);
  ImVec4 track_bg = colors::comboBg;
  track_bg.w *= alpha;
  ImVec4 track_border = colors::border;
  track_border.w *= alpha;

  // Same elevated track as combo fields.
  track_bg = ImVec4(0.10f, 0.10f, 0.11f, alpha);
  track_border = ImVec4(0.22f, 0.22f, 0.24f, alpha);
  draw->AddRectFilled(min, max, ImGui::GetColorU32(track_bg), rounding);
  draw->AddRect(min, max, ImGui::GetColorU32(track_border), rounding, 0, std::max(1.0f, scale));

  const float inset = 3.0f * scale;
  const float target_x = static_cast<float>(*selected) * segment_width;
  const float current_x = motion.system.value(
      ::ui::motion::MotionKey("segment-selector", id ? id : "close_behavior", "slide-x"), target_x,
      motion.tokens.springSoft, target_x);

  const ImVec2 pill_min(min.x + current_x + inset, min.y + inset);
  const ImVec2 pill_max(min.x + current_x + segment_width - inset, min.y + height - inset);
  const float pill_rounding = std::max(2.0f * scale, rounding - 2.0f * scale);

  ImVec4 pill_bg(0.18f, 0.18f, 0.20f, alpha);
  draw->AddRectFilled(pill_min, pill_max, ImGui::GetColorU32(pill_bg), pill_rounding);
  draw->AddRect(pill_min, pill_max, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.08f * alpha)),
                pill_rounding, 0, std::max(1.0f, scale));

  for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
    const ImVec2 segment_min(min.x + segment_width * static_cast<float>(index), min.y);
    const ImVec2 segment_max(segment_min.x + segment_width, min.y + height);

    ImGui::SetCursorScreenPos(segment_min);
    const std::string segment_id = std::format("{}##segment_{}", id ? id : "segment", index);
    if (ImGui::InvisibleButton(segment_id.c_str(), ImVec2(segment_width, height))) {
      if (*selected != index) {
        *selected = index;
        changed = true;
      }
    }
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    const float hover_val =
        motion.system.value(::ui::motion::MotionKey("segment-selector", segment_id, "hover"),
                            hovered ? 1.0f : 0.0f, motion.tokens.hoverFast, 0.0f);
    if (hover_val > 0.0f && *selected != index) {
      ImVec4 hover_glow(1.0f, 1.0f, 1.0f, 0.03f * hover_val * alpha);
      draw->AddRectFilled(ImVec2(segment_min.x + inset, segment_min.y + inset),
                          ImVec2(segment_max.x - inset, segment_max.y - inset),
                          ImGui::GetColorU32(hover_glow), pill_rounding);
    }

    const bool is_active = (*selected == index);
    const ImVec4 text_target = is_active || hovered ? colors::text : colors::textDim;
    ImVec4 text_color = motion.system.color(
        ::ui::motion::MotionKey("segment-selector", segment_id, "text-color"), text_target,
        is_active ? motion.tokens.selectSharp : motion.tokens.hoverFast, colors::textDim);
    text_color.w *= alpha;

    const ImVec2 text_size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, labels[index]);
    const float text_x = std::floor(segment_min.x + (segment_width - text_size.x) * 0.5f + 0.5f);
    const float text_y = CenteredTextTop(font, segment_min.y, height);
    draw->AddText(font, font->FontSize, ImVec2(text_x, text_y), ImGui::GetColorU32(text_color),
                  labels[index]);
  }

  ImGui::SetCursorScreenPos(ImVec2(min.x, min.y + height));
  return changed;
}

bool EasingGraphEditor(const MotionContext& motion, const char* id, animation::CubicBezier* bezier,
                       const ImVec2& size, float scale, float alpha, bool* changed,
                       ImFont* caption_font) {
  (void)motion;
  if (!bezier || size.x < 8.0f || size.y < 8.0f) return false;
  if (changed) *changed = false;
  if (!caption_font) caption_font = ImGui::GetFont();

  ImGui::PushID(id);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##easing_graph", size);
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  ImDrawList* draw = ImGui::GetWindowDrawList();

  const float rounding = 10.0f * scale;
  const float pad = 14.0f * scale;
  const float caption_h = caption_font->FontSize + 6.0f * scale;
  const ImVec2 panel_min = origin;
  const ImVec2 panel_max(origin.x + size.x, origin.y + size.y);

  // Soft inset card
  draw->AddRectFilled(panel_min, panel_max,
                      ImGui::GetColorU32(ImVec4(0.09f, 0.09f, 0.10f, alpha)), rounding);
  draw->AddRect(panel_min, panel_max,
                ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.24f, 0.9f * alpha)), rounding, 0,
                std::max(1.0f, scale));

  // Plot area — leave room for axis labels at bottom
  const ImVec2 plot_min(panel_min.x + pad, panel_min.y + pad * 0.65f);
  const ImVec2 plot_max(panel_max.x - pad, panel_max.y - pad - caption_h);
  const float plot_w = std::max(1.0f, plot_max.x - plot_min.x);
  const float plot_h = std::max(1.0f, plot_max.y - plot_min.y);

  // Y range allows mild overshoot so Back-like custom curves stay visible
  constexpr float kYMin = -0.25f;
  constexpr float kYMax = 1.25f;
  const float y_span = kYMax - kYMin;

  auto to_screen = [&](float nx, float ny) -> ImVec2 {
    const float u = std::clamp(nx, 0.0f, 1.0f);
    const float v = (std::clamp(ny, kYMin, kYMax) - kYMin) / y_span;
    return ImVec2(plot_min.x + u * plot_w, plot_max.y - v * plot_h);
  };
  auto from_screen = [&](ImVec2 p, float* nx, float* ny) {
    *nx = std::clamp((p.x - plot_min.x) / plot_w, 0.0f, 1.0f);
    const float v = std::clamp((plot_max.y - p.y) / plot_h, 0.0f, 1.0f);
    *ny = kYMin + v * y_span;
  };

  // Plot background
  draw->AddRectFilled(plot_min, plot_max,
                      ImGui::GetColorU32(ImVec4(0.06f, 0.06f, 0.07f, alpha)), 6.0f * scale);

  // Grid
  for (int i = 0; i <= 4; ++i) {
    const float t = static_cast<float>(i) * 0.25f;
    const ImU32 grid =
        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, (i == 0 || i == 4 ? 0.10f : 0.05f) * alpha));
    const ImVec2 a = to_screen(t, kYMin);
    const ImVec2 b = to_screen(t, kYMax);
    draw->AddLine(a, b, grid, 1.0f);
    const ImVec2 c = to_screen(0.0f, kYMin + t * y_span);
    const ImVec2 d = to_screen(1.0f, kYMin + t * y_span);
    draw->AddLine(c, d, grid, 1.0f);
  }

  // Unit square outline (0..1 time/progress band)
  {
    const ImVec2 a = to_screen(0.0f, 0.0f);
    const ImVec2 b = to_screen(1.0f, 0.0f);
    const ImVec2 c = to_screen(1.0f, 1.0f);
    const ImVec2 d = to_screen(0.0f, 1.0f);
    const ImU32 unit = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.08f * alpha));
    draw->AddLine(a, b, unit, 1.0f);
    draw->AddLine(b, c, unit, 1.0f);
    draw->AddLine(c, d, unit, 1.0f);
    draw->AddLine(d, a, unit, 1.0f);
  }

  // Linear reference (dashed diagonal)
  {
    const ImU32 ref = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f * alpha));
    const int dashes = 18;
    for (int i = 0; i < dashes; i += 2) {
      const float t0 = static_cast<float>(i) / static_cast<float>(dashes);
      const float t1 = static_cast<float>(i + 1) / static_cast<float>(dashes);
      draw->AddLine(to_screen(t0, t0), to_screen(t1, t1), ref, 1.0f);
    }
  }

  // Interactive handles first so the curve draws with the live dragged values.
  const float handle_r = 6.0f * scale;
  const float hit_r = 12.0f * scale;
  ImGuiStorage* storage = ImGui::GetStateStorage();
  const ImGuiID drag_id = ImGui::GetID("##handle");
  int drag_handle = storage->GetInt(drag_id, 0);  // 0 none, 1 = p1, 2 = p2

  ImVec2 p1 = to_screen(bezier->x1, bezier->y1);
  ImVec2 p2 = to_screen(bezier->x2, bezier->y2);
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  auto near_handle = [&](const ImVec2& h) {
    const float dx = mouse.x - h.x;
    const float dy = mouse.y - h.y;
    return dx * dx + dy * dy <= hit_r * hit_r;
  };

  if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (near_handle(p1))
      drag_handle = 1;
    else if (near_handle(p2))
      drag_handle = 2;
    else
      drag_handle = 0;
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) drag_handle = 0;
  storage->SetInt(drag_id, drag_handle);

  bool dragging = false;
  if (drag_handle == 1 || drag_handle == 2) {
    dragging = true;
    float nx = 0.0f;
    float ny = 0.0f;
    from_screen(mouse, &nx, &ny);
    if (ImGui::GetIO().KeyShift) {
      nx = std::round(nx * 50.0f) / 50.0f;
      ny = std::round(ny * 50.0f) / 50.0f;
    }
    if (drag_handle == 1) {
      if (std::abs(bezier->x1 - nx) > 1e-5f || std::abs(bezier->y1 - ny) > 1e-5f) {
        bezier->x1 = nx;
        bezier->y1 = ny;
        if (changed) *changed = true;
      }
    } else {
      if (std::abs(bezier->x2 - nx) > 1e-5f || std::abs(bezier->y2 - ny) > 1e-5f) {
        bezier->x2 = nx;
        bezier->y2 = ny;
        if (changed) *changed = true;
      }
    }
    bezier->ClampHandles();
    p1 = to_screen(bezier->x1, bezier->y1);
    p2 = to_screen(bezier->x2, bezier->y2);
  }

  if (hovered || dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

  const ImVec2 p0 = to_screen(0.0f, 0.0f);
  const ImVec2 p3 = to_screen(1.0f, 1.0f);

  // Control arms
  const ImU32 arm = ImGui::GetColorU32(ImVec4(0.55f, 0.55f, 0.62f, 0.55f * alpha));
  draw->AddLine(p0, p1, arm, 1.5f * scale);
  draw->AddLine(p3, p2, arm, 1.5f * scale);

  // Curve polyline (dense sample of parametric bezier)
  constexpr int kSamples = 64;
  ImVec2 pts[kSamples + 1];
  for (int i = 0; i <= kSamples; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(kSamples);
    float bx = 0.0f;
    float by = 0.0f;
    animation::CubicBezierPoint(*bezier, u, &bx, &by);
    pts[i] = to_screen(bx, by);
  }
  draw->AddPolyline(pts, kSamples + 1,
                    ImGui::GetColorU32(ImVec4(0.75f, 0.78f, 0.90f, 0.18f * alpha)), 0, 5.0f * scale);
  draw->AddPolyline(pts, kSamples + 1,
                    ImGui::GetColorU32(ImVec4(0.88f, 0.90f, 0.98f, 0.92f * alpha)), 0, 2.0f * scale);

  // Endpoints
  const float end_r = 3.2f * scale;
  draw->AddCircleFilled(p0, end_r, ImGui::GetColorU32(ImVec4(0.75f, 0.75f, 0.78f, alpha)), 16);
  draw->AddCircleFilled(p3, end_r, ImGui::GetColorU32(ImVec4(0.75f, 0.75f, 0.78f, alpha)), 16);

  auto draw_handle = [&](const ImVec2& pos, bool hot) {
    const float r = handle_r * (hot ? 1.12f : 1.0f);
    draw->AddCircleFilled(ImVec2(pos.x, pos.y + 0.8f * scale), r,
                          ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.35f * alpha)), 20);
    draw->AddCircleFilled(pos, r, ImGui::GetColorU32(ImVec4(0.92f, 0.93f, 0.96f, alpha)), 20);
    draw->AddCircle(pos, r, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.25f * alpha)), 20,
                    std::max(1.0f, scale));
    if (hot) {
      draw->AddCircle(pos, r + 2.5f * scale,
                      ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.16f * alpha)), 20, 1.0f);
    }
  };
  draw_handle(p1, drag_handle == 1 || (hovered && near_handle(p1) && drag_handle == 0));
  draw_handle(p2, drag_handle == 2 || (hovered && near_handle(p2) && drag_handle == 0));

  // Caption: cubic-bezier values
  char caption[96]{};
  std::snprintf(caption, sizeof(caption), "cubic-bezier(%.2f, %.2f, %.2f, %.2f)", bezier->x1,
                bezier->y1, bezier->x2, bezier->y2);
  const ImVec2 cap_size =
      caption_font->CalcTextSizeA(caption_font->FontSize, FLT_MAX, 0.0f, caption);
  const float cap_x = std::floor(panel_min.x + (size.x - cap_size.x) * 0.5f + 0.5f);
  const float cap_y = std::floor(panel_max.y - pad * 0.35f - caption_font->FontSize + 0.5f);
  draw->AddText(caption_font, caption_font->FontSize, ImVec2(cap_x, cap_y),
                ImGui::GetColorU32(ImVec4(colors::textDim.x, colors::textDim.y, colors::textDim.z,
                                          colors::textDim.w * alpha)),
                caption);

  ImGui::PopID();
  return dragging || active;
}

}  // namespace genie::app::settings_ui
