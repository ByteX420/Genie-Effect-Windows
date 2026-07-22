#pragma once

#include "ui/components/page_layout.hpp"
#include "ui/motion/motion_context.hpp"

namespace minimize::ui {
class SettingsWindow;
}

namespace minimize::ui::pages {

class DisplaysPage final {
public:
  static void Render(::minimize::ui::SettingsWindow& window, components::PageLayout& layout,
                     const ::minimize::ui::motion::MotionContext& motion, float scale, float alpha);
};

}  // namespace minimize::ui::pages
