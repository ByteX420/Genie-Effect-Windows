#pragma once

#include "ui/pages/general_page.hpp"

namespace genie::ui::pages {

class AnimationPage final {
public:
  static void Render(::genie::ui::SettingsWindow& window, components::PageLayout& layout,
                     const ::genie::ui::motion::MotionContext& motion, float scale, float alpha,
                     float vertical_offset);
};

}  // namespace genie::ui::pages
