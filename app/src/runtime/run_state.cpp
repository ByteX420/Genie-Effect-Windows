#include "pch.hpp"

#include "runtime/run_state.hpp"

namespace genie::runtime {

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
  if (from == to) return true;
  if (to == RunState::kAborting) return from != RunState::kIdle;
  switch (from) {
    case RunState::kIdle:
      return to == RunState::kCapturing || to == RunState::kRestoring;
    case RunState::kCapturing:
      return to == RunState::kWaitingForNativeMinimize || to == RunState::kAnimating ||
             to == RunState::kRestoring || to == RunState::kCleaningUp || to == RunState::kIdle;
    case RunState::kWaitingForNativeMinimize:
      return to == RunState::kAnimating || to == RunState::kCleaningUp || to == RunState::kIdle;
    case RunState::kAnimating:
    case RunState::kRestoring:
      return to == RunState::kCleaningUp || to == RunState::kIdle;
    case RunState::kAborting:
      return to == RunState::kCleaningUp;
    case RunState::kCleaningUp:
      return to == RunState::kIdle;
  }
  return false;
}

}  // namespace genie::runtime
