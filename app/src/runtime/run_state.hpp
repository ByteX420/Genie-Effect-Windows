#pragma once

#include <cstdint>

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
[[nodiscard]] std::uint64_t RunStateTimeoutMs(RunState state);

}  // namespace genie::runtime
