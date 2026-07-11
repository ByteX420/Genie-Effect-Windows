#include "pch.hpp"

#include "app/settings_ui_theme.hpp"

#include <algorithm>
#include <cmath>

namespace genie::app::settings_ui {
namespace {

constexpr float kPi = 3.14159265358979323846f;

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

void DrawLineIcon(ImDrawList* draw, Icon icon, ImVec2 center, float radius, float scale,
                  ImU32 color) {
  const float line = std::max(1.0f, 1.35f * scale);
  const float r = radius * 0.52f;
  switch (icon) {
    case Icon::kGeneral:
      draw->AddCircle(center, r, color, 18, line);
      draw->AddCircleFilled(center, r * 0.32f, color, 12);
      for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * kPi / 4.0f;
        draw->AddLine(
            ImVec2(center.x + std::cos(angle) * r * 1.05f, center.y + std::sin(angle) * r * 1.05f),
            ImVec2(center.x + std::cos(angle) * r * 1.38f, center.y + std::sin(angle) * r * 1.38f),
            color, line);
      }
      break;
    case Icon::kAnimation:
      draw->AddTriangleFilled(ImVec2(center.x - r * 0.55f, center.y - r),
                              ImVec2(center.x + r, center.y),
                              ImVec2(center.x - r * 0.55f, center.y + r), color);
      break;
    case Icon::kApplications: {
      const float cell = r * 0.58f;
      for (int y = -1; y <= 1; y += 2) {
        for (int x = -1; x <= 1; x += 2) {
          const ImVec2 c(center.x + x * r * 0.48f, center.y + y * r * 0.48f);
          draw->AddRectFilled(ImVec2(c.x - cell * 0.5f, c.y - cell * 0.5f),
                              ImVec2(c.x + cell * 0.5f, c.y + cell * 0.5f), color, 1.5f * scale);
        }
      }
      break;
    }
    case Icon::kWindows:
      draw->AddRect(ImVec2(center.x - r, center.y - r * 0.72f),
                    ImVec2(center.x + r, center.y + r * 0.65f), color, 1.5f * scale, 0, line);
      draw->AddLine(ImVec2(center.x - r * 0.35f, center.y + r),
                    ImVec2(center.x + r * 0.35f, center.y + r), color, line);
      draw->AddLine(ImVec2(center.x, center.y + r * 0.65f), ImVec2(center.x, center.y + r), color,
                    line);
      break;
    case Icon::kHotkeys:
      draw->AddRect(ImVec2(center.x - r, center.y - r * 0.65f),
                    ImVec2(center.x + r, center.y + r * 0.65f), color, 2.0f * scale, 0, line);
      for (int y = -1; y <= 1; ++y) {
        for (int x = -2; x <= 2; ++x) {
          draw->AddCircleFilled(ImVec2(center.x + x * r * 0.32f, center.y + y * r * 0.29f),
                                std::max(0.8f, 0.75f * scale), color, 6);
        }
      }
      break;
    case Icon::kDiagnostics:
      draw->AddLine(ImVec2(center.x - r, center.y + r),
                    ImVec2(center.x - r * 0.45f, center.y + r * 0.1f), color, line);
      draw->AddLine(ImVec2(center.x - r * 0.45f, center.y + r * 0.1f),
                    ImVec2(center.x, center.y + r * 0.55f), color, line);
      draw->AddLine(ImVec2(center.x, center.y + r * 0.55f),
                    ImVec2(center.x + r * 0.48f, center.y - r * 0.72f), color, line);
      draw->AddLine(ImVec2(center.x + r * 0.48f, center.y - r * 0.72f),
                    ImVec2(center.x + r, center.y - r * 0.25f), color, line);
      break;
    case Icon::kAbout:
      draw->AddCircle(center, r, color, 20, line);
      draw->AddCircleFilled(ImVec2(center.x, center.y - r * 0.42f), std::max(1.0f, 1.25f * scale),
                            color, 8);
      draw->AddLine(ImVec2(center.x, center.y - r * 0.05f), ImVec2(center.x, center.y + r * 0.55f),
                    color, std::max(1.0f, 1.8f * scale));
      break;
  }
}

}  // namespace

