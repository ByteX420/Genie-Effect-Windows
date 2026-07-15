#pragma once

#include "imgui.h"
#include "menu/motion/motion_context.hpp"

namespace genie::app::settings_ui {

struct MotionContext {
  ::ui::motion::MotionSystem& system;
  const ::ui::motion::MotionTokens& tokens;
};

// Production subset of the ImGuiBase Sequoia settings design. The editor,
// layout-export and demo-host state intentionally stay in the design project.
struct Metrics final {
  static constexpr float kWindowWidth = 738.0f;
  static constexpr float kWindowHeight = 556.0f;
  static constexpr float kWindowRounding = 0.0f;
  static constexpr float kTitlebarHeight = 54.0f;
  static constexpr float kSidebarWidth = 220.0f;
  static constexpr float kSidebarMargin = 12.5f;
  static constexpr float kSidebarContentWidth = 195.0f;
  static constexpr float kNavigationY = 70.0f;
  static constexpr float kNavigationRowHeight = 36.0f;
  static constexpr float kNavigationSpacing = 1.5f;
  // App layout rhythm: 20pt page margins, 16pt card padding, 8pt related
  // spacing and a larger separation between independent groups.
  static constexpr float kPageInset = 20.0f;
  static constexpr float kCardInnerInset = 16.0f;
  static constexpr float kContentInset = kPageInset + kCardInnerInset;
  static constexpr float kRelatedSpacing = 8.0f;
  static constexpr float kGroupSpacing = 20.0f;
  static constexpr float kScrollGutter = 10.0f;
  static constexpr float kScrollBottomPadding = 28.0f;
  static constexpr float kScrollFadeHeight = 16.0f;
  static constexpr float kMainInset = kPageInset;
  static constexpr float kMainTop = 68.0f;
  static constexpr float kCardWidth = 603.0f;
  static constexpr float kCardRounding = 0.0f;
  static constexpr float kCardSpacing = kGroupSpacing;
  static constexpr float kRowHeight = 57.0f;
};

//  beta palette. Alpha is applied by the caller so transitions remain
// consistent across panels and custom draw-list content.
inline constexpr ImU32 kMainBackground = IM_COL32(10, 10, 10, 255);
inline constexpr ImU32 kSidebarBackground = IM_COL32(12, 12, 12, 255);
inline constexpr ImU32 kCardBackground = IM_COL32(17, 17, 17, 255);
inline constexpr ImU32 kPanelHeader = IM_COL32(21, 21, 21, 255);
inline constexpr ImU32 kActiveItem = IM_COL32(99, 102, 241, 255);
inline constexpr ImU32 kText = IM_COL32(238, 238, 238, 255);
inline constexpr ImU32 kMutedText = IM_COL32(85, 85, 85, 255);
inline constexpr ImU32 kBorder = IM_COL32(42, 42, 42, 255);
inline constexpr ImU32 kSeparator = IM_COL32(42, 42, 42, 255);

inline ImU32 WithAlpha(ImU32 color, float alpha) {
  alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
  const auto value = static_cast<ImU32>(
      alpha * 255.0f + 0.5f);
  return (color & 0x00ffffffu) | (value << 24u);
}

inline ImU32 Blend(ImU32 from, ImU32 to, float amount) {
  amount = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
  const ImVec4 a = ImGui::ColorConvertU32ToFloat4(from);
  const ImVec4 b = ImGui::ColorConvertU32ToFloat4(to);
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount,
             a.z + (b.z - a.z) * amount, a.w + (b.w - a.w) * amount));
}

enum class TrafficLightAction {
  kNone,
  kClose,
  kMinimize,
  kZoom,
};

void ApplyStyle(float scale);
void DrawGradientShadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float radius, float alpha,
                        float scale);
void DrawCard(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, float alpha = 1.0f);
void DrawSeparator(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha = 1.0f);
TrafficLightAction DrawTrafficLights(const MotionContext& motion,
                                     ImVec2 window_origin, float scale, float alpha = 1.0f);
bool SidebarItem(const MotionContext& motion, const char* id, const char* label,
                 bool selected, ImVec2 position, ImVec2 size, ImFont* font, float scale,
                 float alpha = 1.0f);

}  // namespace genie::app::settings_ui
