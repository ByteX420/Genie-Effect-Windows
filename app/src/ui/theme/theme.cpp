#include "pch.hpp"

#include "ui/theme/theme.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "ui/theme/theme_tokens.hpp"

namespace genie::ui::theme {
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
  style.Colors[ImGuiCol_WindowBg] = ui::theme::kMainColor;
  style.Colors[ImGuiCol_ChildBg] = ui::theme::kPanelColor;
  style.Colors[ImGuiCol_PopupBg] = ui::theme::kComboBackgroundColor;
  style.Colors[ImGuiCol_Text] = ui::theme::kTextColor;
  style.Colors[ImGuiCol_TextDisabled] = ui::theme::kTextDimColor;
  style.Colors[ImGuiCol_Border] = ui::theme::kBorderColor;
  style.Colors[ImGuiCol_FrameBg] = ui::theme::kComboBackgroundColor;
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
  style.Colors[ImGuiCol_FrameBgActive] = ui::theme::kPanelHeaderColor;
  style.Colors[ImGuiCol_Button] = ui::theme::kPanelHeaderColor;
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
  style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
  style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
  style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
  style.Colors[ImGuiCol_CheckMark] = ui::theme::kAccentColor;
  style.Colors[ImGuiCol_SliderGrab] = ui::theme::kAccentColor;
  style.Colors[ImGuiCol_SliderGrabActive] = ui::theme::kTextColor;
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
  ImVec4 panel = ui::theme::kPanelColor;
  panel.w *= alpha;
  ImVec4 border = ui::theme::kBorderColor;
  border.w *= 0.85f * alpha;
  draw->AddRectFilled(min, max, ImGui::GetColorU32(panel), rounding);
  draw->AddRect(min, max, ImGui::GetColorU32(border), rounding, 0, std::max(1.0f, scale));
}

void DrawSeparator(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha) {
  draw->AddLine(min, max, Alpha(kSeparator, alpha * 0.7f), 1.0f);
}

TrafficLightAction DrawTrafficLights(const motion::MotionContext& motion, ImVec2 window_origin,
                                     float scale, float alpha) {
  constexpr const char* ids[] = {"##traffic_close", "##traffic_minimize", "##traffic_zoom"};
  // Compact macOS-style dots, left-aligned with the nav column.
  const float inset = Metrics::kSidebarMargin * scale;
  const float half_size = 6.0f * scale;
  const float spacing = 20.0f * scale;
  const ImVec2 base(window_origin.x + inset + half_size + 2.0f * scale,
                    window_origin.y + 18.0f * scale);
  // Hit pad must leave a gap between targets (spacing - 2*(half+pad) > 0) so two lights
  // cannot be under the cursor at once — was 5px and overlapped by ~2px.
  const float hit_pad = 3.0f * scale;
  const float hit_half = half_size + hit_pad;

  ImVec2 centers[3]{};
  bool clicked[3]{};
  bool pressed[3]{};
  bool item_hovered[3]{};
  for (int index = 0; index < 3; ++index) {
    centers[index] = ImVec2(base.x + spacing * static_cast<float>(index), base.y);
    const ImVec2 hit_min(centers[index].x - hit_half, centers[index].y - hit_half);
    ImGui::SetCursorScreenPos(hit_min);
    ImGui::InvisibleButton(ids[index], ImVec2(hit_half * 2.0f, hit_half * 2.0f));
    // Allow hover even if a (legacy) popup is up — combo itself is non-modal now.
    item_hovered[index] = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup |
                                               ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    pressed[index] = ImGui::IsItemActive();
    clicked[index] = ImGui::IsItemClicked();
  }

  // Exactly one light may own hover (later submitted items sit on top if anything overlaps).
  int exclusive = -1;
  for (int index = 0; index < 3; ++index) {
    if (item_hovered[index]) exclusive = index;
  }
  if (exclusive >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (int index = 0; index < 3; ++index) {
    const bool is_hot = exclusive == index;
    // Snap non-hot targets to 0 so exit/enter never leave two glyphs mid-fade.
    const float hover =
        motion.system.AnimateValue(ui::motion::MotionKey("window_controls", ids[index], "hover"),
                                   is_hot ? 1.0f : 0.0f, motion.tokens.hover_fast);
    const float press = motion.system.AnimateValue(
        ui::motion::MotionKey("window_controls", ids[index], "press"),
        is_hot && pressed[index] ? 1.0f : 0.0f, motion.tokens.press_fast);
    const float grow = 1.0f + 0.08f * hover - 0.06f * press;
    const float r = half_size * grow;
    const ImVec2 center = centers[index];
    // Quiet idle dots; only tint on hover (close → red, others → slight lift).
    const ImU32 idle = IM_COL32(58, 58, 62, 255);
    const ImU32 hover_col =
        index == 0 ? IM_COL32(200, 72, 72, 255)
                   : (index == 1 ? IM_COL32(200, 160, 70, 255) : IM_COL32(90, 170, 100, 255));
    // Only the exclusive hot light tints/grows — ignore residual motion on the others.
    const float visual_hover = is_hot ? hover : 0.0f;
    draw->AddCircleFilled(center, is_hot ? r : half_size,
                          Alpha(Mix(idle, hover_col, visual_hover), alpha), 24);
    if (is_hot && hover > 0.15f) {
      // Glyph only on the single hot light — never on a fading neighbour.
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
                      ImVec2(center.x + 2.4f * scale, center.y + 2.4f * scale), icon, 1.0f * scale,
                      0, 1.0f);
      }
    }
  }
  if (clicked[0]) return TrafficLightAction::kClose;
  if (clicked[1]) return TrafficLightAction::kMinimize;
  if (clicked[2]) return TrafficLightAction::kZoom;
  return TrafficLightAction::kNone;
}

