#include "pch.hpp"

#include "ui/components/easing_editor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "ui/theme/theme_tokens.hpp"

namespace minimize::ui::components {
using ::minimize::ui::motion::MotionContext;
using ::minimize::ui::theme::CenteredTextTop;

bool EasingGraphEditor(const MotionContext& motion, const char* id, animation::CubicBezier* bezier,
                       const ImVec2& size, float scale, float alpha, bool* changed,
                       ImFont* caption_font) {
  (void)motion;
  if (!bezier || size.x < 8.0f || size.y < 8.0f) return false;
  if (changed) *changed = false;
  if (!caption_font) caption_font = ImGui::GetFont();

  ImGui::PushID(id);
  // Square plot on top; four compact value fields (x1,y1,x2,y2) below.
  const float field_h = 24.0f * scale;
  const float row_pad = 2.0f * scale;
  const float input_band = field_h + row_pad * 2.0f;
  const float side = std::max(8.0f, std::min(size.x, std::max(8.0f, size.y - input_band)));
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImDrawList* draw = ImGui::GetWindowDrawList();

  // Keep pad inside the square so plot_min/max never invert.
  const float pad = std::min(10.0f * scale, side * 0.2f);
  const ImVec2 panel_min = origin;
  const ImVec2 panel_max(panel_min.x + side, panel_min.y + side);

  // Plot fills the square with a light pad — no card/plot background.
  const ImVec2 plot_min(panel_min.x + pad, panel_min.y + pad);
  const ImVec2 plot_max(panel_max.x - pad, panel_max.y - pad);
  const float plot_w = std::max(1.0f, plot_max.x - plot_min.x);
  const float plot_h = std::max(1.0f, plot_max.y - plot_min.y);

  // Y range allows mild overshoot so Back-like custom curves stay visible
  constexpr float kYMin = -0.25f;
  constexpr float kYMax = 1.25f;
  const float y_span = kYMax - kYMin;

  auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
  auto clamp_y = [](float v) { return v < kYMin ? kYMin : (v > kYMax ? kYMax : v); };

  auto to_screen = [&](float nx, float ny) -> ImVec2 {
    const float u = clamp01(nx);
    const float v = (clamp_y(ny) - kYMin) / y_span;
    return ImVec2(plot_min.x + u * plot_w, plot_max.y - v * plot_h);
  };
  auto from_screen = [&](ImVec2 p, float* nx, float* ny) {
    *nx = clamp01((p.x - plot_min.x) / plot_w);
    const float v = clamp01((plot_max.y - p.y) / plot_h);
    *ny = kYMin + v * y_span;
  };

  // Graph hit target only (inputs live below).
  ImGui::SetCursorScreenPos(panel_min);
  ImGui::InvisibleButton("##easing_graph", ImVec2(side, side));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();

  // Grid
  for (int i = 0; i <= 4; ++i) {
    const float t = static_cast<float>(i) * 0.25f;
    const ImU32 grid =
        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, (i == 0 || i == 4 ? 0.10f : 0.05f) * alpha));
    const ImVec2 a = to_screen(t, kYMin);
    const ImVec2 b = to_screen(t, kYMax);
    draw->AddLine(a, b, grid, 1.0f);
    const ImVec2 c = to_screen(0.0f, kYMin + t * y_span);
    const ImVec2 d = to_screen(1.0f, kYMin + t * y_span);
    draw->AddLine(c, d, grid, 1.0f);
  }

  // Unit square outline (0..1 time/progress band)
  {
    const ImVec2 a = to_screen(0.0f, 0.0f);
    const ImVec2 b = to_screen(1.0f, 0.0f);
    const ImVec2 c = to_screen(1.0f, 1.0f);
    const ImVec2 d = to_screen(0.0f, 1.0f);
    const ImU32 unit = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.08f * alpha));
    draw->AddLine(a, b, unit, 1.0f);
    draw->AddLine(b, c, unit, 1.0f);
    draw->AddLine(c, d, unit, 1.0f);
    draw->AddLine(d, a, unit, 1.0f);
  }

  // Linear reference (dashed diagonal)
  {
    const ImU32 ref = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f * alpha));
    const int dashes = 18;
    for (int i = 0; i < dashes; i += 2) {
      const float t0 = static_cast<float>(i) / static_cast<float>(dashes);
      const float t1 = static_cast<float>(i + 1) / static_cast<float>(dashes);
      draw->AddLine(to_screen(t0, t0), to_screen(t1, t1), ref, 1.0f);
    }
  }

  // Interactive handles first so the curve draws with the live dragged values.
  const float handle_r = 6.0f * scale;
  const float hit_r = 12.0f * scale;
  ImGuiStorage* storage = ImGui::GetStateStorage();
  const ImGuiID drag_id = ImGui::GetID("##handle");
  int drag_handle = storage->GetInt(drag_id, 0);  // 0 none, 1 = p1, 2 = p2

  ImVec2 p1 = to_screen(bezier->x1, bezier->y1);
  ImVec2 p2 = to_screen(bezier->x2, bezier->y2);
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  auto near_handle = [&](const ImVec2& h) {
    const float dx = mouse.x - h.x;
    const float dy = mouse.y - h.y;
    return dx * dx + dy * dy <= hit_r * hit_r;
  };

  if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (near_handle(p1))
      drag_handle = 1;
    else if (near_handle(p2))
      drag_handle = 2;
    else
      drag_handle = 0;
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) drag_handle = 0;
  storage->SetInt(drag_id, drag_handle);

  bool dragging = false;
  if (drag_handle == 1 || drag_handle == 2) {
    dragging = true;
    float nx = 0.0f;
    float ny = 0.0f;
    from_screen(mouse, &nx, &ny);
    if (ImGui::GetIO().KeyShift) {
      nx = std::round(nx * 50.0f) / 50.0f;
      ny = std::round(ny * 50.0f) / 50.0f;
    }
    if (drag_handle == 1) {
      if (std::abs(bezier->x1 - nx) > 1e-5f || std::abs(bezier->y1 - ny) > 1e-5f) {
        bezier->x1 = nx;
        bezier->y1 = ny;
        if (changed) *changed = true;
      }
    } else {
      if (std::abs(bezier->x2 - nx) > 1e-5f || std::abs(bezier->y2 - ny) > 1e-5f) {
        bezier->x2 = nx;
        bezier->y2 = ny;
        if (changed) *changed = true;
      }
    }
    bezier->ClampHandles();
    p1 = to_screen(bezier->x1, bezier->y1);
    p2 = to_screen(bezier->x2, bezier->y2);
  }

  if (hovered || dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

  const ImVec2 p0 = to_screen(0.0f, 0.0f);
  const ImVec2 p3 = to_screen(1.0f, 1.0f);

  // Control arms
  const ImU32 arm = ImGui::GetColorU32(ImVec4(0.55f, 0.55f, 0.62f, 0.55f * alpha));
  draw->AddLine(p0, p1, arm, 1.5f * scale);
  draw->AddLine(p3, p2, arm, 1.5f * scale);

  // Curve polyline (dense sample of parametric bezier)
  constexpr int kSamples = 64;
  ImVec2 pts[kSamples + 1];
  for (int i = 0; i <= kSamples; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(kSamples);
    float bx = 0.0f;
    float by = 0.0f;
    animation::CubicBezierPoint(*bezier, u, &bx, &by);
    pts[i] = to_screen(bx, by);
  }
  draw->AddPolyline(pts, kSamples + 1,
                    ImGui::GetColorU32(ImVec4(0.75f, 0.78f, 0.90f, 0.18f * alpha)), 0,
                    5.0f * scale);
  draw->AddPolyline(pts, kSamples + 1,
                    ImGui::GetColorU32(ImVec4(0.88f, 0.90f, 0.98f, 0.92f * alpha)), 0,
                    2.0f * scale);

  // Endpoints
  const float end_r = 3.2f * scale;
  draw->AddCircleFilled(p0, end_r, ImGui::GetColorU32(ImVec4(0.75f, 0.75f, 0.78f, alpha)), 16);
  draw->AddCircleFilled(p3, end_r, ImGui::GetColorU32(ImVec4(0.75f, 0.75f, 0.78f, alpha)), 16);

  auto draw_handle = [&](const ImVec2& pos, bool hot) {
    const float r = handle_r * (hot ? 1.12f : 1.0f);
    draw->AddCircleFilled(ImVec2(pos.x, pos.y + 0.8f * scale), r,
                          ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.35f * alpha)), 20);
    draw->AddCircleFilled(pos, r, ImGui::GetColorU32(ImVec4(0.92f, 0.93f, 0.96f, alpha)), 20);
    draw->AddCircle(pos, r, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.25f * alpha)), 20,
                    std::max(1.0f, scale));
    if (hot) {
      draw->AddCircle(pos, r + 2.5f * scale,
                      ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.16f * alpha)), 20, 1.0f);
    }
  };
  draw_handle(p1, drag_handle == 1 || (hovered && near_handle(p1) && drag_handle == 0));
  draw_handle(p2, drag_handle == 2 || (hovered && near_handle(p2) && drag_handle == 0));

  // --- Value inputs below the graph: x1, y1, x2, y2 ---
  const float field_gap = 4.0f * scale;
  const float field_w = std::max(1.0f, (side - field_gap * 3.0f) * 0.25f);
  float* field_values[4] = {&bezier->x1, &bezier->y1, &bezier->x2, &bezier->y2};
  const char* field_ids[4] = {"##bx1", "##by1", "##bx2", "##by2"};
  bool input_active = false;
  bool input_changed = false;

  ImGui::PushFont(caption_font);
  ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(4.0f * scale, std::max(1.0f, (field_h - caption_font->FontSize) * 0.5f)));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scale);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.09f, 0.09f, 0.10f, 0.95f * alpha));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.12f, 0.13f, 0.98f * alpha));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.14f, 0.14f, 0.15f, 1.0f * alpha));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.24f, 0.9f * alpha));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ui::theme::kTextColor.x, ui::theme::kTextColor.y,
                                              ui::theme::kTextColor.z, alpha));

  const float input_row_y = origin.y + side + row_pad;
  for (int i = 0; i < 4; ++i) {
    const float fx = origin.x + static_cast<float>(i) * (field_w + field_gap);
    ImGui::SetCursorScreenPos(ImVec2(std::floor(fx + 0.5f), std::floor(input_row_y + 0.5f)));
    ImGui::SetNextItemWidth(field_w);
    const float before = *field_values[i];
    if (ImGui::InputFloat(field_ids[i], field_values[i], 0.0f, 0.0f, "%.2f")) {
      input_changed = true;
    }
    if (ImGui::IsItemActive() || ImGui::IsItemDeactivated()) input_active = true;
    if (ImGui::IsItemDeactivatedAfterEdit()) input_changed = true;
    if (std::abs(*field_values[i] - before) > 1e-6f) input_changed = true;
  }

  ImGui::PopStyleColor(5);
  ImGui::PopStyleVar(4);
  ImGui::PopFont();

  if (input_changed) {
    bezier->ClampHandles();
    if (changed) *changed = true;
  }

  ImGui::PopID();
  return dragging || active || input_active;
}

}  // namespace minimize::ui::components
