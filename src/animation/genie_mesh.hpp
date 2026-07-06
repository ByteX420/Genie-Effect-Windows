#pragma once

#include <cstdint>
#include <vector>

#include "animation/geometry.hpp"

namespace genie::animation {

enum class GenieEdge {
  kTop,
  kBottom,
  kLeft,
  kRight,
};

enum class GenieDirection {
  kMinimize,
  kMaximize,
};

struct MeshVertex {
  float x = 0.0f;
  float y = 0.0f;
  float u = 0.0f;
  float v = 0.0f;
};

struct GenieMesh {
  std::vector<MeshVertex> vertices;
  std::vector<std::uint16_t> indices;
};

class GenieMeshGenerator {
public:
  [[nodiscard]] GenieMesh Generate(const RectF& source_rect, const RectF& target_rect,
                                   GenieEdge edge, GenieDirection direction, float progress,
                                   float viewport_height) const;

private:
  [[nodiscard]] std::vector<PointF> GenerateScreenPositions(const RectF& source_rect,
                                                           const RectF& target_rect,
                                                           GenieEdge edge,
                                                           float progress) const;
};

}  // namespace genie::animation
