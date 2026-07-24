#include "pch.hpp"

#include "ui/update_presenter.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "features/update_service.hpp"
#include "imgui.h"
#include "ui/settings_window.hpp"
#include "ui/theme/theme.hpp"
#include "ui/theme/theme_tokens.hpp"

namespace minimize::ui {
namespace {

using features::UpdatePhase;
using features::UpdateSnapshot;
using theme::WithAlpha;

bool IsTransferPhase(UpdatePhase phase) {
  return phase == UpdatePhase::kDownloading || phase == UpdatePhase::kVerifying ||
         phase == UpdatePhase::kStaging;
}

std::string FormatBytes(std::uint64_t bytes) {
  if (bytes < 1024) return std::format("{} B", bytes);
  const double kibibytes = static_cast<double>(bytes) / 1024.0;
  if (kibibytes < 1024.0) return std::format("{:.0f} KB", kibibytes);
  return std::format("{:.1f} MB", kibibytes / 1024.0);
}

bool MotionButton(motion::MotionSystem& motion_system, const motion::MotionTokens& tokens,
                  float scale, ImFont* body_font, ImDrawList* draw, const char* id,
                  const char* label, const ImVec2& minimum, const ImVec2& size, bool primary,
                  float alpha) {
  ImGui::SetCursorScreenPos(minimum);
  ImGui::SetNextItemAllowOverlap();
  const bool clicked = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool held = ImGui::IsItemActive();
  const std::string key_base = std::string("update/button/") + id;
  const float hover = motion_system.AnimateValue(key_base + "/hover", hovered ? 1.0f : 0.0f,
                                                 tokens.hover_fast, 0.0f);
  const float press =
      motion_system.AnimateValue(key_base + "/press", held ? 1.0f : 0.0f, tokens.press_fast, 0.0f);
  const float inset = press * 1.5f * scale;
  const ImVec2 button_min(minimum.x + inset, minimum.y + inset);
  const ImVec2 button_max(minimum.x + size.x - inset, minimum.y + size.y - inset);
  ImVec4 background =
      primary ? ImVec4(0.91f, 0.91f, 0.93f, 1.0f) : ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
  if (primary) {
    background.x = std::min(1.0f, background.x + hover * 0.06f);
    background.y = std::min(1.0f, background.y + hover * 0.06f);
    background.z = std::min(1.0f, background.z + hover * 0.06f);
  } else {
    background.x += hover * 0.05f;
    background.y += hover * 0.05f;
    background.z += hover * 0.05f;
  }
  background.w *= alpha;
  const float rounding = 9.0f * scale;
  draw->AddRectFilled(button_min, button_max, ImGui::GetColorU32(background), rounding);
  if (!primary) {
    draw->AddRect(button_min, button_max, WithAlpha(theme::kBorder, alpha * 0.9f), rounding, 0,
                  std::max(1.0f, scale));
  }
  ImFont* font = body_font ? body_font : ImGui::GetFont();
  const ImVec2 text_size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label);
  const ImU32 text_color = primary ? IM_COL32(22, 22, 24, static_cast<int>(255.0f * alpha))
                                   : IM_COL32(226, 226, 231, static_cast<int>(255.0f * alpha));
  draw->AddText(
      font, font->FontSize,
      ImVec2(std::floor(button_min.x + (button_max.x - button_min.x - text_size.x) * 0.5f),
             std::floor(theme::CenteredTextTop(font, button_min.y, button_max.y - button_min.y))),
      text_color, label);
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  return clicked;
}

void DrawProgress(motion::MotionSystem& motion_system, const motion::MotionTokens& tokens,
                  float scale, ImDrawList* draw, const ImVec2& minimum, float width, float progress,
                  float alpha) {
  const float height = 7.0f * scale;
  const ImVec2 maximum(minimum.x + width, minimum.y + height);
  draw->AddRectFilled(minimum, maximum, IM_COL32(255, 255, 255, static_cast<int>(22.0f * alpha)),
                      height * 0.5f);
  const float displayed =
      motion_system.AnimateValue(motion::MotionKey("update", "download", "progress"),
                                 std::clamp(progress, 0.0f, 1.0f), tokens.spring_soft, 0.0f);
  if (displayed > 0.001f) {
    const ImVec2 fill_max(minimum.x + width * displayed, maximum.y);
    draw->AddRectFilled(minimum, fill_max,
                        IM_COL32(233, 233, 239, static_cast<int>(255.0f * alpha)), height * 0.5f);
  }
}

void DrawHeroUpdateBadge(ImDrawList* draw, const ImVec2& center, float scale, float alpha,
                         UpdatePhase phase) {
  const float badge_size = 68.0f * scale;
  const ImVec2 badge_min(center.x - badge_size * 0.5f, center.y - badge_size * 0.5f);
  const ImVec2 badge_max(center.x + badge_size * 0.5f, center.y + badge_size * 0.5f);
  const float rounding = 16.0f * scale;

  // Frosted Elevated Card Container
  theme::DrawGradientShadow(draw, badge_min, badge_max, rounding, 0.40f * alpha, scale);
  draw->AddRectFilled(badge_min, badge_max, IM_COL32(26, 26, 30, static_cast<int>(245.0f * alpha)),
                      rounding);
  draw->AddRect(badge_min, badge_max, WithAlpha(theme::kBorder, alpha * 0.9f), rounding, 0,
                std::max(1.0f, scale));

  // Progress Spinner Ring
  const float ring_radius = 22.0f * scale;
  const ImU32 track_color = IM_COL32(255, 255, 255, static_cast<int>(20.0f * alpha));
  draw->AddCircle(center, ring_radius, track_color, 48, std::max(1.5f, 2.0f * scale));

  const float speed = (phase == UpdatePhase::kError) ? 0.0f : 0.0034f;
  const float rotation = static_cast<float>((GetTickCount64() % 600000ULL) * speed);
  const float arc_len = (phase == UpdatePhase::kError) ? 6.283185f : 1.90f;
  const float stroke = std::max(1.5f, 2.6f * scale);
  const ImU32 arc_color = (phase == UpdatePhase::kError)
                              ? IM_COL32(235, 110, 110, static_cast<int>(240.0f * alpha))
                              : IM_COL32(238, 238, 244, static_cast<int>(255.0f * alpha));

  draw->PathArcTo(center, ring_radius, rotation, rotation + arc_len, 32);
  draw->PathStroke(arc_color, 0, stroke);

  if (phase != UpdatePhase::kError) {
    const ImVec2 head_pos(center.x + std::cos(rotation + arc_len) * ring_radius,
                          center.y + std::sin(rotation + arc_len) * ring_radius);
    draw->AddCircleFilled(head_pos, 2.0f * scale, IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)));
  }

  // Vector Icon in Badge Center
  const ImU32 icon_col = IM_COL32(236, 236, 240, static_cast<int>(255.0f * alpha));
  const float icon_stroke = std::max(1.5f, 2.0f * scale);

  switch (phase) {
    case UpdatePhase::kDownloading: {
      // Downward Arrow + Tray
      draw->AddLine(ImVec2(center.x, center.y - 7.0f * scale),
                    ImVec2(center.x, center.y + 2.5f * scale), icon_col, icon_stroke);
      draw->AddLine(ImVec2(center.x - 4.0f * scale, center.y - 1.0f * scale),
                    ImVec2(center.x, center.y + 3.0f * scale), icon_col, icon_stroke);
      draw->AddLine(ImVec2(center.x + 4.0f * scale, center.y - 1.0f * scale),
                    ImVec2(center.x, center.y + 3.0f * scale), icon_col, icon_stroke);
      draw->AddLine(ImVec2(center.x - 6.0f * scale, center.y + 7.5f * scale),
                    ImVec2(center.x + 6.0f * scale, center.y + 7.5f * scale), icon_col, icon_stroke);
      break;
    }
    case UpdatePhase::kVerifying:
    case UpdatePhase::kStaging: {
      // Checkmark
      draw->AddLine(ImVec2(center.x - 5.0f * scale, center.y + 0.5f * scale),
                    ImVec2(center.x - 1.5f * scale, center.y + 4.0f * scale), icon_col, icon_stroke);
      draw->AddLine(ImVec2(center.x - 1.5f * scale, center.y + 4.0f * scale),
                    ImVec2(center.x + 5.5f * scale, center.y - 3.5f * scale), icon_col, icon_stroke);
      break;
    }
    case UpdatePhase::kReadyToInstall:
    case UpdatePhase::kInstalling: {
      // Curved sync arrows
      draw->PathArcTo(center, 9.0f * scale, 3.4f, 5.8f, 16);
      draw->PathStroke(icon_col, 0, icon_stroke);
      draw->AddLine(ImVec2(center.x + 6.0f * scale, center.y - 8.0f * scale),
                    ImVec2(center.x + 9.5f * scale, center.y - 4.0f * scale), icon_col, icon_stroke);
      draw->PathArcTo(center, 9.0f * scale, 0.25f, 2.65f, 16);
      draw->PathStroke(icon_col, 0, icon_stroke);
      draw->AddLine(ImVec2(center.x - 6.0f * scale, center.y + 8.0f * scale),
                    ImVec2(center.x - 9.5f * scale, center.y + 4.0f * scale), icon_col, icon_stroke);
      break;
    }
    case UpdatePhase::kError: {
      // Exclamation Mark
      const ImU32 err_col = IM_COL32(240, 120, 120, static_cast<int>(255.0f * alpha));
      draw->AddLine(ImVec2(center.x, center.y - 7.0f * scale),
                    ImVec2(center.x, center.y + 1.5f * scale), err_col, icon_stroke);
      draw->AddCircleFilled(ImVec2(center.x, center.y + 6.0f * scale), 1.8f * scale, err_col);
      break;
    }
    default: {
      // Default download arrow
      draw->AddLine(ImVec2(center.x, center.y - 7.0f * scale),
                    ImVec2(center.x, center.y + 2.5f * scale), icon_col, icon_stroke);
      draw->AddLine(ImVec2(center.x - 4.0f * scale, center.y - 1.0f * scale),
                    ImVec2(center.x, center.y + 3.0f * scale), icon_col, icon_stroke);
      draw->AddLine(ImVec2(center.x + 4.0f * scale, center.y - 1.0f * scale),
                    ImVec2(center.x, center.y + 3.0f * scale), icon_col, icon_stroke);
      draw->AddLine(ImVec2(center.x - 6.0f * scale, center.y + 7.5f * scale),
                    ImVec2(center.x + 6.0f * scale, center.y + 7.5f * scale), icon_col, icon_stroke);
      break;
    }
  }
}

}  // namespace

