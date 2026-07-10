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

float SafeDivisor(float value) {
  if (std::abs(value) < 0.0001f) {
    return value < 0.0f ? -0.0001f : 0.0001f;
  }
  return value;
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

}  // namespace

bool GenieMeshGenerator::GenerateInto(const RectF& source_rect, const RectF& target_rect,
                                      GenieEdge edge, GenieDirection direction, float progress,
                                      float viewport_height, GenieMesh* mesh) {
  if (mesh == nullptr) {
    return false;
  }
  (void)viewport_height;
  const float oriented_progress =
      direction == GenieDirection::kMinimize ? Clamp01(progress) : 1.0f - Clamp01(progress);

  GenerateScreenPositions(source_rect, target_rect, edge, oriented_progress);

  const bool horizontal = edge == GenieEdge::kTop || edge == GenieEdge::kBottom;
  const int rows = horizontal ? kLongGridSegmentCount : 1;
  const int columns = horizontal ? 1 : kLongGridSegmentCount;

  mesh->vertices.resize(screen_positions_.size());

  for (int row = 0; row <= rows; ++row) {
    const float row_fraction = static_cast<float>(row) / rows;
    for (int column = 0; column <= columns; ++column) {
      const float column_fraction = static_cast<float>(column) / columns;
      const int index = row * (columns + 1) + column;
      const PointF pt = screen_positions_[index];
      const float linear_left =
          source_rect.left + (target_rect.left - source_rect.left) * oriented_progress;
      const float linear_top =
          source_rect.top + (target_rect.top - source_rect.top) * oriented_progress;
      const float linear_right =
          source_rect.right + (target_rect.right - source_rect.right) * oriented_progress;
      const float linear_bottom =
          source_rect.bottom + (target_rect.bottom - source_rect.bottom) * oriented_progress;
      const float linear_x = linear_left + (linear_right - linear_left) * column_fraction;
      const float linear_y = linear_top + (linear_bottom - linear_top) * row_fraction;
      const float strength = std::clamp(strength_, 0.0f, 1.0f);
      mesh->vertices[index] = MeshVertex{
          .x = linear_x + (pt.x - linear_x) * strength,
          .y = linear_y + (pt.y - linear_y) * strength,
          .u = column_fraction,
          .v = row_fraction,
      };
    }
  }

  const bool indices_changed =
      !has_index_layout_ || index_layout_horizontal_ != horizontal || mesh->indices.empty();
  if (indices_changed) {
    mesh->indices.clear();
    AppendCellIndices(rows, columns, &mesh->indices);
    has_index_layout_ = true;
    index_layout_horizontal_ = horizontal;
  }
  return indices_changed;
}

