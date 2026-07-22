#pragma once

#include <span>

#include "ui/theme/theme.hpp"

namespace minimize::ui::components {

bool Combo(const ::minimize::ui::motion::MotionContext& motion, const char* id, const char* label,
           int* current, std::span<const char* const> items, const ImVec2& size, ImFont* label_font,
           ImFont* item_font, float scale, float alpha);

}  // namespace minimize::ui::components
