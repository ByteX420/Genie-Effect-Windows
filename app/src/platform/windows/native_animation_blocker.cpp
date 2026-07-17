#include "pch.hpp"

#include "platform/windows/native_animation_blocker.hpp"

#include <iostream>

#include "platform/windows/window_state.hpp"

namespace genie::platform {

NativeAnimationBlocker::~NativeAnimationBlocker() { Disable(); }

bool NativeAnimationBlocker::Enable(HWND ignored_window) {
  if (enabled_) Disable();
  ignored_window_ = ignored_window;
  enabled_ = true;
  for (HWND window : EnumerateTopLevelWindows(ignored_window_)) {
    SetDwmTransitionsDisabled(window, true);
    blocked_windows_.insert(window);
  }
  return true;
}

void NativeAnimationBlocker::Disable() {
  enabled_ = false;
  for (HWND window : blocked_windows_) {
    if (IsWindow(window)) SetDwmTransitionsDisabled(window, false);
  }
  blocked_windows_.clear();
  for (HWND window : EnumerateTopLevelWindows(ignored_window_)) {
    SetDwmTransitionsDisabled(window, false);
  }
}

void NativeAnimationBlocker::SetTransitionsDisabledForWindow(HWND window, bool disabled) {
  if (disabled) {
    if (!enabled_ || !IsInterestingTopLevelWindow(window, ignored_window_)) return;
    SetDwmTransitionsDisabled(window, disabled);
    blocked_windows_.insert(window);
    return;
  }

  if (IsWindow(window)) SetDwmTransitionsDisabled(window, false);
  blocked_windows_.erase(window);
}

}  // namespace genie::platform