void GenieMeshGenerator::GenerateScreenPositions(const RectF& source_rect, const RectF& target_rect,
                                                 GenieEdge edge, float progress) {
  const bool horizontal = edge == GenieEdge::kTop || edge == GenieEdge::kBottom;
  const int row_count = horizontal ? kLongGridSegmentCount : 1;
  const int column_count = horizontal ? 1 : kLongGridSegmentCount;

  screen_positions_.resize(static_cast<std::size_t>((row_count + 1) * (column_count + 1)));

  const float slide_progress = Clamp01(progress / kSlideAnimationEndFraction);
  const float translate_progress = Clamp01((progress - kTranslateAnimationStartFraction) /
                                           (1.0f - kTranslateAnimationStartFraction));

  if (edge == GenieEdge::kBottom) {
    const float left_bezier_bottom_x = source_rect.left;
    const float right_bezier_bottom_x = source_rect.right;

    const float left_edge_distance_to_move = target_rect.left - source_rect.left;
    const float right_edge_distance_to_move = target_rect.right - source_rect.right;
    const float vertical_distance_to_move = target_rect.top - source_rect.top;

    const float bezier_top_y = target_rect.top;
    const float bezier_bottom_y = source_rect.top;
    const float bezier_height = bezier_top_y - bezier_bottom_y;

    const float translation = translate_progress * vertical_distance_to_move;
    const float top_edge_vertical_position =
        std::min(source_rect.bottom + translation, target_rect.bottom);
    const float bottom_edge_vertical_position = source_rect.top + translation;

    const float left_bezier_top_x =
        left_bezier_bottom_x + (slide_progress * left_edge_distance_to_move);
    const float right_bezier_top_x =
        right_bezier_bottom_x + (slide_progress * right_edge_distance_to_move);

    auto left_bezier_position = [&](float y) -> float {
      if (y < bezier_bottom_y) {
        return left_bezier_bottom_x;
      }
      if (y < bezier_top_y) {
        float p = QuadraticEaseInOut((y - bezier_bottom_y) / SafeDivisor(bezier_height));
        return (p * (left_bezier_top_x - left_bezier_bottom_x)) + left_bezier_bottom_x;
      }
      return left_bezier_top_x;
    };

    auto right_bezier_position = [&](float y) -> float {
      if (y < bezier_bottom_y) {
        return right_bezier_bottom_x;
      }
      if (y < bezier_top_y) {
        float p = QuadraticEaseInOut((y - bezier_bottom_y) / SafeDivisor(bezier_height));
        return (p * (right_bezier_top_x - right_bezier_bottom_x)) + right_bezier_bottom_x;
      }
      return right_bezier_top_x;
    };

    for (int row = 0; row <= row_count; ++row) {
      float position = static_cast<float>(row) / row_count;
      float y = (top_edge_vertical_position * position) +
                (bottom_edge_vertical_position * (1.0f - position));
      float x_min = left_bezier_position(y);
      float x_max = right_bezier_position(y);
      const std::size_t base = static_cast<std::size_t>(row * 2);
      screen_positions_[base] = PointF{.x = x_min, .y = y};
      screen_positions_[base + 1] = PointF{.x = x_max, .y = y};
    }
  } else if (edge == GenieEdge::kTop) {
    const float left_bezier_top_x = source_rect.left;
    const float right_bezier_top_x = source_rect.right;

    const float left_edge_distance_to_move = target_rect.left - source_rect.left;
    const float right_edge_distance_to_move = target_rect.right - source_rect.right;
    const float vertical_distance_to_move = target_rect.bottom - source_rect.bottom;

    const float bezier_top_y = source_rect.bottom;
    const float bezier_bottom_y = target_rect.bottom;
    const float bezier_height = bezier_top_y - bezier_bottom_y;

    const float left_bezier_bottom_x =
        left_bezier_top_x + (slide_progress * left_edge_distance_to_move);
    const float right_bezier_bottom_x =
        right_bezier_top_x + (slide_progress * right_edge_distance_to_move);

    auto left_bezier_position = [&](float y) -> float {
      if (y < bezier_bottom_y) {
        return left_bezier_bottom_x;
      }
      if (y < bezier_top_y) {
        float p = QuadraticEaseInOut((y - bezier_bottom_y) / SafeDivisor(bezier_height));
        return (p * (left_bezier_top_x - left_bezier_bottom_x)) + left_bezier_bottom_x;
      }
      return left_bezier_top_x;
    };

    auto right_bezier_position = [&](float y) -> float {
      if (y < bezier_bottom_y) {
        return right_bezier_bottom_x;
      }
      if (y < bezier_top_y) {
        float p = QuadraticEaseInOut((y - bezier_bottom_y) / SafeDivisor(bezier_height));
        return (p * (right_bezier_top_x - right_bezier_bottom_x)) + right_bezier_bottom_x;
      }
      return right_bezier_top_x;
    };

    const float translation = translate_progress * vertical_distance_to_move;
    const float top_edge_vertical_position = source_rect.bottom + translation;
    const float bottom_edge_vertical_position =
        std::max(source_rect.top + translation, target_rect.top);

    for (int row = 0; row <= row_count; ++row) {
      float position = static_cast<float>(row) / row_count;
      float y = (top_edge_vertical_position * position) +
                (bottom_edge_vertical_position * (1.0f - position));
      float x_min = left_bezier_position(y);
      float x_max = right_bezier_position(y);
      const std::size_t base = static_cast<std::size_t>(row * 2);
      screen_positions_[base] = PointF{.x = x_min, .y = y};
      screen_positions_[base + 1] = PointF{.x = x_max, .y = y};
    }
  } else if (edge == GenieEdge::kLeft) {
    const float top_bezier_left_y = source_rect.bottom;
    const float bottom_bezier_left_y = source_rect.top;

    const float top_edge_distance_to_move = target_rect.bottom - source_rect.bottom;
    const float bottom_edge_distance_to_move = target_rect.top - source_rect.top;
    const float horizontal_distance_to_move = target_rect.left - source_rect.left;

    const float bezier_left_x = source_rect.left;
    const float bezier_right_x = target_rect.left;
    const float bezier_width = bezier_right_x - bezier_left_x;

    const float translation = translate_progress * horizontal_distance_to_move;
    const float left_edge_horizontal_position = source_rect.left + translation;
    const float right_edge_horizontal_position =
        std::min(source_rect.right + translation, target_rect.right);

    const float top_bezier_right_y =
        top_bezier_left_y + (slide_progress * top_edge_distance_to_move);
    const float bottom_bezier_right_y =
        bottom_bezier_left_y + (slide_progress * bottom_edge_distance_to_move);

    auto top_bezier_position = [&](float x) -> float {
      if (x < bezier_left_x) {
        return top_bezier_left_y;
      }
      if (x < bezier_right_x) {
        float p = QuadraticEaseInOut((x - bezier_left_x) / SafeDivisor(bezier_width));
        return (p * (top_bezier_right_y - top_bezier_left_y)) + top_bezier_left_y;
      }
      return top_bezier_right_y;
    };

    auto bottom_bezier_position = [&](float x) -> float {
      if (x < bezier_left_x) {
        return bottom_bezier_left_y;
      }
      if (x < bezier_right_x) {
        float p = QuadraticEaseInOut((x - bezier_left_x) / SafeDivisor(bezier_width));
        return (p * (bottom_bezier_right_y - bottom_bezier_left_y)) + bottom_bezier_left_y;
      }
      return bottom_bezier_right_y;
    };

    for (int col = 0; col <= column_count; ++col) {
      float position = static_cast<float>(col) / column_count;
      float x = (left_edge_horizontal_position * (1.0f - position)) +
                (right_edge_horizontal_position * position);
      float y_top = top_bezier_position(x);
      float y_bottom = bottom_bezier_position(x);
      screen_positions_[static_cast<std::size_t>(col)] = PointF{.x = x, .y = y_bottom};
      screen_positions_[static_cast<std::size_t>(column_count + 1 + col)] =
          PointF{.x = x, .y = y_top};
    }
  } else if (edge == GenieEdge::kRight) {
    const float top_bezier_right_y = source_rect.bottom;
    const float bottom_bezier_right_y = source_rect.top;

    const float top_edge_distance_to_move = target_rect.bottom - source_rect.bottom;
    const float bottom_edge_distance_to_move = target_rect.top - source_rect.top;
    const float horizontal_distance_to_move = target_rect.right - source_rect.right;

    const float bezier_left_x = target_rect.right;
    const float bezier_right_x = source_rect.right;
    const float bezier_width = bezier_right_x - bezier_left_x;

    const float translation = translate_progress * horizontal_distance_to_move;
    const float left_edge_horizontal_position =
        std::max(source_rect.left + translation, target_rect.left);
    const float right_edge_horizontal_position = source_rect.right + translation;

    const float top_bezier_left_y =
        top_bezier_right_y + (slide_progress * top_edge_distance_to_move);
    const float bottom_bezier_left_y =
        bottom_bezier_right_y + (slide_progress * bottom_edge_distance_to_move);

    auto top_bezier_position = [&](float x) -> float {
      if (x < bezier_left_x) {
        return top_bezier_left_y;
      }
      if (x < bezier_right_x) {
        float p = QuadraticEaseInOut((x - bezier_left_x) / SafeDivisor(bezier_width));
        return (p * (top_bezier_right_y - top_bezier_left_y)) + top_bezier_left_y;
      }
      return top_bezier_right_y;
    };

    auto bottom_bezier_position = [&](float x) -> float {
      if (x < bezier_left_x) {
        return bottom_bezier_left_y;
      }
      if (x < bezier_right_x) {
        float p = QuadraticEaseInOut((x - bezier_left_x) / SafeDivisor(bezier_width));
        return (p * (bottom_bezier_right_y - bottom_bezier_left_y)) + bottom_bezier_left_y;
      }
      return bottom_bezier_right_y;
    };

    for (int col = 0; col <= column_count; ++col) {
      float position = static_cast<float>(col) / column_count;
      float x = (left_edge_horizontal_position * (1.0f - position)) +
                (right_edge_horizontal_position * position);
      float y_top = top_bezier_position(x);
      float y_bottom = bottom_bezier_position(x);
      screen_positions_[static_cast<std::size_t>(col)] = PointF{.x = x, .y = y_bottom};
      screen_positions_[static_cast<std::size_t>(column_count + 1 + col)] =
          PointF{.x = x, .y = y_top};
    }
  }
}

}  // namespace genie::animation