bool SidebarItem(const motion::MotionContext& motion, const char* id, const char* label,
                 bool selected, ImVec2 position, ImVec2 size, ImFont* regular, ImFont* emphasis,
                 float scale, float alpha) {
  if (!regular) regular = ImGui::GetFont();
  if (!emphasis) emphasis = regular;
  ImGui::SetCursorScreenPos(position);
  const bool pressed = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool focused = ImGui::IsItemFocused();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  ImDrawList* draw = ImGui::GetWindowDrawList();

  const bool is_hot = hovered || focused;
  const float hover =
      motion.system.AnimateValue(ui::motion::MotionKey("sidebar-main", id, "hover"),
                                 is_hot ? 1.0f : 0.0f, motion.tokens.hover_fast, 0.0f);
  const float select =
      motion.system.AnimateValue(ui::motion::MotionKey("sidebar-main", id, "select"),
                                 selected ? 1.0f : 0.0f, motion.tokens.select_sharp, 0.0f);
  const float rounding = 8.0f * scale;
  // Paint hover only while this item is actually hot — residual motion on a previous
  // row must not leave a second pill lit when the pointer already moved on.
  const float visual_hover = is_hot ? hover : 0.0f;
  if (visual_hover > 0.001f || select > 0.001f) {
    // Quieter pill — selected a bit stronger, hover barely there.
    // Selection is the pill + weight only (no sliding rail between items).
    const float fill_alpha = 0.10f * select + 0.04f * visual_hover * (1.0f - select);
    draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
                        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, fill_alpha * alpha)), rounding);
  }

  // Weight rule: selected nav uses SemiBold (emphasis), idle uses Regular.
  ImFont* font = selected ? emphasis : regular;
  const float font_size = font->FontSize;
  const ImVec4 text_target = selected || is_hot ? ui::theme::kTextColor : ui::theme::kTextDimColor;
  ImVec4 text_color = motion.system.AnimateColor(
      ui::motion::MotionKey("sidebar-main", id, "text"), text_target,
      selected ? motion.tokens.select_sharp : motion.tokens.hover_fast, ui::theme::kTextDimColor);
  text_color.w *= alpha;
  draw->AddText(font, font_size,
                ImVec2(std::floor(position.x + 14.0f * scale + 0.5f),
                       CenteredTextTop(font, position.y, size.y)),
                ImGui::GetColorU32(text_color), label);
  return pressed;
}

}  // namespace genie::ui::theme
