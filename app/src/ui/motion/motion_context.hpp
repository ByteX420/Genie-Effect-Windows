#pragma once

#include "ui/motion/motion.hpp"
#include "ui/motion/motion_tokens.hpp"

namespace minimize::ui::motion {

struct MotionContext {
  MotionSystem& system;
  const MotionTokens& tokens;
};

}  // namespace minimize::ui::motion
