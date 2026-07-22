#pragma once

#include <windows.h>

#include "animation/minimize_mesh.hpp"
#include "animation/geometry.hpp"

namespace minimize::platform {

struct TaskbarTarget {
  minimize::animation::RectF rect;
  minimize::animation::MinimizeEdge edge = minimize::animation::MinimizeEdge::kBottom;
};

class TaskbarTargetProvider {
public:
  [[nodiscard]] TaskbarTarget GetTargetForWindow(HWND window, const RECT& window_rect) const;

private:
  [[nodiscard]] bool TryGetEnvironmentTarget(RECT* target_rect) const;
  [[nodiscard]] RECT GetShellTaskbarRect() const;
};

}  // namespace minimize::platform
