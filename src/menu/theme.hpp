#pragma once

#include "third_party/imgui/imgui.h"

// Shared color palette for the menu UI (matched to web mock).
namespace colors {
  inline const ImVec4 main        = ImVec4( 0.039f, 0.039f, 0.039f, 0.62f );  // #0a0a0a - slightly lower lets more blur through
  inline const ImVec4 panel       = ImVec4( 0.066f, 0.066f, 0.066f, 0.68f );  // #111111 - translucent panels show parent blur
  inline const ImVec4 border      = ImVec4( 0.165f, 0.165f, 0.165f, 1.0f );   // #2a2a2a
  inline const ImVec4 panelHeader = ImVec4( 0.082f, 0.082f, 0.082f, 0.78f );  // #151515 - subtle header translucency
  inline ImVec4 accent            = ImVec4( 0.388f, 0.400f, 0.945f, 1.0f );   // rgb(99, 102, 241)
  inline const ImVec4 text        = ImVec4( 0.933f, 0.933f, 0.933f, 1.0f );   // #eeeeee
  inline const ImVec4 textDim     = ImVec4( 0.333f, 0.333f, 0.333f, 1.0f );   // #555555

  inline const ImVec4 sidebar       = ImVec4( 0.047f, 0.047f, 0.047f, 0.65f );  // #0c0c0c - translucent sidebar
  inline const ImVec4 subLifetimeBg = ImVec4( 0.039f, 0.039f, 0.039f, 0.72f );  // #0a0a0a
  inline const ImVec4 comboBg       = ImVec4( 0.019f, 0.019f, 0.019f, 0.88f );  // #050505
}  // namespace colors

// Global Icon Reference (set by ui.cpp)
extern void* g_MenuArrowIcon;
extern void* g_MenuConfigIcon;
