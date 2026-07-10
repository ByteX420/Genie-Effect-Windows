#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

namespace genie::animation {

enum class EasingCurve {
  kLinear,
  kEaseIn,
  kEaseOut,
  kEaseInOut,
  kCubic,
  kBack,
  kElastic,
};

inline EasingCurve EasingCurveFromName(std::string_view name) {
  if (name == "Ease In") return EasingCurve::kEaseIn;
  if (name == "Ease Out") return EasingCurve::kEaseOut;
  if (name == "Ease In Out") return EasingCurve::kEaseInOut;
  if (name == "Cubic") return EasingCurve::kCubic;
  if (name == "Back") return EasingCurve::kBack;
  if (name == "Elastic") return EasingCurve::kElastic;
  return EasingCurve::kLinear;
}

inline float ApplyEasing(EasingCurve curve, float value) {
  constexpr float kPi = 3.14159265358979323846f;
  const float t = std::clamp(value, 0.0f, 1.0f);
  float eased = t;
  switch (curve) {
    case EasingCurve::kLinear: eased = t; break;
    case EasingCurve::kEaseIn: eased = t * t; break;
    case EasingCurve::kEaseOut: eased = 1.0f - (1.0f - t) * (1.0f - t); break;
    case EasingCurve::kEaseInOut: eased = t * t * (3.0f - 2.0f * t); break;
    case EasingCurve::kCubic:
      eased = t < 0.5f ? 4.0f * t * t * t
                       : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
      break;
    case EasingCurve::kBack: {
      constexpr float kOvershoot = 1.35f;
      const float shifted = t - 1.0f;
      eased = 1.0f + shifted * shifted * ((kOvershoot + 1.0f) * shifted + kOvershoot);
      break;
    }
    case EasingCurve::kElastic:
      if (t != 0.0f && t != 1.0f) {
        eased = std::pow(2.0f, -9.0f * t) *
                    std::sin((t * 9.0f - 0.75f) * (2.0f * kPi / 3.0f)) +
                1.0f;
      }
      break;
  }
  return std::clamp(eased, 0.0f, 1.0f);
}

}  // namespace genie::animation
