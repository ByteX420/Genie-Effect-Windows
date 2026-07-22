#pragma once

#include "ui/theme/theme.hpp"

namespace minimize::ui::components::detail {

inline motion::MotionKey MotionKey(const char* scope, const char* id, const char* channel) {
  return motion::MotionKey(scope, id ? id : "", channel);
}

inline ImVec4 MixColor(const ImVec4& from, const ImVec4& to, float amount) {
  return ImVec4(from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount,
                from.z + (to.z - from.z) * amount, from.w + (to.w - from.w) * amount);
}

inline float ControlRounding(float scale) {
  return ::minimize::ui::theme::Metrics::kControlRounding * scale;
}

}  // namespace minimize::ui::components::detail
