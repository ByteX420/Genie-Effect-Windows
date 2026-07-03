#pragma once

#include <windows.h>

namespace genie::platform {

class NativeAnimationBlocker {
public:
  NativeAnimationBlocker() = default;
  ~NativeAnimationBlocker();

  NativeAnimationBlocker(const NativeAnimationBlocker&) = delete;
  NativeAnimationBlocker& operator=(const NativeAnimationBlocker&) = delete;

  bool Enable(HWND ignored_window);
  void Disable();
  void SetTransitionsDisabledForWindow(HWND window, bool disabled);

private:
  HWND ignored_window_ = nullptr;
  bool enabled_ = false;
};

}  // namespace genie::platform
