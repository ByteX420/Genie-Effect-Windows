#pragma once

#include <cstddef>
#include <windows.h>

namespace genie::runtime {
class SnapshotCache;
}

namespace genie::features {

class WindowRecoveryService final {
public:
  explicit WindowRecoveryService(runtime::SnapshotCache& snapshots);

  void Restore(HWND window, bool force_show_if_iconic = true);
  // Drop Genie cloak/transparency/props without unminimizing. When finish_as_minimized is
  // true, mid-minimize (cloaked but still visible) windows are native-minimized first.
  void ReleaseWithoutShowing(HWND window, bool finish_as_minimized = false);
  [[nodiscard]] std::size_t HealLeftovers();
  void HealUntrackedWindows();
  [[nodiscard]] bool restoring() const { return restoring_; }

private:
  runtime::SnapshotCache& snapshots_;
  bool restoring_ = false;
};

}  // namespace genie::features
