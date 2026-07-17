#pragma once

#include <windows.h>

#include "animation/genie_mesh.hpp"
#include "animation/geometry.hpp"

namespace genie::platform {

struct TaskbarTarget {
  genie::animation::RectF rect;
  genie::animation::GenieEdge edge = genie::animation::GenieEdge::kBottom;
};

class TaskbarTargetProvider {
public:
  [[nodiscard]] TaskbarTarget GetTargetForWindow(HWND window, const RECT& window_rect) const;

private:
  [[nodiscard]] bool TryGetEnvironmentTarget(RECT* target_rect) const;
  [[nodiscard]] RECT GetShellTaskbarRect() const;
};

}  // namespace genie::platform
