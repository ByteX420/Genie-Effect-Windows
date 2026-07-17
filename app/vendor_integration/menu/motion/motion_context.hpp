#pragma once

#include "ui/motion/motion.hpp"
#include "ui/motion/motion_tokens.hpp"

namespace ui {
namespace motion = ::genie::ui::motion;
}

namespace WindowMotion {

class MotionSystemAdapter final {
public:
  float value(const genie::ui::motion::MotionKey& key, float target,
              const genie::ui::motion::MotionSpec& spec, float initial) {
    return system_.AnimateValue(key, target, spec, initial);
  }
  float value(const genie::ui::motion::MotionKey& key, float target,
              const genie::ui::motion::MotionSpec& spec) {
    return system_.AnimateValue(key, target, spec);
  }

  ImVec4 color(const genie::ui::motion::MotionKey& key, const ImVec4& target,
               const genie::ui::motion::MotionSpec& spec, const ImVec4& initial) {
    return system_.AnimateColor(key, target, spec, initial);
  }

  void set(const genie::ui::motion::MotionKey& key, float value) { system_.Set(key, value); }

private:
  genie::ui::motion::MotionSystem system_;
};

struct MotionTokenAdapter final {
  genie::ui::motion::MotionSpec hoverFast;
  genie::ui::motion::MotionSpec pressFast;
  genie::ui::motion::MotionSpec fadeSlow;
  genie::ui::motion::MotionSpec slideSoft;
  genie::ui::motion::MotionSpec popupOpen;
  genie::ui::motion::MotionSpec springSnappy;
};

inline MotionSystemAdapter& System() {
  static MotionSystemAdapter system;
  return system;
}

inline const MotionTokenAdapter& Tokens() {
  static const MotionTokenAdapter tokens = [] {
    const auto source = genie::ui::motion::MotionTokens::Default();
    return MotionTokenAdapter{
        .hoverFast = source.hover_fast,
        .pressFast = source.press_fast,
        .fadeSlow = source.fade_slow,
        .slideSoft = source.slide_soft,
        .popupOpen = source.popup_open,
        .springSnappy = source.spring_snappy,
    };
  }();
  return tokens;
}

}  // namespace WindowMotion