void ApplyStyle(float scale) {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(0.0f, 0.0f);
  style.WindowRounding = Metrics::kWindowRounding * scale;
  style.WindowBorderSize = 0.0f;
  style.ChildRounding = Metrics::kCardRounding * scale;
  style.ChildBorderSize = 0.0f;
  style.PopupRounding = 7.0f * scale;
  style.FramePadding = ImVec2(10.0f * scale, 7.0f * scale);
  style.FrameRounding = 7.0f * scale;
  style.FrameBorderSize = 0.0f;
  style.ItemSpacing = ImVec2(8.0f * scale, 8.0f * scale);
  style.ItemInnerSpacing = ImVec2(7.0f * scale, 5.0f * scale);
  style.ScrollbarSize = 7.0f * scale;
  style.ScrollbarRounding = 8.0f * scale;
  style.GrabMinSize = 14.0f * scale;
  style.GrabRounding = 8.0f * scale;
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;
  style.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(kMainBackground);
  style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  style.Colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.13f, 0.98f);
  style.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(kText);
  style.Colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(kMutedText);
  style.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(kBorder);
  style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.31f);
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1, 1, 1, 0.08f);
  style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1, 1, 1, 0.11f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.27f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.29f, 0.29f, 0.31f, 1.0f);
  style.Colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(kActiveItem);
  style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.04f, 0.52f, 1.0f, 0.72f);
  style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.04f, 0.52f, 1.0f, 1.0f);
  style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.55f, 0.55f, 0.58f, 0.38f);
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.62f, 0.62f, 0.65f, 0.68f);
  style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.62f, 0.62f, 0.65f, 0.85f);
  style.Colors[ImGuiCol_CheckMark] = ImVec4(1, 1, 1, 1);
  style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.95f, 0.95f, 0.97f, 1.0f);
  style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1, 1, 1, 1);
}

float Animate(ImGuiID id, float target, float speed) {
  ImGuiStorage* storage = ImGui::GetStateStorage();
  const float current = storage->GetFloat(id, target);
  const float blend = 1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime);
  const float next = current + (target - current) * blend;
  storage->SetFloat(id, next);
  return next;
}

void DrawGradientShadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float radius, float alpha,
                        float scale) {
  for (int i = 1; i <= 4; ++i) {
    const float extent = static_cast<float>(i) * scale;
    const float layer_alpha = alpha * (1.0f - static_cast<float>(i) / 4.0f);
    draw->AddRectFilled(ImVec2(min.x - extent, min.y - extent + 2.0f * scale),
                        ImVec2(max.x + extent, max.y + extent + 4.0f * scale),
                        IM_COL32(0, 0, 0, static_cast<int>(layer_alpha * 255.0f)), radius + extent);
  }
}

void DrawAppMark(ImDrawList* draw, ImVec2 center, float radius, float scale) {
  draw->AddCircleFilled(center, radius, IM_COL32(68, 132, 205, 255), 36);
  draw->AddCircle(center, radius, IM_COL32(255, 255, 255, 40), 36, std::max(1.0f, scale));
  const float inner = radius * 0.58f;
  draw->AddCircle(center, inner, IM_COL32(255, 255, 255, 235), 28, std::max(1.0f, 1.8f * scale));
  draw->AddCircleFilled(center, inner * 0.28f, IM_COL32(255, 255, 255, 235), 20);
  for (int i = 0; i < 8; ++i) {
    const float angle = static_cast<float>(i) * kPi / 4.0f;
    draw->AddLine(ImVec2(center.x + std::cos(angle) * inner * 1.05f,
                         center.y + std::sin(angle) * inner * 1.05f),
                  ImVec2(center.x + std::cos(angle) * inner * 1.34f,
                         center.y + std::sin(angle) * inner * 1.34f),
                  IM_COL32(255, 255, 255, 235), std::max(1.0f, 1.7f * scale));
  }
}

void DrawSidebarIcon(ImDrawList* draw, Icon icon, ImVec2 center, float radius, float scale,
                     float alpha) {
  constexpr ImU32 backgrounds[] = {
      IM_COL32(142, 142, 147, 255), IM_COL32(10, 132, 255, 255), IM_COL32(48, 209, 88, 255),
      IM_COL32(10, 132, 255, 255),  IM_COL32(175, 82, 222, 255), IM_COL32(255, 149, 0, 255),
      IM_COL32(142, 142, 147, 255),
  };
  const size_t index = static_cast<size_t>(icon);
  draw->AddRectFilled(ImVec2(center.x - radius, center.y - radius),
                      ImVec2(center.x + radius, center.y + radius),
                      Alpha(backgrounds[index], alpha), 5.0f * scale);
  DrawLineIcon(draw, icon, center, radius, scale, Alpha(IM_COL32_WHITE, alpha));
}

void DrawSearchIcon(ImDrawList* draw, ImVec2 center, float scale, float alpha) {
  const float radius = 5.5f * scale;
  const ImU32 color = Alpha(kMutedText, alpha * 0.9f);
  draw->AddCircle(center, radius, color, 18, 1.5f * scale);
  draw->AddLine(ImVec2(center.x + radius * 0.7f, center.y + radius * 0.7f),
                ImVec2(center.x + radius * 1.5f, center.y + radius * 1.5f), color, 1.5f * scale);
}

