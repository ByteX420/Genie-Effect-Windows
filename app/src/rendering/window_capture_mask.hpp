#pragma once

#include <Windows.h>
#include <cstdint>
#include <vector>

namespace minimize::rendering::window_capture_mask {

int CornerRadius(HWND window);
void Apply(std::vector<std::uint8_t>* pixels, int width, int height, int radius,
           const RECT& window_rect, const RECT& extended_bounds);

}  // namespace minimize::rendering::window_capture_mask
