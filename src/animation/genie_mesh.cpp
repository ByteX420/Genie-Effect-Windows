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

float Lerp(float from, float to, float progress) { return from + ((to - from) * progress); }

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

float BezierAxisPosition(float value, float bezier_min, float bezier_max, float start_value,
                         float end_value) {
  if (value < bezier_min) {
    return start_value;
  }
  if (value < bezier_max) {
    const float progress =
        QuadraticEaseInOut((value - bezier_min) / SafeDivisor(bezier_max - bezier_min));
    return Lerp(start_value, end_value, progress);
  }
  return end_value;
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
    const float slide_progress = Clamp01(progress / kSlideAnimationEndFraction);
    const float translate_progress =
        Clamp01((progress - kTranslateAnimationStartFraction) /
                (1.0f - kTranslateAnimationStartFraction));

    if (edge == GenieEdge::kBottom) {
      const float left_bezier_top_x = source_rect.left;
      const float right_bezier_top_x = source_rect.right;
      const float left_bezier_bottom_x =
          Lerp(left_bezier_top_x, target_rect.left, slide_progress);
      const float right_bezier_bottom_x =
          Lerp(right_bezier_top_x, target_rect.right, slide_progress);

      const float translation = translate_progress * (target_rect.bottom - source_rect.bottom);
      const float top_edge_y = source_rect.bottom + translation;
      const float bottom_edge_y = std::max(source_rect.top + translation, target_rect.top);
      const float bezier_min_y = target_rect.bottom;
      const float bezier_max_y = source_rect.bottom;

      for (int row = 0; row <= row_count; ++row) {
        const float row_fraction = static_cast<float>(row) / row_count;
        const float y = Lerp(bottom_edge_y, top_edge_y, row_fraction);
        const float left_x = BezierAxisPosition(y, bezier_min_y, bezier_max_y,
                                                left_bezier_bottom_x, left_bezier_top_x);
        const float right_x = BezierAxisPosition(y, bezier_min_y, bezier_max_y,
                                                 right_bezier_bottom_x, right_bezier_top_x);
        positions.push_back(PointF{.x = left_x, .y = y});
        positions.push_back(PointF{.x = right_x, .y = y});
      }
      return positions;
    }

    if (edge == GenieEdge::kTop) {
      const float left_bezier_bottom_x = source_rect.left;
      const float right_bezier_bottom_x = source_rect.right;
      const float left_bezier_top_x =
          Lerp(left_bezier_bottom_x, target_rect.left, slide_progress);
      const float right_bezier_top_x =
          Lerp(right_bezier_bottom_x, target_rect.right, slide_progress);

      const float translation = translate_progress * (target_rect.top - source_rect.top);
      const float top_edge_y = std::min(source_rect.bottom + translation, target_rect.bottom);
      const float bottom_edge_y = source_rect.top + translation;
      const float bezier_min_y = source_rect.top;
      const float bezier_max_y = target_rect.top;

      for (int row = 0; row <= row_count; ++row) {
        const float row_fraction = static_cast<float>(row) / row_count;
        const float y = Lerp(bottom_edge_y, top_edge_y, row_fraction);
        const float left_x = BezierAxisPosition(y, bezier_min_y, bezier_max_y,
                                                left_bezier_bottom_x, left_bezier_top_x);
        const float right_x = BezierAxisPosition(y, bezier_min_y, bezier_max_y,
                                                 right_bezier_bottom_x, right_bezier_top_x);
        positions.push_back(PointF{.x = left_x, .y = y});
        positions.push_back(PointF{.x = right_x, .y = y});
      }
      return positions;
    }

    for (int row = 0; row <= row_count; ++row) {
      const float row_fraction = static_cast<float>(row) / row_count;
      const float y = Lerp(source_rect.top, target_rect.top, row_fraction);
      const float left_x = Lerp(source_rect.left, target_rect.left, progress);
      const float right_x = Lerp(source_rect.right, target_rect.right, progress);

      positions.push_back(PointF{.x = left_x, .y = y});
      positions.push_back(PointF{.x = right_x, .y = y});
    }
  } else {
    const float slide_progress = Clamp01(progress / kSlideAnimationEndFraction);
    const float translate_progress =
        Clamp01((progress - kTranslateAnimationStartFraction) /
                (1.0f - kTranslateAnimationStartFraction));

    if (edge == GenieEdge::kLeft) {
      const float top_bezier_right_y = source_rect.bottom;
      const float bottom_bezier_right_y = source_rect.top;
      const float top_bezier_left_y =
          Lerp(top_bezier_right_y, target_rect.bottom, slide_progress);
      const float bottom_bezier_left_y =
          Lerp(bottom_bezier_right_y, target_rect.top, slide_progress);

      const float translation = translate_progress * (target_rect.right - source_rect.right);
      const float left_edge_x = std::max(source_rect.left + translation, target_rect.left);
      const float right_edge_x = source_rect.right + translation;
      const float bezier_min_x = target_rect.right;
      const float bezier_max_x = source_rect.right;

      for (int row = 0; row <= row_count; ++row) {
        const bool top_row = row == 1;
        for (int column = 0; column <= column_count; ++column) {
          const float column_fraction = static_cast<float>(column) / column_count;
          const float x = Lerp(left_edge_x, right_edge_x, column_fraction);
          const float y = BezierAxisPosition(
              x, bezier_min_x, bezier_max_x,
              top_row ? top_bezier_left_y : bottom_bezier_left_y,
              top_row ? top_bezier_right_y : bottom_bezier_right_y);
          positions.push_back(PointF{.x = x, .y = y});
        }
      }
      return positions;
    }

    if (edge == GenieEdge::kRight) {
      const float top_bezier_left_y = source_rect.bottom;
      const float bottom_bezier_left_y = source_rect.top;
      const float top_bezier_right_y =
          Lerp(top_bezier_left_y, target_rect.bottom, slide_progress);
      const float bottom_bezier_right_y =
          Lerp(bottom_bezier_left_y, target_rect.top, slide_progress);

      const float translation = translate_progress * (target_rect.left - source_rect.left);
      const float left_edge_x = source_rect.left + translation;
      const float right_edge_x = std::min(source_rect.right + translation, target_rect.right);
      const float bezier_min_x = source_rect.left;
      const float bezier_max_x = target_rect.left;

      for (int row = 0; row <= row_count; ++row) {
        const bool top_row = row == 1;
        for (int column = 0; column <= column_count; ++column) {
          const float column_fraction = static_cast<float>(column) / column_count;
          const float x = Lerp(left_edge_x, right_edge_x, column_fraction);
          const float y = BezierAxisPosition(
              x, bezier_min_x, bezier_max_x,
              top_row ? top_bezier_left_y : bottom_bezier_left_y,
              top_row ? top_bezier_right_y : bottom_bezier_right_y);
          positions.push_back(PointF{.x = x, .y = y});
        }
      }
      return positions;
    }

    for (int column = 0; column <= column_count; ++column) {
      const float column_fraction = static_cast<float>(column) / column_count;
      positions.push_back(PointF{
          .x = Lerp(source_rect.left, target_rect.left, column_fraction),
          .y = Lerp(source_rect.top, target_rect.top, progress),
      });
    }
    for (int column = 0; column <= column_count; ++column) {
      const float column_fraction = static_cast<float>(column) / column_count;
      positions.push_back(PointF{
          .x = Lerp(source_rect.left, target_rect.left, column_fraction),
          .y = Lerp(source_rect.bottom, target_rect.bottom, progress),
      });
    }
  }

  return positions;
}

}  // namespace genie::animation
