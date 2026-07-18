#pragma once

#include "ui/components/page_layout.hpp"
#include "ui/motion/motion_context.hpp"

namespace genie::ui {
class SettingsWindow;
}

namespace genie::ui::pages {

class DisplaysPage final {
public:
  static void Render(::genie::ui::SettingsWindow& window, components::PageLayout& layout,
                     const ::genie::ui::motion::MotionContext& motion, float scale, float alpha);
};

}  // namespace genie::ui::pages
