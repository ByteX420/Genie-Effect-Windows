#pragma once

#include "menu/motion/motion.hpp"

namespace ui::motion {
  struct MotionTokens
  {
    MotionSpec hoverFast;
    MotionSpec hoverSoft;
    MotionSpec pressFast;
    MotionSpec fadeFast;
    MotionSpec fadeMedium;
    MotionSpec fadeSlow;
    MotionSpec slideSoft;
    MotionSpec slideMedium;
    MotionSpec slideLarge;
    MotionSpec panelEnterFade;
    MotionSpec panelEnterOffset;
    MotionSpec popupOpen;
    MotionSpec popupClose;
    MotionSpec tabFade;
    MotionSpec tabSlide;
    MotionSpec searchFade;
    MotionSpec searchSlide;
    MotionSpec selectSharp;
    MotionSpec springSoft;
    MotionSpec springSnappy;

    static MotionTokens Default() {
      // Keep interactive hovers short + cubic (predictable). Springs only for
      // toggles/knobs. Avoid long SmootherStep on hover — it feels laggy/stuttery.
      MotionTokens tokens{};
      tokens.hoverFast        = MotionSpec::Timed( 0.14f, MotionEasing::EaseOutCubic );
      tokens.hoverSoft        = MotionSpec::Timed( 0.18f, MotionEasing::EaseOutCubic );
      tokens.pressFast        = MotionSpec::Timed( 0.09f, MotionEasing::EaseOutCubic );
      tokens.fadeFast         = MotionSpec::Timed( 0.18f, MotionEasing::EaseOutCubic );
      tokens.fadeMedium       = MotionSpec::Timed( 0.28f, MotionEasing::EaseOutCubic );
      tokens.fadeSlow         = MotionSpec::Timed( 0.40f, MotionEasing::SmootherStep );
      tokens.slideSoft        = MotionSpec::Timed( 0.30f, MotionEasing::SmootherStep );
      tokens.slideMedium      = MotionSpec::Timed( 0.36f, MotionEasing::SmootherStep );
      tokens.slideLarge       = MotionSpec::Timed( 0.48f, MotionEasing::SmootherStep );
      tokens.panelEnterFade   = MotionSpec::Timed( 0.34f, MotionEasing::EaseOutCubic );
      tokens.panelEnterOffset = MotionSpec::Timed( 0.40f, MotionEasing::SmootherStep );
      // Combo / popup expand: S-curve open (not linear-feeling cubic), snappier close.
      tokens.popupOpen        = MotionSpec::Timed( 0.30f, MotionEasing::SmootherStep );
      tokens.popupClose       = MotionSpec::Timed( 0.18f, MotionEasing::EaseInQuad );
      tokens.tabFade          = MotionSpec::Timed( 0.22f, MotionEasing::EaseOutCubic );
      tokens.tabSlide         = MotionSpec::Timed( 0.32f, MotionEasing::SmootherStep );
      tokens.searchFade       = MotionSpec::Timed( 0.18f, MotionEasing::EaseOutCubic );
      tokens.searchSlide      = MotionSpec::Timed( 0.26f, MotionEasing::SmootherStep );
      tokens.selectSharp      = MotionSpec::Timed( 0.16f, MotionEasing::EaseOutCubic );
      tokens.springSoft       = MotionSpec::Spring( 14.0f, MotionEasing::SpringSoft );
      tokens.springSnappy     = MotionSpec::Spring( 20.0f, MotionEasing::SpringSnappy );
      return tokens;
    }

    static MotionTokens Reduced() {
      MotionTokens tokens{};
      tokens.hoverFast        = MotionSpec::Timed( 0.05f, MotionEasing::EaseOutQuad );
      tokens.hoverSoft        = MotionSpec::Timed( 0.06f, MotionEasing::EaseOutQuad );
      tokens.pressFast        = MotionSpec::Timed( 0.04f, MotionEasing::EaseOutQuad );
      tokens.fadeFast         = MotionSpec::Timed( 0.05f, MotionEasing::EaseOutQuad );
      tokens.fadeMedium       = MotionSpec::Timed( 0.07f, MotionEasing::EaseOutQuad );
      tokens.fadeSlow         = MotionSpec::Timed( 0.10f, MotionEasing::EaseOutQuad );
      tokens.slideSoft        = MotionSpec::Timed( 0.07f, MotionEasing::EaseOutQuad );
      tokens.slideMedium      = MotionSpec::Timed( 0.09f, MotionEasing::EaseOutQuad );
      tokens.slideLarge       = MotionSpec::Timed( 0.10f, MotionEasing::EaseOutQuad );
      tokens.panelEnterFade   = MotionSpec::Timed( 0.08f, MotionEasing::EaseOutQuad );
      tokens.panelEnterOffset = MotionSpec::Timed( 0.10f, MotionEasing::EaseOutQuad );
      tokens.popupOpen        = MotionSpec::Timed( 0.07f, MotionEasing::EaseOutQuad );
      tokens.popupClose       = MotionSpec::Timed( 0.06f, MotionEasing::EaseOutQuad );
      tokens.tabFade          = MotionSpec::Timed( 0.06f, MotionEasing::EaseOutQuad );
      tokens.tabSlide         = MotionSpec::Timed( 0.08f, MotionEasing::EaseOutQuad );
      tokens.searchFade       = MotionSpec::Timed( 0.06f, MotionEasing::EaseOutQuad );
      tokens.searchSlide      = MotionSpec::Timed( 0.08f, MotionEasing::EaseOutQuad );
      tokens.selectSharp      = MotionSpec::Timed( 0.06f, MotionEasing::EaseOutQuad );
      tokens.springSoft       = MotionSpec::Spring( 22.0f, MotionEasing::SpringSoft );
      tokens.springSnappy     = MotionSpec::Spring( 28.0f, MotionEasing::SpringSnappy );
      return tokens;
    }
  };
}  // namespace ui::motion
