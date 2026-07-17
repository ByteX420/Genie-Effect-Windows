#pragma once

#include <span>

#include "ui/theme/theme.hpp"

namespace genie::ui::components {

bool Combo(const ::genie::ui::motion::MotionContext& motion, const char* id, const char* label,
           int* current, std::span<const char* const> items, const ImVec2& size, ImFont* label_font,
           ImFont* item_font, float scale, float alpha);

}  // namespace genie::ui::components
