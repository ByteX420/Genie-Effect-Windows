#pragma once

#include "ui/pages/general_page.hpp"

namespace minimize::ui::pages {

class AnimationPage final {
public:
  static void Render(::minimize::ui::SettingsWindow& window, components::PageLayout& layout,
                     const ::minimize::ui::motion::MotionContext& motion, float scale, float alpha,
                     float vertical_offset);
};

}  // namespace minimize::ui::pages
