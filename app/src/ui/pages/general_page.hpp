#pragma once

#include "ui/components/page_layout.hpp"

namespace minimize::ui {
class SettingsWindow;
}

namespace minimize::ui::pages {

class GeneralPage final {
public:
  static void Render(::minimize::ui::SettingsWindow& window, components::PageLayout& layout,
                     const ::minimize::ui::motion::MotionContext& motion, float scale, float alpha);
};

}  // namespace minimize::ui::pages
