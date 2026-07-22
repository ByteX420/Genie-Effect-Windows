#include "pch.hpp"

#include "rendering/window_capture_mask.hpp"

#include <algorithm>
#include <cmath>
#include <dwmapi.h>

namespace minimize::rendering::window_capture_mask {

int CornerRadius(HWND window) {
  if (window == nullptr || !IsWindow(window) || IsZoomed(window)) {
    return 0;
  }

  DWORD corner_preference = 0;
  constexpr auto kWindowCornerPreference = static_cast<DWMWINDOWATTRIBUTE>(33);
  if (FAILED(DwmGetWindowAttribute(window, kWindowCornerPreference, &corner_preference,
                                   sizeof(corner_preference)))) {
    return 0;
  }

  if (corner_preference == 1) {
    return 0;
  }
  const int base_radius = corner_preference == 3 ? 8 : 12;
  const UINT dpi = std::max(GetDpiForWindow(window), 96U);
  return MulDiv(base_radius, dpi, 96);
}

void Apply(std::vector<std::uint8_t>* pixels, int width, int height, int radius,
           const RECT& window_rect, const RECT& extended_bounds) {
  if (pixels == nullptr || width <= 0 || height <= 0) {
    return;
  }

  for (size_t i = 3; i < pixels->size(); i += 4) {
    (*pixels)[i] = 0xff;
  }

  int visible_left = std::max(0, static_cast<int>(extended_bounds.left - window_rect.left));
  int visible_top = std::max(0, static_cast<int>(extended_bounds.top - window_rect.top));
  int visible_right = std::min(width, static_cast<int>(extended_bounds.right - window_rect.left));
  int visible_bottom = std::min(height, static_cast<int>(extended_bounds.bottom - window_rect.top));
  if (visible_right <= visible_left || visible_bottom <= visible_top) {
    visible_left = 0;
    visible_top = 0;
    visible_right = width;
    visible_bottom = height;
  }

  const auto clear_pixel = [pixels, width](int x, int y) {
    const size_t index =
        (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4 + 3;
    (*pixels)[index] = 0;
  };
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (x < visible_left || x >= visible_right || y < visible_top || y >= visible_bottom) {
        clear_pixel(x, y);
      }
    }
  }

  if (radius <= 0) {
    return;
  }
  radius =
      std::min({radius, (visible_right - visible_left) / 2, (visible_bottom - visible_top) / 2});

  for (int y = 0; y < radius; ++y) {
    for (int x = 0; x < radius; ++x) {
      const float center = static_cast<float>(radius) - 0.5f;
      const float dx = static_cast<float>(x) - center;
      const float dy = static_cast<float>(y) - center;
      const float distance = std::sqrt(dx * dx + dy * dy);
      float alpha_factor = 1.0f;
      if (distance > static_cast<float>(radius) + 0.5f) {
        alpha_factor = 0.0f;
      } else if (distance > static_cast<float>(radius) - 0.5f) {
        alpha_factor = static_cast<float>(radius) + 0.5f - distance;
      }
      if (alpha_factor >= 1.0f) {
        continue;
      }

      const auto apply_alpha = [pixels, width, alpha_factor](int px, int py) {
        const size_t index =
            (static_cast<size_t>(py) * static_cast<size_t>(width) + static_cast<size_t>(px)) * 4 +
            3;
        (*pixels)[index] =
            static_cast<std::uint8_t>(static_cast<float>((*pixels)[index]) * alpha_factor);
      };
      apply_alpha(visible_left + x, visible_top + y);
      apply_alpha(visible_right - 1 - x, visible_top + y);
      apply_alpha(visible_left + x, visible_bottom - 1 - y);
      apply_alpha(visible_right - 1 - x, visible_bottom - 1 - y);
    }
  }
}

}  // namespace minimize::rendering::window_capture_mask
