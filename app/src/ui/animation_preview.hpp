#pragma once

#include <windows.h>

namespace genie::ui {

class AnimationPreview final {
public:
  ~AnimationPreview();

  void Start(HWND owner);
  void Update(HWND owner, float minimize_duration, float restore_duration);
  void Close();
  [[nodiscard]] bool active() const { return active_; }

private:
  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);

  HWND window_ = nullptr;
  bool active_ = false;
  int phase_ = 0;
  ULONGLONG phase_started_ms_ = 0;
  bool dragging_ = false;
  POINT drag_offset_{};
};

}  // namespace genie::ui
