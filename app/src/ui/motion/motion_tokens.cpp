#include "pch.hpp"

#include "ui/motion/motion_tokens.hpp"

namespace minimize::ui::motion {

MotionTokens MotionTokens::Default() {
  MotionTokens tokens{};
  tokens.hover_fast = MotionSpec::Timed(0.14f, MotionEasing::kEaseOutCubic);
  tokens.hover_soft = MotionSpec::Timed(0.18f, MotionEasing::kEaseOutCubic);
  tokens.press_fast = MotionSpec::Timed(0.09f, MotionEasing::kEaseOutCubic);
  tokens.fade_fast = MotionSpec::Timed(0.18f, MotionEasing::kEaseOutCubic);
  tokens.fade_medium = MotionSpec::Timed(0.28f, MotionEasing::kEaseOutCubic);
  tokens.fade_slow = MotionSpec::Timed(0.40f, MotionEasing::kSmootherStep);
  tokens.slide_soft = MotionSpec::Timed(0.30f, MotionEasing::kSmootherStep);
  tokens.slide_medium = MotionSpec::Timed(0.36f, MotionEasing::kSmootherStep);
  tokens.slide_large = MotionSpec::Timed(0.48f, MotionEasing::kSmootherStep);
  tokens.panel_enter_fade = MotionSpec::Timed(0.34f, MotionEasing::kEaseOutCubic);
  tokens.panel_enter_offset = MotionSpec::Timed(0.40f, MotionEasing::kSmootherStep);
  tokens.popup_open = MotionSpec::Timed(0.30f, MotionEasing::kSmootherStep);
  tokens.popup_close = MotionSpec::Timed(0.18f, MotionEasing::kEaseInQuad);
  tokens.tab_fade = MotionSpec::Timed(0.22f, MotionEasing::kEaseOutCubic);
  tokens.tab_slide = MotionSpec::Timed(0.32f, MotionEasing::kSmootherStep);
  tokens.search_fade = MotionSpec::Timed(0.18f, MotionEasing::kEaseOutCubic);
  tokens.search_slide = MotionSpec::Timed(0.26f, MotionEasing::kSmootherStep);
  tokens.select_sharp = MotionSpec::Timed(0.16f, MotionEasing::kEaseOutCubic);
  tokens.spring_soft = MotionSpec::Spring(14.0f, MotionEasing::kSpringSoft);
  tokens.spring_snappy = MotionSpec::Spring(20.0f, MotionEasing::kSpringSnappy);
  return tokens;
}

MotionTokens MotionTokens::Reduced() {
  MotionTokens tokens{};
  tokens.hover_fast = MotionSpec::Timed(0.05f, MotionEasing::kEaseOutQuad);
  tokens.hover_soft = MotionSpec::Timed(0.06f, MotionEasing::kEaseOutQuad);
  tokens.press_fast = MotionSpec::Timed(0.04f, MotionEasing::kEaseOutQuad);
  tokens.fade_fast = MotionSpec::Timed(0.05f, MotionEasing::kEaseOutQuad);
  tokens.fade_medium = MotionSpec::Timed(0.07f, MotionEasing::kEaseOutQuad);
  tokens.fade_slow = MotionSpec::Timed(0.10f, MotionEasing::kEaseOutQuad);
  tokens.slide_soft = MotionSpec::Timed(0.07f, MotionEasing::kEaseOutQuad);
  tokens.slide_medium = MotionSpec::Timed(0.09f, MotionEasing::kEaseOutQuad);
  tokens.slide_large = MotionSpec::Timed(0.10f, MotionEasing::kEaseOutQuad);
  tokens.panel_enter_fade = MotionSpec::Timed(0.08f, MotionEasing::kEaseOutQuad);
  tokens.panel_enter_offset = MotionSpec::Timed(0.10f, MotionEasing::kEaseOutQuad);
  tokens.popup_open = MotionSpec::Timed(0.07f, MotionEasing::kEaseOutQuad);
  tokens.popup_close = MotionSpec::Timed(0.06f, MotionEasing::kEaseOutQuad);
  tokens.tab_fade = MotionSpec::Timed(0.06f, MotionEasing::kEaseOutQuad);
  tokens.tab_slide = MotionSpec::Timed(0.08f, MotionEasing::kEaseOutQuad);
  tokens.search_fade = MotionSpec::Timed(0.06f, MotionEasing::kEaseOutQuad);
  tokens.search_slide = MotionSpec::Timed(0.08f, MotionEasing::kEaseOutQuad);
  tokens.select_sharp = MotionSpec::Timed(0.06f, MotionEasing::kEaseOutQuad);
  tokens.spring_soft = MotionSpec::Spring(22.0f, MotionEasing::kSpringSoft);
  tokens.spring_snappy = MotionSpec::Spring(28.0f, MotionEasing::kSpringSnappy);
  return tokens;
}

}  // namespace minimize::ui::motion
