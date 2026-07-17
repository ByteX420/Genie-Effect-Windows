#pragma once

#include "animation/easing.hpp"
#include "ui/theme/theme.hpp"

namespace genie::ui::components {

bool EasingGraphEditor(const ::genie::ui::motion::MotionContext& motion, const char* id,
                       animation::CubicBezier* bezier, const ImVec2& size, float scale, float alpha,
                       bool* changed, ImFont* caption_font = nullptr);

}  // namespace genie::ui::components
