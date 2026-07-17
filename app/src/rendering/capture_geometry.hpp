#pragma once

#include <Windows.h>

namespace genie::rendering::capture_geometry {

int Width(const RECT& rect);
int Height(const RECT& rect);
RECT ClampToOutput(const RECT& rect, const RECT& output_rect);

}  // namespace genie::rendering::capture_geometry
