#pragma once

#include <deque>
#include <vector>
#include <windows.h>

#include "runtime/animation_run.hpp"

namespace genie::runtime {

class AnimationRunPool final {
public:
  using Container = std::deque<AnimationRun>;
  using iterator = Container::iterator;
  using const_iterator = Container::const_iterator;

  [[nodiscard]] bool empty() const { return runs_.empty(); }
  [[nodiscard]] std::size_t size() const { return runs_.size(); }
  [[nodiscard]] AnimationRun& operator[](std::size_t index) { return runs_[index]; }
  [[nodiscard]] const AnimationRun& operator[](std::size_t index) const { return runs_[index]; }
  [[nodiscard]] iterator begin() { return runs_.begin(); }
  [[nodiscard]] iterator end() { return runs_.end(); }
  [[nodiscard]] const_iterator begin() const { return runs_.begin(); }
  [[nodiscard]] const_iterator end() const { return runs_.end(); }

  AnimationRun& Add();
  void RemoveLast();
  void Clear();
  [[nodiscard]] std::vector<HWND> DetachAnimatingWindows();
  void ShutdownOverlays();

private:
  Container runs_;
};

}  // namespace genie::runtime
