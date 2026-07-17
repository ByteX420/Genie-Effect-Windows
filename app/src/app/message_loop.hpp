#pragma once

#include <functional>

namespace genie::app {

enum class MessageLoopWait {
  kImmediate,
  kFrame,
  kAnimation,
};

struct MessageLoopCallbacks {
  std::function<bool()> should_stop;
  std::function<void()> update;
  std::function<void()> display_changed;
  std::function<void()> render_settings;
  std::function<MessageLoopWait()> tick_runtime;
  std::function<void()> wait_for_animation;
};

class MessageLoop final {
public:
  [[nodiscard]] int Run(const MessageLoopCallbacks& callbacks) const;
};

}  // namespace genie::app
