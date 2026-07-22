#include "pch.hpp"

#include "animation/easing.hpp"

#include <algorithm>
#include <cmath>

namespace minimize::animation {
namespace {

float BezierComponent(float parameter, float first_handle, float second_handle) {
  const float inverse = 1.0f - parameter;
  return 3.0f * inverse * inverse * parameter * first_handle +
         3.0f * inverse * parameter * parameter * second_handle + parameter * parameter * parameter;
}

float BezierComponentDerivative(float parameter, float first_handle, float second_handle) {
  const float inverse = 1.0f - parameter;
  return 3.0f * inverse * inverse * first_handle +
         6.0f * inverse * parameter * (second_handle - first_handle) +
         3.0f * parameter * parameter * (1.0f - second_handle);
}

float SolveBezierParameter(float progress, float first_x, float second_x) {
  progress = std::clamp(progress, 0.0f, 1.0f);
  if (progress <= 0.0f) {
    return 0.0f;
  }
  if (progress >= 1.0f) {
    return 1.0f;
  }

  float parameter = progress;
  for (int iteration = 0; iteration < 8; ++iteration) {
    const float estimate = BezierComponent(parameter, first_x, second_x) - progress;
    if (std::abs(estimate) < 1e-6f) {
      return std::clamp(parameter, 0.0f, 1.0f);
    }
    const float derivative = BezierComponentDerivative(parameter, first_x, second_x);
    if (std::abs(derivative) < 1e-6f) {
      break;
    }
    parameter = std::clamp(parameter - estimate / derivative, 0.0f, 1.0f);
  }

  float lower = 0.0f;
  float upper = 1.0f;
  parameter = progress;
  for (int iteration = 0; iteration < 12; ++iteration) {
    const float estimate = BezierComponent(parameter, first_x, second_x);
    if (std::abs(estimate - progress) < 1e-6f) {
      break;
    }
    if (estimate < progress) {
      lower = parameter;
    } else {
      upper = parameter;
    }
    parameter = 0.5f * (lower + upper);
  }
  return std::clamp(parameter, 0.0f, 1.0f);
}

}  // namespace

void CubicBezier::ClampHandles() {
  x1 = std::clamp(x1, 0.0f, 1.0f);
  x2 = std::clamp(x2, 0.0f, 1.0f);
  y1 = std::clamp(y1, -0.5f, 1.5f);
  y2 = std::clamp(y2, -0.5f, 1.5f);
}

CubicBezier CubicBezier::EaseInOut() { return CubicBezier{0.42f, 0.0f, 0.58f, 1.0f}; }

EasingCurve EasingCurveFromName(std::string_view name) {
  if (name == "Ease In") {
    return EasingCurve::kEaseIn;
  }
  if (name == "Ease Out") {
    return EasingCurve::kEaseOut;
  }
  if (name == "Ease In Out") {
    return EasingCurve::kEaseInOut;
  }
  if (name == "Cubic") {
    return EasingCurve::kCubic;
  }
  if (name == "Back") {
    return EasingCurve::kBack;
  }
  if (name == "Elastic") {
    return EasingCurve::kElastic;
  }
  if (name == "Custom") {
    return EasingCurve::kCustom;
  }
  return EasingCurve::kLinear;
}

float EvaluateCubicBezier(const CubicBezier& bezier, float progress) {
  const float parameter = SolveBezierParameter(progress, bezier.x1, bezier.x2);
  return BezierComponent(parameter, bezier.y1, bezier.y2);
}

void CubicBezierPoint(const CubicBezier& bezier, float parameter, float* out_x, float* out_y) {
  parameter = std::clamp(parameter, 0.0f, 1.0f);
  if (out_x != nullptr) {
    *out_x = BezierComponent(parameter, bezier.x1, bezier.x2);
  }
  if (out_y != nullptr) {
    *out_y = BezierComponent(parameter, bezier.y1, bezier.y2);
  }
}

float ApplyEasing(EasingCurve curve, float value, const CubicBezier& custom) {
  constexpr float kPi = 3.14159265358979323846f;
  const float progress = std::clamp(value, 0.0f, 1.0f);
  float eased = progress;
  switch (curve) {
    case EasingCurve::kLinear:
      break;
    case EasingCurve::kEaseIn:
      eased = progress * progress;
      break;
    case EasingCurve::kEaseOut:
      eased = 1.0f - (1.0f - progress) * (1.0f - progress);
      break;
    case EasingCurve::kEaseInOut:
      eased = progress * progress * (3.0f - 2.0f * progress);
      break;
    case EasingCurve::kCubic:
      eased = progress < 0.5f ? 4.0f * progress * progress * progress
                              : 1.0f - std::pow(-2.0f * progress + 2.0f, 3.0f) * 0.5f;
      break;
    case EasingCurve::kBack: {
      // Exact endpoints avoid residual float noise after the overshoot polynomial.
      if (progress == 0.0f || progress == 1.0f) {
        eased = progress;
        break;
      }
      constexpr float kOvershoot = 1.35f;
      const float shifted = progress - 1.0f;
      eased = 1.0f + shifted * shifted * ((kOvershoot + 1.0f) * shifted + kOvershoot);
      break;
    }
    case EasingCurve::kElastic:
      if (progress != 0.0f && progress != 1.0f) {
        eased = std::pow(2.0f, -9.0f * progress) *
                    std::sin((progress * 9.0f - 0.75f) * (2.0f * kPi / 3.0f)) +
                1.0f;
      }
      break;
    case EasingCurve::kCustom:
      eased = EvaluateCubicBezier(custom, progress);
      break;
  }
  return std::clamp(eased, 0.0f, 1.0f);
}

}  // namespace minimize::animation
