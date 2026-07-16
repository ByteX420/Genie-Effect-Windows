#pragma once

#include "menu/motion/motion.hpp"
#include "menu/motion/motion_tokens.hpp"

namespace WindowMotion {
inline ui::motion::MotionSystem g_system{};
inline ui::motion::MotionTokens g_tokens = ui::motion::MotionTokens::Default();

inline ui::motion::MotionSystem& System() { return g_system; }

inline ui::motion::MotionTokens& Tokens() { return g_tokens; }

inline void BeginFrame(float deltaTime) { g_system.beginFrame(deltaTime); }

inline void Reset() { g_system.clear(); }

inline void SetReducedMotion(bool enabled) {
  g_system.setReducedMotion(enabled);
  g_tokens = enabled ? ui::motion::MotionTokens::Reduced() : ui::motion::MotionTokens::Default();
}

inline bool ReducedMotion() { return g_system.reducedMotion(); }

// Example:
// float alpha = WindowMotion::System().value(
//     ui::motion::MotionKey( "panel", "info", "alpha" ),
//     isOpen ? 1.0f : 0.0f,
//     WindowMotion::Tokens().panelEnterFade,
//     0.0f );
//
// ImVec2 offset = WindowMotion::System().vec2(
//     ui::motion::MotionKey( "panel", "info", "offset" ),
//     isOpen ? ImVec2( 0.0f, 0.0f ) : ImVec2( 0.0f, 10.0f ),
//     WindowMotion::Tokens().panelEnterOffset,
//     ImVec2( 0.0f, 10.0f ) );
}  // namespace WindowMotion
