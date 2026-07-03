#include "pch.hpp"

#include "platform/native_animation_blocker.hpp"

#include <iostream>

#include "platform/window_util.hpp"

namespace genie::platform {

NativeAnimationBlocker::~NativeAnimationBlocker() { Disable(); }

bool NativeAnimationBlocker::Enable(HWND ignored_window) {
  ignored_window_ = ignored_window;
  enabled_ = true;
  for (HWND window : EnumerateTopLevelWindows(ignored_window_)) {
    SetDwmTransitionsDisabled(window, true);
  }
  return true;
}

void NativeAnimationBlocker::Disable() {
  enabled_ = false;
  for (HWND window : EnumerateTopLevelWindows(ignored_window_)) {
    SetDwmTransitionsDisabled(window, false);
  }
}

void NativeAnimationBlocker::SetTransitionsDisabledForWindow(HWND window, bool disabled) {
  if (IsInterestingTopLevelWindow(window, ignored_window_)) {
    if (enabled_ && !disabled) {
      return;
    }
    SetDwmTransitionsDisabled(window, disabled);
  }
}

}  // namespace genie::platform
