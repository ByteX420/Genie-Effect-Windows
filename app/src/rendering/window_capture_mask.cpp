#include "pch.hpp"

#include "rendering/window_capture_mask.hpp"

#include <algorithm>
#include <cmath>

namespace minimize::rendering::window_capture_mask {
namespace {

void FillRegionRect(std::vector<std::uint8_t>* mask, int width, int height, const RECT& rect) {
  const int left = std::clamp(static_cast<int>(rect.left), 0, width);
  const int top = std::clamp(static_cast<int>(rect.top), 0, height);
  const int right = std::clamp(static_cast<int>(rect.right), 0, width);
  const int bottom = std::clamp(static_cast<int>(rect.bottom), 0, height);
  for (int y = top; y < bottom; ++y) {
    auto begin = mask->begin() + static_cast<std::size_t>(y) * width + left;
    std::fill(begin, begin + (right - left), std::uint8_t{0xff});
  }
}

}  // namespace

std::vector<std::uint8_t> Build(const WindowVisualMetadata& metadata, int width, int height,
                                const RECT& window_rect, const RECT& capture_rect,
                                const RECT& extended_bounds) {
  if (width <= 0 || height <= 0) return {};

  std::vector<std::uint8_t> mask(
      static_cast<std::size_t>(width) * height,
      metadata.window_region.is_set ? std::uint8_t{0} : std::uint8_t{0xff});
  for (RECT region_rect : metadata.window_region.rectangles) {
    OffsetRect(&region_rect, window_rect.left - capture_rect.left,
               window_rect.top - capture_rect.top);
    FillRegionRect(&mask, width, height, region_rect);
  }

  const int visible_left =
      std::clamp(static_cast<int>(extended_bounds.left - capture_rect.left), 0, width);
  const int visible_top =
      std::clamp(static_cast<int>(extended_bounds.top - capture_rect.top), 0, height);
  const int visible_right =
      std::clamp(static_cast<int>(extended_bounds.right - capture_rect.left), 0, width);
  const int visible_bottom =
      std::clamp(static_cast<int>(extended_bounds.bottom - capture_rect.top), 0, height);
  if (visible_right > visible_left && visible_bottom > visible_top) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        if (x < visible_left || x >= visible_right || y < visible_top || y >= visible_bottom) {
          mask[static_cast<std::size_t>(y) * width + x] = 0;
        }
      }
    }
  }

  int radius = static_cast<int>(std::round(metadata.corner_radius));
  if (radius <= 0 || visible_right <= visible_left || visible_bottom <= visible_top) return mask;
  radius =
      std::min({radius, (visible_right - visible_left) / 2, (visible_bottom - visible_top) / 2});

  for (int y = 0; y < radius; ++y) {
    for (int x = 0; x < radius; ++x) {
      const float center = static_cast<float>(radius) - 0.5f;
      const float dx = static_cast<float>(x) - center;
      const float dy = static_cast<float>(y) - center;
      const float distance = std::sqrt(dx * dx + dy * dy);
      const float alpha_factor =
          std::clamp(static_cast<float>(radius) + 0.5f - distance, 0.0f, 1.0f);
      if (alpha_factor >= 1.0f) continue;

      const auto apply_alpha = [&](int px, int py) {
        const std::size_t index = static_cast<std::size_t>(py) * width + px;
        mask[index] = static_cast<std::uint8_t>(static_cast<float>(mask[index]) * alpha_factor);
      };
      apply_alpha(visible_left + x, visible_top + y);
      apply_alpha(visible_right - 1 - x, visible_top + y);
      apply_alpha(visible_left + x, visible_bottom - 1 - y);
      apply_alpha(visible_right - 1 - x, visible_bottom - 1 - y);
    }
  }
  return mask;
}

}  // namespace minimize::rendering::window_capture_mask
