#pragma once

#include <Windows.h>
#include <cstdint>
#include <vector>

#include "rendering/window_visual_metadata.hpp"

namespace minimize::rendering::window_capture_mask {

[[nodiscard]] std::vector<std::uint8_t> Build(const WindowVisualMetadata& metadata, int width,
                                              int height, const RECT& window_rect,
                                              const RECT& capture_rect,
                                              const RECT& extended_bounds);

}  // namespace minimize::rendering::window_capture_mask
