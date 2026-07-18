#pragma once

#include <unordered_map>
#include <windows.h>

#include "runtime/animation_run.hpp"

namespace genie::runtime {

class SnapshotCache final {
public:
  using Map = std::unordered_map<HWND, CachedSnapshot>;

  struct Contents {
    Map pre_minimize;
    Map restore;
  };

  [[nodiscard]] Map& PreMinimize() { return pre_minimize_; }
  [[nodiscard]] const Map& PreMinimize() const { return pre_minimize_; }
  [[nodiscard]] Map& Restore() { return restore_; }
  [[nodiscard]] const Map& Restore() const { return restore_; }
  [[nodiscard]] const CachedSnapshot* FindBest(HWND window) const;

  void Prune();
  void Clear();
  [[nodiscard]] Contents TakeAll();

private:
  static constexpr std::size_t kMaximumPreMinimizeSnapshots = 16;
  Map pre_minimize_;
  Map restore_;
};

}  // namespace genie::runtime
