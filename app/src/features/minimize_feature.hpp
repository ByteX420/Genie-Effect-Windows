#pragma once

#include <functional>
#include <optional>
#include <unordered_set>
#include <windows.h>

#include "runtime/run_state.hpp"

namespace genie::features {

class AnimationConfiguration;
class EffectPolicy;
class WindowRecoveryService;
struct RenderingPressure;
}  // namespace genie::features
namespace genie::platform {
class NativeAnimationBlocker;
class TaskbarTargetProvider;
}  // namespace genie::platform
namespace genie::rendering {
class DesktopCapture;
}
namespace genie::runtime {
class AnimationRunPool;
class SnapshotCache;
}  // namespace genie::runtime
namespace genie::features {

struct MinimizeRequest {
  HWND window = nullptr;
  bool shutting_down = false;
  bool renderer_available = false;
};

struct MinimizeExecutionContext {
  HWND overlay = nullptr;
  bool effect_active = false;
  bool renderer_recovering = false;
  bool shutting_down = false;
  rendering::DesktopCapture* capture = nullptr;
  platform::TaskbarTargetProvider* taskbar_targets = nullptr;
  platform::NativeAnimationBlocker* animation_blocker = nullptr;
  AnimationConfiguration* animation_configuration = nullptr;
  const RenderingPressure* rendering_pressure = nullptr;
  std::function<int(HWND)> find_run;
  std::function<int()> find_available_run;
  std::function<void(int, runtime::RunState)> set_state;
  std::function<void(int, HWND, const RECT&)> reset_frame_pacing;
  std::function<void(int)> abort_run;
  std::function<void(HWND)> complete_restore;
  std::function<void(float)> record_capture_duration;
};

class MinimizeFeature final {
public:
  class Transaction final {
  public:
    Transaction() = default;
    Transaction(MinimizeFeature& owner, HWND window);
    ~Transaction();
    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&& other) noexcept;
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void HandOff();

  private:
    MinimizeFeature* owner_ = nullptr;
    HWND window_ = nullptr;
  };

  MinimizeFeature(EffectPolicy& policy, WindowRecoveryService& recovery,
                  runtime::AnimationRunPool& runs, runtime::SnapshotCache& snapshots);

  [[nodiscard]] bool Execute(HWND window, const MinimizeExecutionContext& context);
  [[nodiscard]] std::optional<Transaction> Begin(const MinimizeRequest& request);
  void Complete(HWND window);
  void Cancel(HWND window, bool force_show_if_iconic = true);
  void CancelAll(bool force_show_if_iconic = true);
  // Drop tracking without touching the window (used on shutdown after ReleaseWithoutShowing).
  void ReleaseAll();
  [[nodiscard]] bool IsAnimating(HWND window) const;
  void UpdatePreMinimizeSnapshot(HWND window, HWND overlay, rendering::DesktopCapture* capture,
                                 bool renderer_recovering);
  // Startup seed: flash-restore iconic windows, cache real pixels, re-minimize (one quick blip).
  void SeedSnapshotsForIconicWindows(HWND overlay, rendering::DesktopCapture* capture,
                                     platform::TaskbarTargetProvider* taskbar_targets,
                                     bool renderer_recovering);
  void CompletePendingNativeMinimize(int run_index,
                                     const std::function<void(int, runtime::RunState)>& set_state,
                                     const std::function<void(int)>& abort);

private:
  friend class Transaction;
  void HandOff(HWND window);

  EffectPolicy& policy_;
  WindowRecoveryService& recovery_;
  runtime::AnimationRunPool& runs_;
  runtime::SnapshotCache& snapshots_;
  std::unordered_set<HWND> active_;
};

}  // namespace genie::features