void DrawCard(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, float alpha) {
  DrawGradientShadow(draw, min, max, Metrics::kCardRounding * scale, 0.12f * alpha, scale);
  draw->AddRectFilled(min, max, Alpha(kCardBackground, alpha), Metrics::kCardRounding * scale);
  draw->AddRect(min, max, Alpha(kBorder, alpha), Metrics::kCardRounding * scale, 0,
                std::max(1.0f, scale));
}

void DrawSeparator(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha) {
  draw->AddLine(min, max, Alpha(kSeparator, alpha), 1.0f);
}

TrafficLightAction DrawTrafficLights(ImVec2 window_origin, float scale, float alpha) {
  constexpr const char* ids[] = {"##traffic_close", "##traffic_minimize", "##traffic_zoom"};
  constexpr ImU32 colors[] = {kAppleRed, kAppleYellow, kAppleGreen};
  const ImVec2 base(window_origin.x + 35.0f * scale, window_origin.y + 35.0f * scale);
  const float radius = 8.0f * scale;
  const float spacing = 26.0f * scale;
  bool group_hovered = false;
  bool clicked[3]{};
  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (int index = 0; index < 3; ++index) {
    const ImVec2 center(base.x + spacing * index, base.y);
    ImGui::SetCursorScreenPos(
        ImVec2(center.x - radius - 3.0f * scale, center.y - radius - 3.0f * scale));
    ImGui::InvisibleButton(ids[index],
                           ImVec2((radius + 3.0f * scale) * 2.0f, (radius + 3.0f * scale) * 2.0f));
    group_hovered = group_hovered || ImGui::IsItemHovered();
    clicked[index] = ImGui::IsItemClicked();
    draw->AddCircleFilled(center, radius, Alpha(colors[index], alpha), 24);
  }
  const float hover = Animate(ImGui::GetID("##traffic_group_hover"), group_hovered ? 1.0f : 0.0f);
  if (hover > 0.01f) {
    const ImU32 icon = IM_COL32(0, 0, 0, static_cast<int>(160.0f * hover * alpha));
    const ImVec2 red = base;
    const ImVec2 yellow(base.x + spacing, base.y);
    const ImVec2 green(base.x + spacing * 2.0f, base.y);
    draw->AddLine(ImVec2(red.x - 2.5f * scale, red.y - 2.5f * scale),
                  ImVec2(red.x + 2.5f * scale, red.y + 2.5f * scale), icon, 1.5f * scale);
    draw->AddLine(ImVec2(red.x + 2.5f * scale, red.y - 2.5f * scale),
                  ImVec2(red.x - 2.5f * scale, red.y + 2.5f * scale), icon, 1.5f * scale);
    draw->AddLine(ImVec2(yellow.x - 3.0f * scale, yellow.y),
                  ImVec2(yellow.x + 3.0f * scale, yellow.y), icon, 1.5f * scale);
    draw->AddLine(ImVec2(green.x - 2.7f * scale, green.y), ImVec2(green.x + 2.7f * scale, green.y),
                  icon, 1.4f * scale);
    draw->AddLine(ImVec2(green.x, green.y - 2.7f * scale), ImVec2(green.x, green.y + 2.7f * scale),
                  icon, 1.4f * scale);
  }
  if (clicked[0]) return TrafficLightAction::kClose;
  if (clicked[1]) return TrafficLightAction::kMinimize;
  if (clicked[2]) return TrafficLightAction::kZoom;
  return TrafficLightAction::kNone;
}

bool SidebarItem(const char* id, const char* label, Icon icon, bool selected, ImVec2 position,
                 ImVec2 size, ImFont* font, float scale, float alpha) {
  ImGui::SetCursorScreenPos(position);
  const bool pressed = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool focused = ImGui::IsItemFocused();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  const float hover = Animate(ImGui::GetID(id), hovered || focused ? 1.0f : 0.0f);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  (void)icon;
  if (selected) {
    draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
                        Alpha(kActiveItem, alpha), 5.0f * scale);
  } else if (hover > 0.01f) {
    draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
                        IM_COL32(255, 255, 255, static_cast<int>(18.0f * hover * alpha)),
                        5.0f * scale);
  }
  const float font_size = 16.0f * scale;
  const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, label);
  draw->AddText(font, font_size,
                ImVec2(position.x + 18.0f * scale, position.y + (size.y - text_size.y) * 0.5f),
                Alpha(selected ? IM_COL32_WHITE
                               : Mix(IM_COL32(218, 218, 223, 255), IM_COL32_WHITE, hover * 0.35f),
                      alpha),
                label);
  return pressed;
}

}  // namespace genie::app::settings_ui
