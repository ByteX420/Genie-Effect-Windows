#include "pch.hpp"

#include "app/settings_ui_theme.hpp"

#include <algorithm>

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

}  // namespace

void ApplyStyle(float scale) {
  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle();
  ImGui::StyleColorsDark(&style);
  style.WindowPadding = ImVec2(0.0f, 0.0f);
  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 0.0f;
  style.ChildRounding = 0.0f;
  style.ChildBorderSize = 0.0f;
  style.PopupRounding = 0.0f;
  style.PopupBorderSize = 0.0f;
  style.FramePadding = ImVec2(6.0f * scale, 4.0f * scale);
  style.FrameRounding = 0.0f;
  style.FrameBorderSize = 1.0f * scale;
  style.ItemSpacing = ImVec2(6.0f * scale, 4.0f * scale);
  style.ItemInnerSpacing = ImVec2(4.0f * scale, 4.0f * scale);
  style.ScrollbarSize = 14.0f * scale;
  style.ScrollbarRounding = 0.0f;
  style.GrabMinSize = 10.0f * scale;
  style.GrabRounding = 4.0f * scale;
  style.TabRounding = 0.0f;
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;
  style.Colors[ImGuiCol_WindowBg] = colors::main;
  style.Colors[ImGuiCol_ChildBg] = colors::panel;
  style.Colors[ImGuiCol_PopupBg] = colors::comboBg;
  style.Colors[ImGuiCol_Text] = colors::text;
  style.Colors[ImGuiCol_TextDisabled] = colors::textDim;
  style.Colors[ImGuiCol_Border] = colors::border;
  style.Colors[ImGuiCol_FrameBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.067f, 0.067f, 0.067f, 1.0f);
  style.Colors[ImGuiCol_FrameBgActive] = colors::panelHeader;
  style.Colors[ImGuiCol_Button] = colors::panelHeader;
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.11f, 0.11f, 0.11f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);
  style.Colors[ImGuiCol_Header] =
      ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.20f);
  style.Colors[ImGuiCol_HeaderHovered] =
      ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.28f);
  style.Colors[ImGuiCol_HeaderActive] =
      ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.36f);
  style.Colors[ImGuiCol_CheckMark] = colors::accent;
  style.Colors[ImGuiCol_SliderGrab] = colors::accent;
  style.Colors[ImGuiCol_SliderGrabActive] = colors::accent;
  style.Colors[ImGuiCol_TextSelectedBg] =
      ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.25f);
  style.Colors[ImGuiCol_ScrollbarBg] = colors::panel;
  style.Colors[ImGuiCol_ScrollbarGrab] = colors::border;
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
  style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
}

void DrawGradientShadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float radius, float alpha,
                        float scale) {
  (void)radius;
  for (int i = 1; i <= 4; ++i) {
    const float extent = static_cast<float>(i) * scale;
    const float layer_alpha = alpha * (1.0f - static_cast<float>(i) / 4.0f);
    draw->AddRectFilled(ImVec2(min.x - extent, min.y - extent + 2.0f * scale),
                        ImVec2(max.x + extent, max.y + extent + 4.0f * scale),
                        IM_COL32(0, 0, 0, static_cast<int>(layer_alpha * 255.0f)), 0.0f);
  }
}

void DrawCard(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, float alpha) {
  DrawGradientShadow(draw, min, max, Metrics::kCardRounding * scale, 0.12f * alpha, scale);
  ImVec4 panel = colors::panel;
  panel.w *= alpha;
  ImVec4 border = colors::border;
  border.w *= alpha;
  draw->AddRectFilled(min, max, ImGui::GetColorU32(panel), 0.0f);
  draw->AddRect(min, max, ImGui::GetColorU32(border), 0.0f, 0, std::max(1.0f, scale));
}

void DrawSeparator(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha) {
  draw->AddLine(min, max, Alpha(kSeparator, alpha), 1.0f);
}

