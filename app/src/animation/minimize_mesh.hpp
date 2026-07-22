#pragma once

#include <cstdint>

#include "animation/geometry.hpp"

namespace minimize::animation {

enum class MinimizeEdge {
  kTop,
  kBottom,
  kLeft,
  kRight,
};

enum class MinimizeDirection {
  kMinimize,
  kMaximize,
};

enum class AnimationStyle {
  kClassic,
  kCurvy,
  kSquash,
};

struct GridVertex {
  float u = 0.0f;
  float v = 0.0f;
};

struct GenieConstants {
  RectF source;
  RectF target;
  float progress = 0.0f;
  float strength = 1.0f;
  std::uint32_t edge = 0;
  std::uint32_t style = 0;
};

static_assert(sizeof(GenieConstants) == 48);

}  // namespace minimize::animation
