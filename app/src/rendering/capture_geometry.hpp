#pragma once

#include <Windows.h>

namespace minimize::rendering::capture_geometry {

int Width(const RECT& rect);
int Height(const RECT& rect);
RECT ClampToOutput(const RECT& rect, const RECT& output_rect);

}  // namespace minimize::rendering::capture_geometry
