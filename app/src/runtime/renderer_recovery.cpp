#include "pch.hpp"

#include "runtime/renderer_recovery.hpp"

#include <algorithm>

namespace minimize::runtime {

void RendererRecovery::Begin() {
  pending_ = true;
  delay_ms_ = kInitialDelayMilliseconds;
  next_attempt_ms_ = GetTickCount64();
}

bool RendererRecovery::ShouldAttempt(ULONGLONG now) const {
  return pending_ && now >= next_attempt_ms_;
}

void RendererRecovery::MarkSucceeded() {
  pending_ = false;
  delay_ms_ = kInitialDelayMilliseconds;
  next_attempt_ms_ = 0;
}

void RendererRecovery::ScheduleRetry(ULONGLONG now) {
  if (delay_ms_ == 0) delay_ms_ = kInitialDelayMilliseconds;
  next_attempt_ms_ = now + delay_ms_;
  delay_ms_ = std::min(delay_ms_ * 2, kMaximumDelayMilliseconds);
}

}  // namespace minimize::runtime
