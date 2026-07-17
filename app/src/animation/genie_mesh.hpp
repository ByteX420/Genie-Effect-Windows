#pragma once

#include <algorithm>
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

enum class AnimationStyle {
  kClassic,
  kCurvy,
  kSquash,
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
  void SetStrength(float strength) { strength_ = strength; }
  void SetLongGridSegmentCount(int segment_count) {
    long_grid_segment_count_ = std::clamp(segment_count, 2, 100);
  }
  // Returns true when the index layout changed and must be uploaded again.
  bool GenerateInto(const RectF& source_rect, const RectF& target_rect, GenieEdge edge,
                    GenieDirection direction, AnimationStyle style, float progress,
                    float viewport_height, GenieMesh* mesh);

private:
  void GenerateClassicPositions(const RectF& source_rect, const RectF& target_rect, GenieEdge edge,
                                float progress);
  void GenerateCurvyPositions(const RectF& source_rect, const RectF& target_rect, GenieEdge edge,
                              float progress);
  void GenerateSquashPositions(const RectF& source_rect, const RectF& target_rect, float progress);

  std::vector<PointF> screen_positions_;
  bool has_index_layout_ = false;
  int index_layout_rows_ = 0;
  int index_layout_columns_ = 0;
  int long_grid_segment_count_ = 50;
  float strength_ = 1.0f;
};

}  // namespace genie::animation
