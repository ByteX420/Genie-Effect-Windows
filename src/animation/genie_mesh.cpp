#include "pch.hpp"

#include "animation/genie_mesh.hpp"

#include <algorithm>
#include <cmath>

namespace genie::animation {
namespace {

constexpr float kSlideAnimationEndFraction = 0.5f;
constexpr float kTranslateAnimationStartFraction = 0.4f;
constexpr int kLongGridSegmentCount = 50;

float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

float QuadraticEaseInOut(float value) {
  const float clamped = Clamp01(value);
  if (clamped < 0.5f) {
    return 2.0f * clamped * clamped;
  }
  return (-2.0f * clamped * clamped) + (4.0f * clamped) - 1.0f;
}

RectF ToBottomLeftRect(const RectF& rect, float viewport_height) {
  return RectF{
      .left = rect.left,
      .top = viewport_height - rect.bottom,
      .right = rect.right,
      .bottom = viewport_height - rect.top,
  };
}

PointF ToTopLeftPoint(const PointF& point, float viewport_height) {
  return PointF{.x = point.x, .y = viewport_height - point.y};
}

void AppendCellIndices(int rows, int columns, std::vector<std::uint16_t>* indices) {
  indices->reserve(static_cast<std::size_t>(rows * columns * 6));
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      const auto lower_left = static_cast<std::uint16_t>(row * (columns + 1) + column);
      const auto lower_right = static_cast<std::uint16_t>(lower_left + 1);
      const auto upper_left = static_cast<std::uint16_t>((row + 1) * (columns + 1) + column);
      const auto upper_right = static_cast<std::uint16_t>(upper_left + 1);

      indices->push_back(lower_left);
      indices->push_back(upper_left);
      indices->push_back(lower_right);
      indices->push_back(lower_right);
      indices->push_back(upper_left);
      indices->push_back(upper_right);
    }
  }
}

float SafeDivisor(float value) {
  if (std::abs(value) < 0.0001f) {
    return value < 0.0f ? -0.0001f : 0.0001f;
  }
  return value;
}

}  // namespace

GenieMesh GenieMeshGenerator::Generate(const RectF& source_rect, const RectF& target_rect,
                                       GenieEdge edge, GenieDirection direction, float progress,
                                       float viewport_height) const {
  const float oriented_progress =
      direction == GenieDirection::kMinimize ? Clamp01(progress) : 1.0f - Clamp01(progress);
  const RectF source_bottom_left = ToBottomLeftRect(source_rect, viewport_height);
  const RectF target_bottom_left = ToBottomLeftRect(target_rect, viewport_height);
  const std::vector<PointF> bottom_left_positions =
      GenerateBottomLeftPositions(source_bottom_left, target_bottom_left, edge, oriented_progress);

  const bool horizontal = edge == GenieEdge::kTop || edge == GenieEdge::kBottom;
  const int rows = horizontal ? kLongGridSegmentCount : 1;
  const int columns = horizontal ? 1 : kLongGridSegmentCount;

  GenieMesh mesh;
  mesh.vertices.reserve(bottom_left_positions.size());

  for (int row = 0; row <= rows; ++row) {
    const float row_fraction = static_cast<float>(row) / rows;
    for (int column = 0; column <= columns; ++column) {
      const float column_fraction = static_cast<float>(column) / columns;
      const int index = row * (columns + 1) + column;
      const PointF top_left = ToTopLeftPoint(bottom_left_positions[index], viewport_height);
      mesh.vertices.push_back(MeshVertex{
          .x = top_left.x,
          .y = top_left.y,
          .u = column_fraction,
          .v = 1.0f - row_fraction,
      });
    }
  }

  AppendCellIndices(rows, columns, &mesh.indices);
  return mesh;
}

