#include "pch.hpp"

#include "features/pause_controller.hpp"

namespace genie::features {

void PauseController::PauseFor(std::uint64_t duration_ms, std::uint64_t now_ms) {
  until_restart_ = false;
  until_ms_ = now_ms + duration_ms;
}

void PauseController::PauseUntilRestart() {
  until_restart_ = true;
  until_ms_ = 0;
}

void PauseController::Resume() {
  until_restart_ = false;
  until_ms_ = 0;
}

bool PauseController::Update(std::uint64_t now_ms) {
  if (until_ms_ == 0 || now_ms < until_ms_) return false;
  until_ms_ = 0;
  return true;
}

bool PauseController::IsPaused(std::uint64_t now_ms) const {
  return until_restart_ || (until_ms_ != 0 && now_ms < until_ms_);
}

}  // namespace genie::features
