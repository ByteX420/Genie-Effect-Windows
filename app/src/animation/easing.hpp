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

  void ClampHandles() {
    x1 = std::clamp(x1, 0.0f, 1.0f);
    x2 = std::clamp(x2, 0.0f, 1.0f);
    y1 = std::clamp(y1, -0.5f, 1.5f);
    y2 = std::clamp(y2, -0.5f, 1.5f);
  }

  // CSS "ease" preset — smooth default for custom mode.
  static CubicBezier Ease() { return CubicBezier{0.25f, 0.1f, 0.25f, 1.0f}; }
  static CubicBezier EaseInOut() { return CubicBezier{0.42f, 0.0f, 0.58f, 1.0f}; }
};

inline EasingCurve EasingCurveFromName(std::string_view name) {
  if (name == "Ease In") return EasingCurve::kEaseIn;
  if (name == "Ease Out") return EasingCurve::kEaseOut;
  if (name == "Ease In Out") return EasingCurve::kEaseInOut;
  if (name == "Cubic") return EasingCurve::kCubic;
  if (name == "Back") return EasingCurve::kBack;
  if (name == "Elastic") return EasingCurve::kElastic;
  if (name == "Custom") return EasingCurve::kCustom;
  return EasingCurve::kLinear;
}

namespace detail {

// Cubic Bernstein for one axis with P0=0, P3=1.
inline float BezierComponent(float t, float p1, float p2) {
  const float u = 1.0f - t;
  return 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t;
}

inline float BezierComponentDerivative(float t, float p1, float p2) {
  const float u = 1.0f - t;
  // d/dt [3(1-t)^2 t p1 + 3(1-t) t^2 p2 + t^3]
  return 3.0f * u * u * p1 + 6.0f * u * t * (p2 - p1) + 3.0f * t * t * (1.0f - p2);
}

// Invert Bx(t) = x with Newton + bisection fallback (stable CSS cubic-bezier solve).
inline float SolveBezierT(float x, float x1, float x2) {
  x = std::clamp(x, 0.0f, 1.0f);
  if (x <= 0.0f) return 0.0f;
  if (x >= 1.0f) return 1.0f;

  float t = x;
  for (int i = 0; i < 8; ++i) {
    const float x_est = BezierComponent(t, x1, x2) - x;
    if (std::abs(x_est) < 1e-6f) return std::clamp(t, 0.0f, 1.0f);
    const float d = BezierComponentDerivative(t, x1, x2);
    if (std::abs(d) < 1e-6f) break;
    t = std::clamp(t - x_est / d, 0.0f, 1.0f);
  }

  float lo = 0.0f;
  float hi = 1.0f;
  t = x;
  for (int i = 0; i < 12; ++i) {
    const float x_est = BezierComponent(t, x1, x2);
    if (std::abs(x_est - x) < 1e-6f) break;
    if (x_est < x)
      lo = t;
    else
      hi = t;
    t = 0.5f * (lo + hi);
  }
  return std::clamp(t, 0.0f, 1.0f);
}

}  // namespace detail

// Sample the cubic-bezier Y for a given unit progress X (CSS cubic-bezier semantics).
inline float EvaluateCubicBezier(const CubicBezier& bezier, float progress) {
  const float t = detail::SolveBezierT(progress, bezier.x1, bezier.x2);
  return detail::BezierComponent(t, bezier.y1, bezier.y2);
}

// Point on the parametric curve for drawing (u in [0,1] is bezier parameter, not progress).
inline void CubicBezierPoint(const CubicBezier& bezier, float u, float* out_x, float* out_y) {
  u = std::clamp(u, 0.0f, 1.0f);
  if (out_x) *out_x = detail::BezierComponent(u, bezier.x1, bezier.x2);
  if (out_y) *out_y = detail::BezierComponent(u, bezier.y1, bezier.y2);
}

inline float ApplyEasing(EasingCurve curve, float value,
                         const CubicBezier& custom = CubicBezier::EaseInOut()) {
  constexpr float kPi = 3.14159265358979323846f;
  const float t = std::clamp(value, 0.0f, 1.0f);
  float eased = t;
  switch (curve) {
    case EasingCurve::kLinear:
      eased = t;
      break;
    case EasingCurve::kEaseIn:
      eased = t * t;
      break;
    case EasingCurve::kEaseOut:
      eased = 1.0f - (1.0f - t) * (1.0f - t);
      break;
    case EasingCurve::kEaseInOut:
      eased = t * t * (3.0f - 2.0f * t);
      break;
    case EasingCurve::kCubic:
      eased = t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
      break;
    case EasingCurve::kBack: {
      constexpr float kOvershoot = 1.35f;
      const float shifted = t - 1.0f;
      eased = 1.0f + shifted * shifted * ((kOvershoot + 1.0f) * shifted + kOvershoot);
      break;
    }
    case EasingCurve::kElastic:
      if (t != 0.0f && t != 1.0f) {
        eased =
            std::pow(2.0f, -9.0f * t) * std::sin((t * 9.0f - 0.75f) * (2.0f * kPi / 3.0f)) + 1.0f;
      }
      break;
    case EasingCurve::kCustom:
      eased = EvaluateCubicBezier(custom, t);
      break;
  }
  return std::clamp(eased, 0.0f, 1.0f);
}

}  // namespace genie::animation
