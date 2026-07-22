#pragma once

namespace minimize::animation {

struct RectF {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  [[nodiscard]] float Width() const { return right - left; }
  [[nodiscard]] float Height() const { return bottom - top; }
};

struct PointF {
  float x = 0.0f;
  float y = 0.0f;
};

struct SizeF {
  float width = 0.0f;
  float height = 0.0f;
};

}  // namespace minimize::animation