void UpdatePresenter::DrawUpdateWorkspace(SettingsWindow& window) {
  const UpdateSnapshot snapshot = window.update_service_.GetSnapshot();
  const bool active = window.update_workspace_engaged_ || window.update_resume_active_;
  const float show = window.motion_system_.AnimateValue(
      motion::MotionKey("update", "workspace", "show"), active ? 1.0f : 0.0f,
      active ? motion::MotionSpec::Timed(0.54f, motion::MotionEasing::kSmootherStep)
             : motion::MotionSpec::Timed(0.50f, motion::MotionEasing::kSmootherStep),
      0.0f);
  if (show <= 0.005f) return;

  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  const float scale = window.ui_scale_;
  const float chrome_width = 92.0f * scale;
  const ImU32 workspace_background =
      IM_COL32(20, 20, 22, static_cast<int>(255.0f * show));
  // Foreground overlays always sort above the root draw list. Keep a precise transparent
  // chrome pocket so the original traffic lights remain visible and interactive.
  draw->AddRectFilled(ImVec2(chrome_width, 0.0f), display, workspace_background);
  // Draw interactive top-left traffic lights (Close, Minimize, Restore) directly over the update workspace.
  const motion::MotionContext widget_motion{window.motion_system_, window.motion_tokens_};
  const theme::TrafficLightAction action =
      theme::DrawTrafficLights(widget_motion, ImVec2(0.0f, 0.0f), scale, show);
  if (action == theme::TrafficLightAction::kClose) {
    window.controller_->actions().RequestExit();
  } else if (action == theme::TrafficLightAction::kMinimize) {
    ShowWindow(window.hwnd(), SW_MINIMIZE);
  } else if (action == theme::TrafficLightAction::kZoom) {
    ShowWindow(window.hwnd(), IsZoomed(window.hwnd()) ? SW_RESTORE : SW_MAXIMIZE);
  }

  // Smooth synchronized entrance translation
  const float vertical_shift = (1.0f - show) * 16.0f * scale;
  const ImVec2 center(display.x * 0.5f, display.y * 0.5f - 40.0f * scale + vertical_shift);
  const float alpha = show;

  // Render Redesigned Hero Badge Loading Component
  DrawHeroUpdateBadge(draw, center, scale, alpha, snapshot.phase);

  ImFont* title_font = window.font_medium_ ? window.font_medium_ : ImGui::GetFont();
  ImFont* body_font = window.font_small_ ? window.font_small_ : ImGui::GetFont();
  std::string title;
  std::string detail;
  if (window.update_resume_active_) {
    title = "Switching versions";
    detail = "The window stays right here while the new Version takes over";
  } else {
    switch (snapshot.phase) {
      case UpdatePhase::kDownloading:
        title = snapshot.latest_version.empty() ? "Downloading update"
                                                : "Downloading v" + snapshot.latest_version;
        detail = snapshot.total_bytes > 0
                     ? FormatBytes(snapshot.downloaded_bytes) + " of " +
                           FormatBytes(snapshot.total_bytes)
                     : "Receiving the verified package";
        break;
      case UpdatePhase::kVerifying:
        title = "Verifying package";
        detail = "Checking the SHA-256 signature before anything changes";
        break;
      case UpdatePhase::kStaging:
        title = "Preparing new version";
        detail = "Keeping your current version ready for automatic rollback";
        break;
      case UpdatePhase::kReadyToInstall:
      case UpdatePhase::kInstalling:
        title = "Switching versions";
        detail = "The window stays right here while the new Version takes over";
        break;
      case UpdatePhase::kError:
        title = "Update paused";
        detail = snapshot.error.empty() ? "Nothing was changed. You can safely try again."
                                        : snapshot.error;
        break;
      default:
        title = "Preparing update";
        detail = snapshot.status.empty() ? "Getting everything ready" : snapshot.status;
        break;
    }
  }

  const ImVec2 title_size =
      title_font->CalcTextSizeA(title_font->FontSize, FLT_MAX, 0.0f, title.c_str());
  draw->AddText(title_font, title_font->FontSize,
                ImVec2(center.x - title_size.x * 0.5f, center.y + 54.0f * scale),
                IM_COL32(237, 237, 241, static_cast<int>(255.0f * alpha)), title.c_str());
  if (detail.size() > 78) detail = detail.substr(0, 75) + "...";
  const ImVec2 subtitle_size =
      body_font->CalcTextSizeA(body_font->FontSize, FLT_MAX, 0.0f, detail.c_str());
  draw->AddText(body_font, body_font->FontSize,
                ImVec2(center.x - subtitle_size.x * 0.5f, center.y + 80.0f * scale),
                IM_COL32(145, 145, 154, static_cast<int>(255.0f * alpha)), detail.c_str());

  const float progress_width = 286.0f * scale;
  const ImVec2 progress_min(center.x - progress_width * 0.5f, center.y + 116.0f * scale);
  if (snapshot.phase != UpdatePhase::kError) {
    float progress = snapshot.progress;
    if (window.update_resume_active_ || snapshot.phase == UpdatePhase::kReadyToInstall ||
        snapshot.phase == UpdatePhase::kInstalling) {
      progress = 1.0f;
    }
    DrawProgress(window.motion_system_, window.motion_tokens_, scale, draw, progress_min,
                 progress_width, progress, alpha);
  }

  if (!window.update_resume_active_ && snapshot.phase == UpdatePhase::kDownloading) {
    const ImVec2 cancel_size(96.0f * scale, 34.0f * scale);
    if (MotionButton(window.motion_system_, window.motion_tokens_, scale, window.font_body_, draw,
                     "##update_workspace_cancel", "Cancel",
                     ImVec2(center.x - cancel_size.x * 0.5f, center.y + 150.0f * scale),
                     cancel_size, false, alpha)) {
      window.update_service_.CancelDownload();
      window.update_workspace_engaged_ = false;
    }
  } else if (!window.update_resume_active_ && snapshot.phase == UpdatePhase::kError) {
    const ImVec2 back_size(96.0f * scale, 34.0f * scale);
    const ImVec2 retry_size(126.0f * scale, 34.0f * scale);
    const float gap = 10.0f * scale;
    const float left = center.x - (back_size.x + retry_size.x + gap) * 0.5f;
    if (MotionButton(window.motion_system_, window.motion_tokens_, scale, window.font_body_, draw,
                     "##update_workspace_back", "Back",
                     ImVec2(left, center.y + 142.0f * scale), back_size, false, alpha)) {
      window.controller_->actions().ResumeAfterUpdateHandoverFailure();
      window.update_workspace_engaged_ = false;
      window.update_installer_started_ = false;
    }
    if (MotionButton(window.motion_system_, window.motion_tokens_, scale, window.font_body_, draw,
                     "##update_workspace_retry", "Try again",
                     ImVec2(left + back_size.x + gap, center.y + 142.0f * scale), retry_size, true,
                     alpha)) {
      window.update_installer_started_ = false;
      window.update_service_.DownloadUpdate();
    }
  }

  if (snapshot.phase == UpdatePhase::kReadyToInstall && show >= 0.985f &&
      !window.update_installer_started_) {
    RECT bounds{};
    GetWindowRect(window.hwnd_, &bounds);
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    const bool maximized =
        GetWindowPlacement(window.hwnd_, &placement) && placement.showCmd == SW_SHOWMAXIMIZED;
    window.controller_->actions().PrepareForUpdateHandover();
    if (window.update_service_.LaunchInstaller(
            bounds, static_cast<int>(window.selected_page_), window.current_page_scroll_,
            maximized)) {
      window.update_installer_started_ = true;
    } else {
      window.controller_->actions().ResumeAfterUpdateHandoverFailure();
    }
  }
  if (window.update_installer_started_ && window.update_service_.InstallerHandoverReady()) {
    window.controller_->actions().RequestExit();
  } else if (window.update_installer_started_ &&
             window.update_service_.InstallerHandoverFailed()) {
    window.update_installer_started_ = false;
    window.controller_->actions().ResumeAfterUpdateHandoverFailure();
  }
}

void UpdatePresenter::Render(SettingsWindow& window) {
  DrawUpdateWorkspace(window);
}

}  // namespace minimize::ui
