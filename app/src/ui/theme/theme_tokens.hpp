#pragma once

#include "imgui.h"

// Shared color palette for the menu UI.
// Cool zinc neutrals, high-contrast type, no brand-purple accent.
namespace minimize::ui::theme {
inline const ImVec4 kMainColor = ImVec4(0.043f, 0.043f, 0.047f, 0.72f);         // #0b0b0c
inline const ImVec4 kPanelColor = ImVec4(0.078f, 0.078f, 0.086f, 0.92f);        // #141416
inline const ImVec4 kBorderColor = ImVec4(0.165f, 0.165f, 0.176f, 1.0f);        // #2a2a2d
inline const ImVec4 kPanelHeaderColor = ImVec4(0.102f, 0.102f, 0.110f, 0.96f);  // #1a1a1c
// Interactive accent: cool near-white (selected rails, active fills).
inline const ImVec4 kAccentColor = ImVec4(0.910f, 0.910f, 0.925f, 1.0f);   // #e8e8ec
inline const ImVec4 kTextColor = ImVec4(0.925f, 0.925f, 0.933f, 1.0f);     // #ececee
inline const ImVec4 kTextDimColor = ImVec4(0.545f, 0.545f, 0.576f, 1.0f);  // #8b8b93

inline const ImVec4 kSidebarColor = ImVec4(0.051f, 0.051f, 0.055f, 0.55f);  // #0d0d0e
inline const ImVec4 kSubNamespaceBackgroundColor = ImVec4(0.043f, 0.043f, 0.047f, 0.80f);
inline const ImVec4 kComboBackgroundColor = ImVec4(0.063f, 0.063f, 0.071f, 0.98f);  // #101012
}  // namespace minimize::ui::theme
