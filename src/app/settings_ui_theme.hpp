#pragma once

#include "imgui.h"

namespace genie::app::settings_ui {

// Production subset of the ImGuiBase Sequoia settings design. The editor,
// layout-export and demo-host state intentionally stay in the design project.
struct Metrics final {
  static constexpr float kWindowWidth = 738.0f;
  static constexpr float kWindowHeight = 556.0f;
  static constexpr float kWindowRounding = 12.0f;
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
  static constexpr float kCardRounding = 8.0f;
  static constexpr float kCardSpacing = kGroupSpacing;
  static constexpr float kRowHeight = 57.0f;
};

inline constexpr ImU32 kMainBackground = IM_COL32(30, 30, 32, 255);
inline constexpr ImU32 kSidebarBackground = IM_COL32(40, 42, 40, 255);
inline constexpr ImU32 kCardBackground = IM_COL32(44, 44, 46, 255);
inline constexpr ImU32 kActiveItem = IM_COL32(10, 132, 255, 255);
inline constexpr ImU32 kText = IM_COL32(245, 245, 247, 255);
inline constexpr ImU32 kMutedText = IM_COL32(160, 160, 165, 255);
inline constexpr ImU32 kBorder = IM_COL32(110, 110, 116, 60);
inline constexpr ImU32 kSeparator = IM_COL32(110, 110, 116, 92);
inline constexpr ImU32 kAppleRed = IM_COL32(255, 95, 87, 255);
inline constexpr ImU32 kAppleYellow = IM_COL32(254, 188, 46, 255);
inline constexpr ImU32 kAppleGreen = IM_COL32(40, 200, 64, 255);

enum class Icon {
  kGeneral,
  kAnimation,
  kApplications,
  kWindows,
  kHotkeys,
  kDiagnostics,
  kAbout,
};

enum class TrafficLightAction {
  kNone,
  kClose,
  kMinimize,
  kZoom,
};

void ApplyStyle(float scale);
float Animate(ImGuiID id, float target, float speed = 15.0f);
void DrawGradientShadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float radius, float alpha,
                        float scale);
void DrawAppMark(ImDrawList* draw, ImVec2 center, float radius, float scale);
void DrawSidebarIcon(ImDrawList* draw, Icon icon, ImVec2 center, float radius, float scale,
                     float alpha = 1.0f);
void DrawSearchIcon(ImDrawList* draw, ImVec2 center, float scale, float alpha = 1.0f);
void DrawCard(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, float alpha = 1.0f);
void DrawSeparator(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha = 1.0f);
TrafficLightAction DrawTrafficLights(ImVec2 window_origin, float scale, float alpha = 1.0f);
bool SidebarItem(const char* id, const char* label, Icon icon, bool selected, ImVec2 position,
                 ImVec2 size, ImFont* font, float scale, float alpha = 1.0f);

}  // namespace genie::app::settings_ui