TrafficLightAction DrawTrafficLights(const MotionContext& motion, ImVec2 window_origin, float scale,
                                     float alpha) {
  constexpr const char* ids[] = {"##traffic_close", "##traffic_minimize", "##traffic_zoom"};
  const ImVec2 base(window_origin.x + 35.0f * scale, window_origin.y + 35.0f * scale);
  const float half_size = 8.0f * scale;
  const float spacing = 26.0f * scale;
  bool clicked[3]{};
  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (int index = 0; index < 3; ++index) {
    const ImVec2 center(base.x + spacing * index, base.y);
    const ImVec2 min(center.x - half_size, center.y - half_size);
    const ImVec2 max(center.x + half_size, center.y + half_size);
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(ids[index], ImVec2(half_size * 2.0f, half_size * 2.0f));
    const bool hovered = ImGui::IsItemHovered();
    clicked[index] = ImGui::IsItemClicked();
    const float hover =
        motion.system.value(::ui::motion::MotionKey("window_controls", ids[index], "hover"),
                            hovered ? 1.0f : 0.0f, motion.tokens.hoverFast);
    const ImU32 hover_color = index == 0 ? IM_COL32(190, 62, 62, 255) : kActiveItem;
    draw->AddRectFilled(min, max, Alpha(Mix(kPanelHeader, hover_color, hover), alpha), 0.0f);
    draw->AddRect(min, max, Alpha(Mix(kBorder, hover_color, hover), alpha), 0.0f, 0,
                  std::max(1.0f, scale));
    const ImU32 icon = Alpha(Mix(kMutedText, kText, hover), alpha);
    if (index == 0) {
      draw->AddLine(ImVec2(center.x - 3.0f * scale, center.y - 3.0f * scale),
                    ImVec2(center.x + 3.0f * scale, center.y + 3.0f * scale), icon, 1.5f * scale);
      draw->AddLine(ImVec2(center.x + 3.0f * scale, center.y - 3.0f * scale),
                    ImVec2(center.x - 3.0f * scale, center.y + 3.0f * scale), icon, 1.5f * scale);
    } else if (index == 1) {
      draw->AddLine(ImVec2(center.x - 3.5f * scale, center.y),
                    ImVec2(center.x + 3.5f * scale, center.y), icon, 1.5f * scale);
    } else {
      draw->AddRect(ImVec2(center.x - 3.0f * scale, center.y - 3.0f * scale),
                    ImVec2(center.x + 3.0f * scale, center.y + 3.0f * scale), icon, 0.0f, 0,
                    1.2f * scale);
    }
  }
  if (clicked[0]) return TrafficLightAction::kClose;
  if (clicked[1]) return TrafficLightAction::kMinimize;
  if (clicked[2]) return TrafficLightAction::kZoom;
  return TrafficLightAction::kNone;
}

bool SidebarItem(const MotionContext& motion, const char* id, const char* label, bool selected,
                 ImVec2 position, ImVec2 size, ImFont* font, float scale, float alpha) {
  ImGui::SetCursorScreenPos(position);
  const bool pressed = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool focused = ImGui::IsItemFocused();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (selected) {
    const float line_height = 18.0f * scale;
    const float line_width = 2.0f * scale;
    const ImVec2 target(position.x, position.y + (size.y - line_height) * 0.5f);
    const ImVec2 rail =
        motion.system.vec2(::ui::motion::MotionKey("sidebar-selection", "main", "line-pos"), target,
                           motion.tokens.springSoft, target);
    const float rail_alpha =
        motion.system.value(::ui::motion::MotionKey("sidebar-selection", "main", "line-alpha"),
                            1.0f, motion.tokens.fadeFast, 1.0f);
    const float difference = target.y - rail.y;
    const float stretch = std::min(std::abs(difference) * 0.55f, line_height * 1.25f);
    float top = rail.y;
    float bottom = rail.y + line_height;
    if (difference > 0.0f) {
      bottom += stretch;
    } else if (difference < 0.0f) {
      top -= stretch;
    }
    draw->AddRectFilled(ImVec2(rail.x, top), ImVec2(rail.x + line_width, bottom),
                        ImGui::GetColorU32(ImVec4(colors::accent.x, colors::accent.y,
                                                  colors::accent.z, rail_alpha * alpha)),
                        0.0f);
  }
  const float font_size = 16.0f * scale;
  const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, label);
  const ImVec4 text_target = selected || hovered || focused ? colors::text : colors::textDim;
  ImVec4 text_color = motion.system.color(
      ::ui::motion::MotionKey("sidebar-main", id, "text"), text_target,
      selected ? motion.tokens.selectSharp : motion.tokens.hoverFast, colors::textDim);
  text_color.w *= alpha;
  draw->AddText(font, font_size,
                ImVec2(position.x + 18.0f * scale, position.y + (size.y - text_size.y) * 0.5f),
                ImGui::GetColorU32(text_color), label);
  return pressed;
}

}  // namespace genie::app::settings_ui
