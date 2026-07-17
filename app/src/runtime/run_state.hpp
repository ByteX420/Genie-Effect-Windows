#pragma once

namespace genie::runtime {

enum class RunState {
  kIdle,
  kCapturing,
  kWaitingForNativeMinimize,
  kAnimating,
  kRestoring,
  kAborting,
  kCleaningUp,
};

[[nodiscard]] const char* RunStateName(RunState state);
[[nodiscard]] bool IsRunStateTransitionAllowed(RunState from, RunState to);

}  // namespace genie::runtime