std::vector<PointF> GenieMeshGenerator::GenerateBottomLeftPositions(const RectF& source_rect,
                                                                    const RectF& target_rect,
                                                                    GenieEdge edge,
                                                                    float progress) const {
  const bool horizontal = edge == GenieEdge::kTop || edge == GenieEdge::kBottom;
  const int row_count = horizontal ? kLongGridSegmentCount : 1;
  const int column_count = horizontal ? 1 : kLongGridSegmentCount;

  std::vector<PointF> positions;
  positions.reserve(static_cast<std::size_t>((row_count + 1) * (column_count + 1)));

  if (horizontal) {
    for (int row = 0; row <= row_count; ++row) {
      const float row_fraction = static_cast<float>(row) / row_count;
      const float v = (edge == GenieEdge::kBottom) ? row_fraction : (1.0f - row_fraction);

      // Local progress calculation for each row to create the funnel/sucking effect
      const float start_time = 0.4f * v;
      const float end_time = 0.5f + 0.5f * v;
      const float p = Clamp01((progress - start_time) / (end_time - start_time));
      const float eased_p = QuadraticEaseInOut(p);

      // Interpolate Y coordinate between source and target positions
      const float source_y =
          source_rect.top + (source_rect.bottom - source_rect.top) * row_fraction;
      const float target_y =
          target_rect.top + (target_rect.bottom - target_rect.top) * row_fraction;
      const float y = source_y + (target_y - source_y) * eased_p;

      // Interpolate X coordinates (left and right edges) to squeeze the width
      const float source_left = source_rect.left;
      const float target_left = target_rect.left;
      const float left_x = source_left + (target_left - source_left) * eased_p;

      const float source_right = source_rect.right;
      const float target_right = target_rect.right;
      const float right_x = source_right + (target_right - source_right) * eased_p;

      positions.push_back(PointF{.x = left_x, .y = y});
      positions.push_back(PointF{.x = right_x, .y = y});
    }
  } else {
    // Vertical taskbar (kLeft or kRight)
    // Row 0: Bottom edge of the grid
    for (int column = 0; column <= column_count; ++column) {
      const float column_fraction = static_cast<float>(column) / column_count;
      const float v = (edge == GenieEdge::kLeft) ? column_fraction : (1.0f - column_fraction);

      const float start_time = 0.4f * v;
      const float end_time = 0.5f + 0.5f * v;
      const float p = Clamp01((progress - start_time) / (end_time - start_time));
      const float eased_p = QuadraticEaseInOut(p);

      // Interpolate X coordinate between source and target positions
      const float source_x =
          source_rect.left + (source_rect.right - source_rect.left) * column_fraction;
      const float target_x =
          target_rect.left + (target_rect.right - target_rect.left) * column_fraction;
      const float x = source_x + (target_x - source_x) * eased_p;

      // Interpolate Y coordinate for the bottom boundary
      const float source_bottom = source_rect.top;
      const float target_bottom = target_rect.top;
      const float bottom_y = source_bottom + (target_bottom - source_bottom) * eased_p;

      positions.push_back(PointF{.x = x, .y = bottom_y});
    }
    // Row 1: Top edge of the grid
    for (int column = 0; column <= column_count; ++column) {
      const float column_fraction = static_cast<float>(column) / column_count;
      const float v = (edge == GenieEdge::kLeft) ? column_fraction : (1.0f - column_fraction);

      const float start_time = 0.4f * v;
      const float end_time = 0.5f + 0.5f * v;
      const float p = Clamp01((progress - start_time) / (end_time - start_time));
      const float eased_p = QuadraticEaseInOut(p);

      // Interpolate X coordinate between source and target positions
      const float source_x =
          source_rect.left + (source_rect.right - source_rect.left) * column_fraction;
      const float target_x =
          target_rect.left + (target_rect.right - target_rect.left) * column_fraction;
      const float x = source_x + (target_x - source_x) * eased_p;

      // Interpolate Y coordinate for the top boundary
      const float source_top = source_rect.bottom;
      const float target_top = target_rect.bottom;
      const float top_y = source_top + (target_top - source_top) * eased_p;

      positions.push_back(PointF{.x = x, .y = top_y});
    }
  }

  return positions;
}

}  // namespace genie::animation
