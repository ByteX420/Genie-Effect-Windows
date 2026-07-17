#pragma once

#include "ui/motion/motion.hpp"

namespace genie::ui::motion {

struct MotionTokens {
  MotionSpec hover_fast;
  MotionSpec hover_soft;
  MotionSpec press_fast;
  MotionSpec fade_fast;
  MotionSpec fade_medium;
  MotionSpec fade_slow;
  MotionSpec slide_soft;
  MotionSpec slide_medium;
  MotionSpec slide_large;
  MotionSpec panel_enter_fade;
  MotionSpec panel_enter_offset;
  MotionSpec popup_open;
  MotionSpec popup_close;
  MotionSpec tab_fade;
  MotionSpec tab_slide;
  MotionSpec search_fade;
  MotionSpec search_slide;
  MotionSpec select_sharp;
  MotionSpec spring_soft;
  MotionSpec spring_snappy;

  [[nodiscard]] static MotionTokens Default();
  [[nodiscard]] static MotionTokens Reduced();
};

}  // namespace genie::ui::motion
