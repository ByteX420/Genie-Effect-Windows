#pragma once

#include <windows.h>

#include "runtime/animation_run.hpp"
#include "runtime/animation_run_pool.hpp"

namespace genie::runtime {

class FrameScheduler final {
public:
  FrameScheduler() = default;
  ~FrameScheduler();

  FrameScheduler(const FrameScheduler&) = delete;
  FrameScheduler& operator=(const FrameScheduler&) = delete;

  void Initialize();
  void Shutdown();
  void Wake();
  void Reset(AnimationRun& run, HWND window, const RECT& animation_bounds);
  void UpdateMonitor(AnimationRun& run);
  [[nodiscard]] bool IsDue(const AnimationRun& run) const;
  [[nodiscard]] unsigned int Advance(AnimationRun& run);
  void Wait(const AnimationRunPool& runs);
  void EndFallbackTimerResolution();

private:
  void BeginFallbackTimerResolution();

  HANDLE timer_ = nullptr;
  bool high_resolution_timer_ = false;
  bool fallback_resolution_active_ = false;
  UINT fallback_period_ms_ = 0;
};

}  // namespace genie::runtime
