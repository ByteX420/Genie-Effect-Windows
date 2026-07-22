#include "pch.hpp"

#include "rendering/capture_geometry.hpp"

#include <algorithm>

namespace minimize::rendering::capture_geometry {

int Width(const RECT& rect) { return static_cast<int>(rect.right - rect.left); }

int Height(const RECT& rect) { return static_cast<int>(rect.bottom - rect.top); }

RECT ClampToOutput(const RECT& rect, const RECT& output_rect) {
  return RECT{
      .left = std::max(rect.left, output_rect.left),
      .top = std::max(rect.top, output_rect.top),
      .right = std::min(rect.right, output_rect.right),
      .bottom = std::min(rect.bottom, output_rect.bottom),
  };
}

}  // namespace minimize::rendering::capture_geometry
