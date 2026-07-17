#pragma once

#include <functional>
#include <windows.h>

#include "platform/windows/window_event_monitor.hpp"

namespace genie::features {

class EffectPolicy;
class MinimizeFeature;
class PauseController;
class RestoreFeature;
}  // namespace genie::features
namespace genie::platform {
class NativeAnimationBlocker;
}
namespace genie::rendering {
class DesktopCapture;
}
namespace genie::features {

class EffectController final {
public:
  using WindowAction = std::function<bool(HWND)>;
  using WindowSeenAction = std::function<void(HWND, DWORD)>;

  EffectController(EffectPolicy& policy, PauseController& pause, MinimizeFeature& minimize,
                   RestoreFeature& restore);

  [[nodiscard]] bool Start(WindowAction minimize, WindowAction restore,
                           WindowSeenAction window_seen);
  void Stop();
  [[nodiscard]] bool IsActive() const;
  [[nodiscard]] bool IsWindowExcluded(HWND window) const;
  void ApplyExclusionTransitionOverrides(HWND overlay) const;
  void HandleWindowSeen(HWND window, DWORD event, HWND overlay, bool renderer_recovering,
                        platform::NativeAnimationBlocker& animation_blocker,
                        rendering::DesktopCapture* capture, const WindowAction& restore);
  [[nodiscard]] HWND last_foreground_window() const { return last_foreground_window_; }

private:
  EffectPolicy& policy_;
  PauseController& pause_;
  MinimizeFeature& minimize_;
  RestoreFeature& restore_;
  platform::WindowEventMonitor event_monitor_;
  HWND last_foreground_window_ = nullptr;
};

}  // namespace genie::features
