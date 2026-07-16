#pragma once

#include <array>
#include <span>

#include "app/settings_ui_theme.hpp"

namespace genie::app::settings_ui {

void DelayedTooltip(const char* text, float scale);
bool Toggle(const MotionContext& motion, const char* id, bool* value, float scale, float alpha);
bool Slider(const MotionContext& motion, const char* id, const char* label, float* value,
            float minimum, float maximum, float width, float scale, float alpha, ImFont* label_font,
            float step, float display_multiplier = 1.0f, int display_precision = 2,
            const char* display_suffix = "s");
bool Combo(const MotionContext& motion, const char* id, const char* label, int* current,
           std::span<const char* const> items, const ImVec2& size, ImFont* label_font,
           ImFont* item_font, float scale, float alpha);
bool CompactButton(const MotionContext& motion, const char* id, const char* label,
                   const ImVec2& size, ImFont* font, float scale, float alpha, bool active = false);
bool SegmentSelector(const MotionContext& motion, const char* id,
                     const std::array<const char*, 2>& labels, int* selected, float width,
                     ImFont* font, float scale, float alpha);

}  // namespace genie::app::settings_ui
