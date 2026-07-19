#include "pch.hpp"

#include "runtime/run_state.hpp"

namespace genie::runtime {
namespace {

constexpr bool IsTransitionAllowed(RunState from, RunState to) {
  if (from == to) return true;
  if (to == RunState::kAborting) return from != RunState::kIdle;
  switch (from) {
    case RunState::kIdle:
      return to == RunState::kCapturing || to == RunState::kRestoring;
    case RunState::kCapturing:
      return to == RunState::kWaitingForNativeMinimize || to == RunState::kAnimating ||
             to == RunState::kRestoring || to == RunState::kCleaningUp ||
             to == RunState::kIdle;
    case RunState::kWaitingForNativeMinimize:
      return to == RunState::kAnimating || to == RunState::kRestoring ||
             to == RunState::kCleaningUp || to == RunState::kIdle;
    case RunState::kAnimating:
      return to == RunState::kRestoring || to == RunState::kCleaningUp ||
             to == RunState::kIdle;
    case RunState::kRestoring:
      return to == RunState::kAnimating || to == RunState::kCleaningUp ||
             to == RunState::kIdle;
    case RunState::kAborting:
      return to == RunState::kCleaningUp;
    case RunState::kCleaningUp:
      return to == RunState::kIdle;
  }
  return false;
}

constexpr std::uint64_t TimeoutMs(RunState state) {
  switch (state) {
    case RunState::kCapturing:
      return 2500;
    case RunState::kWaitingForNativeMinimize:
      return 2000;
    case RunState::kAborting:
    case RunState::kCleaningUp:
      return 1500;
    case RunState::kAnimating:
    case RunState::kRestoring:
      return 10000;
    case RunState::kIdle:
      return 0;
  }
  return 10000;
}

static_assert(IsTransitionAllowed(RunState::kWaitingForNativeMinimize,
                                  RunState::kRestoring));
static_assert(IsTransitionAllowed(RunState::kAnimating, RunState::kRestoring) &&
              IsTransitionAllowed(RunState::kRestoring, RunState::kAnimating) &&
              IsTransitionAllowed(RunState::kAnimating, RunState::kRestoring));
static_assert(TimeoutMs(RunState::kAnimating) == TimeoutMs(RunState::kRestoring));

}  // namespace

const char* RunStateName(RunState state) {
  switch (state) {
    case RunState::kIdle:
      return "Idle";
    case RunState::kCapturing:
      return "Capturing";
    case RunState::kWaitingForNativeMinimize:
      return "WaitingForNativeMinimize";
    case RunState::kAnimating:
      return "Animating";
    case RunState::kRestoring:
      return "Restoring";
    case RunState::kAborting:
      return "Aborting";
    case RunState::kCleaningUp:
      return "CleaningUp";
  }
  return "Unknown";
}

bool IsRunStateTransitionAllowed(RunState from, RunState to) {
  return IsTransitionAllowed(from, to);
}

std::uint64_t RunStateTimeoutMs(RunState state) {
  return TimeoutMs(state);
}

}  // namespace genie::runtime
