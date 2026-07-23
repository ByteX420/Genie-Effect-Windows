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

}  // namespace

void UpdatePresenter::DrawUpdateWorkspace(SettingsWindow& window) {
  const UpdateSnapshot snapshot = window.update_service_.GetSnapshot();
  const bool active = window.update_workspace_engaged_ || window.update_resume_active_;
  const float show = window.motion_system_.AnimateValue(
      motion::MotionKey("update", "workspace", "show"), active ? 1.0f : 0.0f,
      active ? motion::MotionSpec::Timed(0.36f, motion::MotionEasing::kSmootherStep, 0.10f)
             : motion::MotionSpec::Timed(0.40f, motion::MotionEasing::kSmootherStep),
      0.0f);
  if (show <= 0.005f) return;

  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  const float scale = window.ui_scale_;
  const float chrome_width = 92.0f * scale;
  const float chrome_height = 42.0f * scale;
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

  const float loader_reveal = window.motion_system_.AnimateValue(
      motion::MotionKey("update", "workspace", "loader"), active ? 1.0f : 0.84f,
      window.motion_tokens_.spring_soft, 0.84f);
  const ImVec2 center(display.x * 0.5f,
                      display.y * 0.5f - 42.0f * scale + (1.0f - loader_reveal) * 14.0f * scale);
  const float alpha = show * loader_reveal;
  const float radius = 29.0f * scale;
  // Absolute system time keeps the spinner phase continuous across the two processes.
  const float rotation = static_cast<float>((GetTickCount64() % 600000ULL) * 0.00215);
  draw->AddCircle(center, radius, IM_COL32(255, 255, 255, static_cast<int>(20.0f * alpha)), 64,
                  std::max(1.0f, 2.0f * scale));
  for (int arc = 0; arc < 3; ++arc) {
    const float offset = rotation + static_cast<float>(arc) * 2.0943951f;
    const float arc_alpha = alpha * (1.0f - static_cast<float>(arc) * 0.22f);
    draw->PathArcTo(center, radius, offset, offset + 0.90f, 20);
    draw->PathStroke(IM_COL32(238, 238, 244, static_cast<int>(255.0f * arc_alpha)), 0,
                     std::max(1.5f, (2.7f - static_cast<float>(arc) * 0.35f) * scale));
  }

  ImFont* title_font = window.font_medium_ ? window.font_medium_ : ImGui::GetFont();
  ImFont* body_font = window.font_small_ ? window.font_small_ : ImGui::GetFont();
  std::string title;
  std::string detail;
  if (window.update_resume_active_) {
    title = "Switching versions";
    detail = "The window stays right here while the new build takes over";
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
        title = "Verifying every byte";
        detail = "Checking the SHA-256 signature before anything changes";
        break;
      case UpdatePhase::kStaging:
        title = "Preparing the new version";
        detail = "Keeping your current version ready for automatic rollback";
        break;
      case UpdatePhase::kReadyToInstall:
      case UpdatePhase::kInstalling:
        title = "Switching versions";
        detail = "The window stays right here while the new build takes over";
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
                ImVec2(center.x - title_size.x * 0.5f, center.y + 55.0f * scale),
                IM_COL32(237, 237, 241, static_cast<int>(255.0f * alpha)), title.c_str());
  if (detail.size() > 78) detail = detail.substr(0, 75) + "...";
  const ImVec2 subtitle_size =
      body_font->CalcTextSizeA(body_font->FontSize, FLT_MAX, 0.0f, detail.c_str());
  draw->AddText(body_font, body_font->FontSize,
                ImVec2(center.x - subtitle_size.x * 0.5f, center.y + 82.0f * scale),
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
