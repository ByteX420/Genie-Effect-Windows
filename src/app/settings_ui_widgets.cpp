#include "pch.hpp"

#include "app/settings_ui_widgets.hpp"

#include <algorithm>
#include <array>
#include <cctype>
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
  const float hover = motion.value(ReferenceMotionKey("menu.button", id, "hover"),
                                   hovered ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
  const float press = motion.value(ReferenceMotionKey("menu.button", id, "press"),
                                   pressed ? 1.0f : 0.0f, tokens.pressFast, 0.0f);
  const float selected = motion.value(ReferenceMotionKey("menu.button", id, "on_state"),
                                      active ? 1.0f : 0.0f, tokens.fadeFast, 0.0f);

  const ImVec4 active_background(colors::accent.x, colors::accent.y, colors::accent.z, 0.12f);
  const ImVec4 base = MixColor(colors::panelHeader, active_background, selected);
  const ImVec4 hovered_background = MixColor(base, ImVec4(0.11f, 0.11f, 0.11f, 1.0f), hover);
  ImVec4 background = MixColor(hovered_background, ImVec4(0.13f, 0.13f, 0.13f, 1.0f), press);
  background.w *= alpha;

  const float visual_scale = 1.0f - 0.05f * press;
  const float shrink_x = size.x * (1.0f - visual_scale) * 0.5f;
  const float shrink_y = size.y * (1.0f - visual_scale) * 0.5f;
  const ImVec2 visual_min(position.x + shrink_x, position.y + shrink_y);
  const ImVec2 visual_max(position.x + size.x - shrink_x, position.y + size.y - shrink_y);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(visual_min, visual_max, ImGui::GetColorU32(background), 0.0f);
  ImVec4 border = MixColor(colors::border, colors::accent, selected);
  border.w *= alpha;
  draw->AddRect(visual_min, visual_max, ImGui::GetColorU32(border), 0.0f);

  const ImVec2 text_size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label);
  ImVec4 text_color = colors::text;
  text_color.w *= alpha * (1.0f - 0.3f * press);
  draw->AddText(
      font, font->FontSize,
      ImVec2(
          std::floor(visual_min.x + ((visual_max.x - visual_min.x) - text_size.x) * 0.5f + 0.5f),
          std::floor(visual_min.y + ((visual_max.y - visual_min.y) - text_size.y) * 0.5f + 0.5f)),
      ImGui::GetColorU32(text_color), label);
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
  const float row_height = 24.0f * scale;
  // Checkbox is one of the two intentional size overrides for the settings UI.
  // The reference 22x12 geometry is scaled to 38x22 without changing its motion math.
  const float track_width = 38.0f * scale;
  const float track_height = 22.0f * scale;
  const float reference_scale = track_height / (12.0f * scale);
  const bool changed = ImGui::InvisibleButton(id, ImVec2(track_width, row_height));
  const bool hovered = ImGui::IsItemHovered();
  if (changed && value) *value = !*value;
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

  const ImVec2 item_min = ImGui::GetItemRectMin();
  const float center_y = item_min.y + row_height * 0.5f;
  const ImVec2 track_min(std::floor(item_min.x + 0.5f),
                         std::floor(center_y - track_height * 0.5f + 0.5f));
  const ImVec2 track_max(track_min.x + std::floor(track_width + 0.5f),
                         track_min.y + std::floor(track_height + 0.5f));

  auto& reference_motion = WindowMotion::System();
  const auto& tokens = WindowMotion::Tokens();
  const float fill =
      reference_motion.value(ReferenceMotionKey("menu.checkbox", id, "fill"),
                             value && *value ? 1.0f : 0.0f, tokens.springSnappy, 0.0f);
  const float fill_position = std::clamp(fill, -0.2f, 1.2f);
  const float fill_color = std::clamp(fill, 0.0f, 1.0f);
  const float stretch_sine = std::sin(3.14159265f * fill_color);
  const float stretch = stretch_sine * stretch_sine;
  const float radius = 4.0f * reference_scale * scale;
  const float padding = 2.0f * reference_scale * scale;
  const float travel = track_width - 2.0f * padding - radius * 2.0f;
  const float center_x = track_min.x + padding + radius + travel * fill_position;
  const float radius_x = radius + stretch * 1.8f * reference_scale * scale;
  const float radius_y =
      std::max(2.0f * reference_scale * scale, radius - stretch * 0.8f * reference_scale * scale);

  ImVec4 track_color = colors::comboBg;
  track_color.x += (colors::accent.x - track_color.x) * fill_color;
  track_color.y += (colors::accent.y - track_color.y) * fill_color;
  track_color.z += (colors::accent.z - track_color.z) * fill_color;
  track_color.w *= alpha;
  ImVec4 border_color = hovered ? ImVec4(0.333f, 0.333f, 0.333f, alpha)
                                : ImVec4(colors::border.x, colors::border.y, colors::border.z,
                                         colors::border.w * alpha);

  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(track_min, track_max, ImGui::GetColorU32(track_color), 0.0f);
  draw->AddRect(track_min, track_max, ImGui::GetColorU32(border_color), 0.0f);

  const float hover = reference_motion.value(ReferenceMotionKey("menu.checkbox", id, "label"),
                                             hovered ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
  const ImVec4 knob_off(0.65f + 0.20f * hover, 0.65f + 0.20f * hover, 0.65f + 0.20f * hover, alpha);
  const ImVec4 knob_color(knob_off.x + (1.0f - knob_off.x) * fill_color,
                          knob_off.y + (1.0f - knob_off.y) * fill_color,
                          knob_off.z + (1.0f - knob_off.z) * fill_color, alpha);

  float knob_min_x = center_x - radius_x;
  float knob_max_x = center_x + radius_x;
  float knob_min_y = center_y - radius_y;
  float knob_max_y = center_y + radius_y;
  const float min_x = track_min.x + padding;
  const float max_x = track_max.x - padding;
  if (knob_min_x < min_x) {
    knob_min_x = min_x;
    knob_max_x = min_x + radius_x * 2.0f;
  }
  if (knob_max_x > max_x) {
    knob_max_x = max_x;
    knob_min_x = max_x - radius_x * 2.0f;
  }
  knob_min_y = std::clamp(knob_min_y, track_min.y + padding, track_max.y - padding);
  knob_max_y = std::clamp(knob_max_y, track_min.y + padding, track_max.y - padding);
  draw->AddRectFilled(ImVec2(knob_min_x, knob_min_y), ImVec2(knob_max_x, knob_max_y),
                      ImGui::GetColorU32(knob_color), 0.0f);
  return changed;
}

