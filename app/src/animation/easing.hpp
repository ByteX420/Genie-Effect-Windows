#pragma once

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
  kCustom,
};

// CSS-style cubic-bezier control points for Custom easing.
// Curve runs from (0,0) → (x1,y1) → (x2,y2) → (1,1). X is clamped to [0,1];
// Y may leave [0,1] for mild overshoot (clamped for storage/UI).
struct CubicBezier {
  float x1 = 0.42f;
  float y1 = 0.0f;
  float x2 = 0.58f;
  float y2 = 1.0f;

  bool operator==(const CubicBezier&) const = default;

  void ClampHandles();
  [[nodiscard]] static CubicBezier EaseInOut();
};

[[nodiscard]] EasingCurve EasingCurveFromName(std::string_view name);
[[nodiscard]] float EvaluateCubicBezier(const CubicBezier& bezier, float progress);
void CubicBezierPoint(const CubicBezier& bezier, float parameter, float* out_x, float* out_y);
[[nodiscard]] float ApplyEasing(EasingCurve curve, float value,
                                const CubicBezier& custom = CubicBezier::EaseInOut());

}  // namespace genie::animation
