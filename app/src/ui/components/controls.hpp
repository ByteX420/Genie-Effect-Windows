#pragma once

#include <span>

#include "ui/theme/theme.hpp"

namespace genie::ui::components {

void DelayedTooltip(const char* text, float scale);
bool Toggle(const ::genie::ui::motion::MotionContext& motion, const char* id, bool* value,
            float scale, float alpha);
bool Slider(const ::genie::ui::motion::MotionContext& motion, const char* id, const char* label,
            float* value, float minimum, float maximum, float width, float scale, float alpha,
            ImFont* label_font, float step, float display_multiplier = 1.0f,
            int display_precision = 2, const char* display_suffix = "s");
bool CompactButton(const ::genie::ui::motion::MotionContext& motion, const char* id,
                   const char* label, const ImVec2& size, ImFont* font, float scale, float alpha,
                   bool active = false);
bool SegmentSelector(const ::genie::ui::motion::MotionContext& motion, const char* id,
                     std::span<const char* const> labels, int* selected, float width, ImFont* font,
                     float scale, float alpha);

}  // namespace genie::ui::components