bool Slider(const MotionContext& motion, const char* id, const char* label, float* value,
            float minimum, float maximum, float width, float scale, float alpha, ImFont* label_font,
            float step, float display_multiplier, int display_precision,
            const char* display_suffix) {
  (void)motion;
  if (!value || maximum <= minimum) return false;
  if (display_multiplier == 0.0f) display_multiplier = 1.0f;
  display_precision = std::clamp(display_precision, 0, 6);
  if (!display_suffix) display_suffix = "";

  if (!label_font) label_font = ImGui::GetFont();
  const float value_at_frame_start = *value;
  const ImVec2 row_origin = ImGui::GetCursorScreenPos();
  const float label_height = 14.0f * scale;
  std::string label_upper = label ? label : "";
  std::transform(label_upper.begin(), label_upper.end(), label_upper.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  ImGui::GetWindowDrawList()->AddText(
      label_font, label_font->FontSize, row_origin,
      ImGui::GetColorU32(ImVec4(colors::textDim.x, colors::textDim.y, colors::textDim.z,
                                colors::textDim.w * alpha)),
      label_upper.c_str());

  const float height = 12.0f * scale;
  const ImVec2 origin(row_origin.x, row_origin.y + label_height + 1.0f * scale);
  const float start = origin.x + 1.0f * scale;
  const float end = origin.x + width;
  ImGui::PushID(id);
  ImGui::SetCursorScreenPos(origin);
  ImGui::InvisibleButton("##slider", ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool slider_active = ImGui::IsItemActive();
  const bool focused = ImGui::IsItemFocused();
  const bool open_value_editor = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  const ImGuiIO& io = ImGui::GetIO();
  const float modifier_scale = io.KeyShift ? 0.1f : (io.KeyCtrl ? 2.0f : 1.0f);
  bool adjusted = false;
  if (hovered && !slider_active && io.MouseWheel != 0.0f) {
    *value += io.MouseWheel * step * modifier_scale;
    adjusted = true;
  }
  if (focused) {
    float direction = 0.0f;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
      direction -= 1.0f;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
      direction += 1.0f;
    }
    if (direction != 0.0f) {
      *value += step * modifier_scale * direction;
      adjusted = true;
    }
  }
  if (slider_active && !adjusted) {
    if (io.KeyShift || io.KeyCtrl) {
      *value += (io.MouseDelta.x / (end - start)) * (maximum - minimum) * modifier_scale;
    } else {
      const float ratio = std::clamp((io.MousePos.x - start) / (end - start), 0.0f, 1.0f);
      *value = minimum + (maximum - minimum) * ratio;
    }
  }
  *value = std::clamp(*value, minimum, maximum);

  const float visual = std::clamp((*value - minimum) / (maximum - minimum), 0.0f, 1.0f);
  auto& reference_motion = WindowMotion::System();
  const auto& tokens = WindowMotion::Tokens();
  const float animated = reference_motion.value(
      ReferenceMotionKey("menu.slider", id, "fill"), visual,
      slider_active
          ? ::ui::motion::MotionSpec::Spring(42.0f, ::ui::motion::MotionEasing::SpringSnappy)
          : tokens.springSnappy,
      visual);
  const float track_y = origin.y + height * 0.5f;
  const float knob_x = start + (end - start) * animated;
  ImDrawList* draw = ImGui::GetWindowDrawList();

  const float track_h = 4.0f * scale;
  draw->AddRectFilled(ImVec2(start, track_y - track_h * 0.5f),
                      ImVec2(end, track_y + track_h * 0.5f),
                      ImGui::GetColorU32(ImVec4(0.133f, 0.133f, 0.133f, alpha)), 0.0f);
  draw->AddRectFilled(
      ImVec2(start, track_y - track_h * 0.5f), ImVec2(knob_x, track_y + track_h * 0.5f),
      ImGui::GetColorU32(ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, alpha)),
      0.0f);
  draw->AddRectFilled(ImVec2(start, track_y - track_h * 0.5f),
                      ImVec2(knob_x, track_y + track_h * 0.5f),
                      ImGui::GetColorU32(ImVec4(colors::accent.x, colors::accent.y,
                                                colors::accent.z, 0.25f * alpha)),
                      0.0f);

  const float knob_state =
      reference_motion.value(ReferenceMotionKey("menu.slider", id, "knob"),
                             slider_active ? 1.0f : 0.0f, tokens.pressFast, 0.0f);
  const float knob_width = 4.0f * scale * (1.0f + 0.6f * knob_state);
  const float knob_height = 8.0f * scale * (1.0f + 0.6f * knob_state);
  draw->AddRectFilled(ImVec2(knob_x - knob_width * 0.5f, track_y - knob_height * 0.5f),
                      ImVec2(knob_x + knob_width * 0.5f, track_y + knob_height * 0.5f),
                      ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)), 0.0f);

  char display[32]{};
  std::snprintf(display, sizeof(display), "%.*f%s", display_precision, *value * display_multiplier,
                display_suffix);
  const ImVec2 display_size =
      label_font->CalcTextSizeA(label_font->FontSize, FLT_MAX, 0.0f, display);
  const float value_box_width = std::max(26.0f * scale, display_size.x + 10.0f * scale);
  const float value_box_height = 16.0f * scale;
  const ImVec2 value_box_min(row_origin.x + width - value_box_width,
                             row_origin.y + (label_height - value_box_height) * 0.5f);
  const ImVec2 value_box_max(value_box_min.x + value_box_width, value_box_min.y + value_box_height);
  const ImGuiID mode_id = ImGui::GetID("##value_mode");
  int* mode = ImGui::GetStateStorage()->GetIntRef(mode_id, 0);
  const bool value_hovered = ImGui::IsMouseHoveringRect(value_box_min, value_box_max, false);
  const float value_hover =
      reference_motion.value(ReferenceMotionKey("menu.slider", id, "value-box"),
                             (*mode > 0 || value_hovered) ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
  const ImVec4 box_bg(0.02f + 0.047f * value_hover, 0.02f + 0.047f * value_hover,
                      0.02f + 0.047f * value_hover, alpha);
  const ImVec4 box_border(colors::border.x + (0.267f - colors::border.x) * value_hover,
                          colors::border.y + (0.267f - colors::border.y) * value_hover,
                          colors::border.z + (0.267f - colors::border.z) * value_hover,
                          colors::border.w * alpha);
  draw->AddRectFilled(value_box_min, value_box_max, ImGui::GetColorU32(box_bg), 0.0f);
  draw->AddRect(value_box_min, value_box_max, ImGui::GetColorU32(box_border), 0.0f);

  auto commit_value = [&](const char* text) {
    char* end_ptr = nullptr;
    const float parsed = std::strtof(text, &end_ptr);
    if (end_ptr != text) {
      *value = std::clamp(parsed / display_multiplier, minimum, maximum);
    }
  };
  const ImGuiID input_id = ImGui::GetID("##value_input");
  auto& input_buffer = g_slider_value_buffers[input_id];
  if (*mode > 0) {
    if (*mode == 1) {
      std::snprintf(input_buffer.data(), input_buffer.size(), "%.*f", display_precision,
                    *value * display_multiplier);
      ImGui::SetKeyboardFocusHere();
      *mode = 2;
    }
    ImGui::SetCursorScreenPos(value_box_min);
    ImGui::PushFont(label_font);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f * scale, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, box_bg);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImVec4(colors::text.x, colors::text.y, colors::text.z, alpha));
    ImGui::SetNextItemWidth(value_box_width);
    const bool submitted = ImGui::InputText(
        "##value_input", input_buffer.data(), input_buffer.size(),
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_NoHorizontalScroll);
    if (*mode == 2 && ImGui::IsItemActive()) {
      float direction = 0.0f;
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        direction -= 1.0f;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        direction += 1.0f;
      }
      if (direction != 0.0f) {
        commit_value(input_buffer.data());
        *value = std::clamp(*value + direction * step * modifier_scale, minimum, maximum);
        std::snprintf(input_buffer.data(), input_buffer.size(), "%.*f", display_precision,
                      *value * display_multiplier);
      }
    }
    if (submitted) {
      commit_value(input_buffer.data());
      *mode = 0;
    } else if (*mode == 2 && !ImGui::IsItemActive() &&
               (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
      commit_value(input_buffer.data());
      *mode = 0;
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
  } else {
    draw->AddText(label_font, label_font->FontSize,
                  ImVec2(value_box_min.x + (value_box_width - display_size.x) * 0.5f,
                         value_box_min.y + (value_box_height - display_size.y) * 0.5f),
                  ImGui::GetColorU32(ImVec4(colors::text.x, colors::text.y, colors::text.z, alpha)),
                  display);
    ImGui::SetCursorScreenPos(value_box_min);
    ImGui::InvisibleButton("##value_button", ImVec2(value_box_width, value_box_height));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) *mode = 1;
  }
  if (open_value_editor) *mode = 1;

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

  if (label && label[0] != '\0') {
    std::string upper(label);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    ImGui::PushFont(label_font);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(colors::textDim.x, colors::textDim.y,
                                                colors::textDim.z, colors::textDim.w * alpha));
    ImGui::TextUnformatted(upper.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
  }

  const ImVec2 frame_min = ImGui::GetCursorScreenPos();
  const float frame_width = std::max(1.0f, size.x);
  const float frame_height = std::max(1.0f, size.y);
  const bool pressed = ImGui::InvisibleButton("##combo_button", ImVec2(frame_width, frame_height));
  const bool hovered = ImGui::IsItemHovered();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  const ImVec2 frame_max = ImGui::GetItemRectMax();

  // Keep the popup's close animation when the owning child scrolls this anchor out of view.
  // IsRectVisible is part of the public ImGui API, unlike the beta source's ImGuiWindow access.
  const bool anchor_visible = ImGui::IsRectVisible(frame_min, frame_max);

  const float frame_hover = motion.value(ReferenceMotionKey("menu.combo", id, "frame-hover"),
                                         hovered ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
  ImVec4 frame_background =
      MixColor(colors::comboBg, ImVec4(0.067f, 0.067f, 0.067f, 1.0f), frame_hover);
  ImVec4 frame_border = MixColor(colors::border, ImVec4(0.267f, 0.267f, 0.267f, 1.0f), frame_hover);
  frame_background.w *= alpha;
  frame_border.w *= alpha;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(frame_min, frame_max, ImGui::GetColorU32(frame_background), 0.0f);
  draw->AddRect(frame_min, frame_max, ImGui::GetColorU32(frame_border), 0.0f);

  const ImGuiID popup_id = ImGui::GetID("##combo_popup");
  const auto popup_alpha_key = ReferenceMotionKey("menu.combo", id, "popup-alpha");
  const auto popup_slide_key = ReferenceMotionKey("menu.combo", id, "popup-slide");
  const auto arrow_open_key = ReferenceMotionKey("menu.combo", id, "arrow-open");
  const auto arrow_color_key = ReferenceMotionKey("menu.combo", id, "arrow-color");
  const ::ui::motion::MotionSpec slide_open =
      ::ui::motion::MotionSpec::Timed(0.34f, ::ui::motion::MotionEasing::SmootherStep);
  const ::ui::motion::MotionSpec alpha_open =
      ::ui::motion::MotionSpec::Timed(0.26f, ::ui::motion::MotionEasing::EaseOutCubic);
  const ::ui::motion::MotionSpec slide_close =
      ::ui::motion::MotionSpec::Timed(0.30f, ::ui::motion::MotionEasing::SmootherStep);
  const ::ui::motion::MotionSpec alpha_close =
      ::ui::motion::MotionSpec::Timed(0.24f, ::ui::motion::MotionEasing::EaseOutCubic);

  if (pressed) {
    if (!g_combo_was_open[popup_id]) {
      motion.set(popup_alpha_key, 0.0f);
      motion.set(popup_slide_key, 0.0f);
    }
    ImGui::OpenPopup("##combo_popup");
  }

  bool popup_open = ImGui::IsPopupOpen("##combo_popup");
  bool& was_open = g_combo_was_open[popup_id];
  bool& closing = g_combo_closing[popup_id];
  if (pressed) {
    popup_open = true;
    was_open = true;
    closing = false;
  } else if (was_open && !popup_open && !closing) {
    ImGui::OpenPopup("##combo_popup");
    popup_open = true;
    closing = true;
  }
  if (popup_open && !anchor_visible) closing = true;

  const float popup_alpha = std::clamp(motion.value(popup_alpha_key, closing ? 0.0f : 1.0f,
                                                    closing ? alpha_close : alpha_open, 0.0f),
                                       0.0f, 1.0f);
  const float popup_slide = std::clamp(motion.value(popup_slide_key, closing ? 0.0f : 1.0f,
                                                    closing ? slide_close : slide_open, 0.0f),
                                       0.0f, 1.0f);

  const float row_height = 24.0f * scale;
  const int visible_count = std::min<int>(8, static_cast<int>(items.size()));
  const float popup_height = visible_count * row_height + 2.0f * scale;
  ImVec2 popup_position(frame_min.x, frame_max.y);
  bool opens_upward = false;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (popup_position.y + popup_height > viewport->Pos.y + viewport->Size.y) {
    popup_position.y = frame_min.y - popup_height;
    opens_upward = true;
  }
  const float slide_offset = (1.0f - popup_slide) * 5.0f * scale;
  popup_position.y += opens_upward ? slide_offset : -slide_offset;
  popup_position.x = std::floor(popup_position.x + 0.5f);
  popup_position.y = std::floor(popup_position.y + 0.5f);
  if (popup_open) {
    ImGui::SetNextWindowPos(popup_position);
    ImGui::SetNextWindowSize(ImVec2(frame_width, popup_height));
  }

  bool changed = false;
  if (popup_open) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 3.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, colors::comboBg);
    ImGui::PushStyleColor(ImGuiCol_Border, colors::border);
    if (popup_alpha < 1.0f) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_alpha);

    if (ImGui::BeginPopup("##combo_popup", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                               ImGuiWindowFlags_NoTitleBar |
                                               ImGuiWindowFlags_NoSavedSettings)) {
      if (closing && popup_alpha <= 0.02f) ImGui::CloseCurrentPopup();
      ImDrawList* popup_draw = ImGui::GetWindowDrawList();
      const ImVec2 popup_min = ImGui::GetWindowPos();
      const ImVec2 popup_max(popup_min.x + ImGui::GetWindowSize().x,
                             popup_min.y + ImGui::GetWindowSize().y);
      popup_draw->AddRectFilled(
          popup_min, popup_max,
          ImGui::GetColorU32(ImVec4(colors::comboBg.x, colors::comboBg.y, colors::comboBg.z,
                                    colors::comboBg.w * popup_alpha)));
      popup_draw->AddRect(
          popup_min, popup_max,
          ImGui::GetColorU32(ImVec4(colors::border.x, colors::border.y, colors::border.z,
                                    colors::border.w * popup_alpha)));

      if (closing) ImGui::BeginDisabled();
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
      for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        ImGui::PushID(index);
        const ImGuiID item_id = ImGui::GetID("##item_animation");
        ImGui::SetCursorPosX(0.0f);
        const ImVec2 item_min = ImGui::GetCursorScreenPos();
        const float item_width = ImGui::GetContentRegionAvail().x;
        const bool item_clicked = ImGui::InvisibleButton("##item", ImVec2(item_width, row_height));
        const bool item_hovered = ImGui::IsItemHovered();
        if (item_clicked) {
          *current = index;
          changed = true;
          closing = true;
        }

        const float item_hover = motion.value(
            ::ui::motion::MotionKey("menu.combo.item", std::to_string(item_id), "hover"),
            item_hovered ? 1.0f : 0.0f, tokens.hoverFast, 0.0f);
        if (item_hover > 0.001f) {
          popup_draw->AddRectFilled(
              item_min, ImVec2(item_min.x + item_width, item_min.y + row_height),
              ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.04f * item_hover * popup_alpha)));
        }
        ImVec4 item_color =
            *current == index
                ? colors::text
                : MixColor(colors::textDim, ImVec4(0.8f, 0.8f, 0.8f, 1.0f), item_hover);
        item_color.w *= popup_alpha;
        const float item_y =
            std::floor(item_min.y + (row_height - item_font->FontSize) * 0.5f + 0.5f);
        popup_draw->AddText(item_font, item_font->FontSize,
                            ImVec2(item_min.x + 10.0f * scale + 6.0f * scale * item_hover, item_y),
                            ImGui::GetColorU32(item_color), items[index]);
        ImGui::PopID();
      }
      ImGui::PopStyleVar(2);
      if (closing) ImGui::EndDisabled();
      ImGui::EndPopup();
    }

    if (popup_alpha < 1.0f) ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);

    if (closing && popup_alpha <= 0.02f) {
      motion.forget(popup_alpha_key);
      motion.forget(popup_slide_key);
      motion.forget(arrow_open_key);
      motion.forget(arrow_color_key);
      was_open = false;
      closing = false;
    } else {
      was_open = true;
    }
  }

  const char* preview =
      *current >= 0 && *current < static_cast<int>(items.size()) ? items[*current] : "";
  if (preview && preview[0]) {
    const ImVec2 preview_size =
        item_font->CalcTextSizeA(item_font->FontSize, FLT_MAX, 0.0f, preview);
    const float preview_y = std::floor(frame_min.y + (frame_height - preview_size.y) * 0.5f + 0.5f);
    draw->PushClipRect(frame_min, ImVec2(frame_max.x - 30.0f * scale, frame_max.y), true);
    draw->AddText(item_font, item_font->FontSize, ImVec2(frame_min.x + 10.0f * scale, preview_y),
                  ImGui::GetColorU32(ImVec4(colors::text.x, colors::text.y, colors::text.z, alpha)),
                  preview);
    draw->PopClipRect();
  }

  const float arrow_size = 8.0f * scale;
  const ImVec2 arrow_center(frame_max.x - 12.0f * scale - arrow_size * 0.5f,
                            frame_min.y + frame_height * 0.5f);
  const ImVec4 arrow_target = popup_open && !closing
                                  ? colors::accent
                                  : (hovered ? ImVec4(0.8f, 0.8f, 0.8f, 1.0f) : colors::textDim);
  ImVec4 arrow_color =
      motion.color(arrow_color_key, arrow_target, tokens.hoverFast, colors::textDim);
  arrow_color.w *= alpha;
  draw->PathLineTo(ImVec2(arrow_center.x - arrow_size * 0.5f, arrow_center.y - arrow_size * 0.2f));
  draw->PathLineTo(ImVec2(arrow_center.x, arrow_center.y + arrow_size * 0.2f));
  draw->PathLineTo(ImVec2(arrow_center.x + arrow_size * 0.5f, arrow_center.y - arrow_size * 0.2f));
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
                     const std::array<const char*, 2>& labels, int* selected, float width,
                     ImFont* font, float scale, float alpha) {
  if (!selected) return false;
  if (!font) font = ImGui::GetFont();

  const ImVec2 min = ImGui::GetCursorScreenPos();
  const float height = 24.0f * scale;
  const float segment_width = width * 0.5f;
  bool changed = false;

  ImDrawList* draw = ImGui::GetWindowDrawList();

  // 1. Draw track background and border
  const ImVec2 max(min.x + width, min.y + height);
  ImVec4 track_bg = colors::panelHeader;
  track_bg.w *= alpha;
  ImVec4 track_border = colors::border;
  track_border.w *= alpha;

  draw->AddRectFilled(min, max, ImGui::GetColorU32(track_bg), 0.0f);
  draw->AddRect(min, max, ImGui::GetColorU32(track_border), 0.0f, 0, std::max(1.0f, scale));

  // 2. Animate sliding indicator position
  const float target_x = static_cast<float>(*selected) * segment_width;
  const float current_x = motion.system.value(
      ::ui::motion::MotionKey("segment-selector", id ? id : "close_behavior", "slide-x"), target_x,
      motion.tokens.springSoft, target_x);

  // 3. Draw sliding indicator
  const ImVec2 pill_min(min.x + current_x, min.y);
  const ImVec2 pill_max(min.x + current_x + segment_width, min.y + height);

  ImVec4 pill_bg(colors::accent.x, colors::accent.y, colors::accent.z, 0.15f * alpha);
  ImVec4 pill_border(colors::accent.x, colors::accent.y, colors::accent.z, 0.6f * alpha);

  draw->AddRectFilled(pill_min, pill_max, ImGui::GetColorU32(pill_bg), 0.0f);
  draw->AddRect(pill_min, pill_max, ImGui::GetColorU32(pill_border), 0.0f, 0,
                std::max(1.0f, scale));

  // 4. Render segments
  for (int index = 0; index < 2; ++index) {
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

    // Subtle hover background glow
    const float hover_val =
        motion.system.value(::ui::motion::MotionKey("segment-selector", segment_id, "hover"),
                            hovered ? 1.0f : 0.0f, motion.tokens.hoverFast, 0.0f);
    if (hover_val > 0.0f) {
      ImVec4 hover_glow(1.0f, 1.0f, 1.0f, 0.04f * hover_val * alpha);
      draw->AddRectFilled(segment_min, segment_max, ImGui::GetColorU32(hover_glow), 0.0f);
    }

    // Animate text color based on active/hover state
    const bool is_active = (*selected == index);
    const ImVec4 text_target = is_active || hovered ? colors::text : colors::textDim;
    ImVec4 text_color = motion.system.color(
        ::ui::motion::MotionKey("segment-selector", segment_id, "text-color"), text_target,
        is_active ? motion.tokens.selectSharp : motion.tokens.hoverFast, colors::textDim);
    text_color.w *= alpha;

    // Draw text label centered
    const ImVec2 text_size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, labels[index]);
    const ImVec2 text_pos(std::floor(segment_min.x + (segment_width - text_size.x) * 0.5f + 0.5f),
                          std::floor(segment_min.y + (height - text_size.y) * 0.5f + 0.5f));
    draw->AddText(font, font->FontSize, text_pos, ImGui::GetColorU32(text_color), labels[index]);
  }

  ImGui::SetCursorScreenPos(ImVec2(min.x, min.y + height));
  return changed;
}

}  // namespace genie::app::settings_ui
