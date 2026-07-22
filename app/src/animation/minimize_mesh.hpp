#pragma once

#include <cstdint>
#include <vector>

#include "animation/geometry.hpp"

namespace minimize::animation {

enum class MinimizeEdge {
  kTop,
  kBottom,
  kLeft,
  kRight,
};

enum class MinimizeDirection {
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

struct MinimizeMesh {
  std::vector<MeshVertex> vertices;
  std::vector<std::uint16_t> indices;
};

class MinimizeMeshGenerator {
public:
  void SetStrength(float strength);
  void SetLongGridSegmentCount(int segment_count);

  // Returns true when the index layout changed and must be uploaded again.
  bool GenerateInto(const RectF& source_rect, const RectF& target_rect, MinimizeEdge edge,
                    MinimizeDirection direction, AnimationStyle style, float progress,
                    float viewport_height, MinimizeMesh* mesh);

private:
  void GenerateClassicPositions(const RectF& source_rect, const RectF& target_rect, MinimizeEdge edge,
                                float progress);
  void GenerateCurvyPositions(const RectF& source_rect, const RectF& target_rect, MinimizeEdge edge,
                              float progress);
  void GenerateSquashPositions(const RectF& source_rect, const RectF& target_rect, float progress);

  std::vector<PointF> screen_positions_;
  bool has_index_layout_ = false;
  int index_layout_rows_ = 0;
  int index_layout_columns_ = 0;
  int long_grid_segment_count_ = 50;
  float strength_ = 1.0f;
};

}  // namespace minimize::animation
