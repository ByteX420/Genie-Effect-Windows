#include "pch.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string_view>

#include "app/application_runtime.hpp"
#include "core/environment.hpp"
#include "core/logger.hpp"

#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#include "platform/windows/app_container_permissions.hpp"
#include "platform/windows/display_info.hpp"
#include "platform/windows/power_status.hpp"
#include "platform/windows/process_info.hpp"
#include "platform/windows/startup_manager.hpp"
#include "platform/windows/window_diagnostics.hpp"
#include "platform/windows/window_properties.hpp"
#include "platform/windows/window_state.hpp"
#include "settings/exclusion_rules.hpp"

namespace genie::app {

bool ApplicationRuntime::SetEnabled(bool enabled) {
  const bool result = settings_mutations_.SetEnabled(enabled, [this] {
    effect_policy_.SetEnabled(settings_service_.Get().enabled);
    RefreshEffectRuntimeState();
  });
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

void ApplicationRuntime::SetTemporaryPause(ui::TemporaryPauseAction action) {
  if (action == ui::TemporaryPauseAction::kTenMinutes) {
    pause_controller_.PauseFor(10ULL * 60ULL * 1000ULL, GetTickCount64());
  } else if (action == ui::TemporaryPauseAction::kOneHour) {
    pause_controller_.PauseFor(60ULL * 60ULL * 1000ULL, GetTickCount64());
  } else if (action == ui::TemporaryPauseAction::kUntilRestart) {
    pause_controller_.PauseUntilRestart();
  } else {
    pause_controller_.Resume();
  }
  RefreshEffectRuntimeState();
  settings_window_.UpdatePauseState(IsTemporarilyPaused(), pause_controller_.until_restart());
}

void ApplicationRuntime::UnregisterAllHotkeys() { hotkey_controller_.UnregisterAll(); }

void ApplicationRuntime::RegisterConfiguredHotkeys() {
  hotkey_controller_.RegisterConfigured([this](settings::HotkeyAction action, bool available) {
    settings_window_.SetHotkeyRegistrationStatus(action, available);
  });
}

ui::HotkeyUpdateResult ApplicationRuntime::SetHotkey(genie::settings::HotkeyAction action,
                                                     genie::settings::HotkeyBinding binding) {
  const auto result = hotkey_controller_.Replace(
      action, binding, [this](settings::HotkeyAction changed_action, bool available) {
        settings_window_.SetHotkeyRegistrationStatus(changed_action, available);
      });
  if (result != ui::HotkeyUpdateResult::kSuccess) return result;
  settings_window_.UpdateState(settings_service_.Get());
  RegisterConfiguredHotkeys();
  return result;
}

void ApplicationRuntime::ExecuteHotkeyAction(genie::settings::HotkeyAction action) {
  switch (action) {
    case genie::settings::HotkeyAction::kToggleEffect:
      (void)SetEnabled(!settings_service_.Get().enabled);
      break;
    case genie::settings::HotkeyAction::kOpenSettings:
      settings_window_.Show(true);
      break;
    case genie::settings::HotkeyAction::kRepairWindows:
      HealLeftoverWindows();
      break;
    case genie::settings::HotkeyAction::kCount:
      break;
  }
}

features::DiagnosticsSnapshot ApplicationRuntime::BuildDiagnosticsSnapshot() const {
  int active_animations = 0;
  for (const runtime::AnimationRun& run : runs_) {
    if (run.animating_window != nullptr || run.overlay.active()) ++active_animations;
  }
  return diagnostics_service_.Build(features::DiagnosticsContext{
      .effect_active = IsEffectActive(),
      .hook_installed = cbt_hook_manager_.IsInstalled(),
      .renderer_recovering = renderer_recovery_.pending(),
      .d3d_device = d3d_device_.get(),
      .active_animations = active_animations,
      .startup_repair = startup_repair_status_,
      .reference_window = effect_controller_.last_foreground_window(),
      .taskbar_targets = &taskbar_target_provider_,
  });
}

features::DiagnosticsSnapshot ApplicationRuntime::GetDiagnostics() const {
  return BuildDiagnosticsSnapshot();
}

bool ApplicationRuntime::ExecuteDiagnosticsAction(features::DiagnosticsAction action) {
  return diagnostics_service_.Execute(
      action, features::DiagnosticsActions{
                  .owner = settings_window_.hwnd(),
                  .build_report = [this] { return BuildDiagnosticsSnapshot().report; },
                  .repair_windows =
                      [this] {
                        HealLeftoverWindows();
                        return true;
                      },
                  .restart_renderer =
                      [this] {
                        BeginAnimationRendererRecovery();
                        return !renderer_recovery_.pending() && d3d_device_ != nullptr;
                      },
              });
}

bool ApplicationRuntime::SetAnimationDurations(float minimize_duration, float restore_duration,
                                               bool save) {
  const bool result =
      settings_mutations_.SetAnimationDurations(minimize_duration, restore_duration, save);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetLinkSpeeds(bool linked) {
  const bool result = settings_mutations_.SetLinkSpeeds(linked);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetDisableAnimationsFullscreen(bool enabled) {
  const bool result = settings_mutations_.SetDisableAnimationsFullscreen(enabled, [this] {
    effect_policy_.Configure(settings_service_.Get());
    UpdateFullscreenSuppression(true);
  });
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetDisableEffectsBatterySaver(bool enabled) {
  const bool result = settings_mutations_.SetDisableEffectsBatterySaver(enabled, [this] {
    effect_policy_.Configure(settings_service_.Get());
    UpdatePowerState(true);
  });
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetEasing(const std::string& minimize_easing,
                                   const std::string& restore_easing) {
  const bool result = settings_mutations_.SetEasing(minimize_easing, restore_easing);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetCustomEasingBezier(bool is_minimize, animation::CubicBezier bezier,
                                               bool save) {
  const bool result = settings_mutations_.SetCustomEasingBezier(is_minimize, bezier, save);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetAnimationStyle(const std::string& style) {
  const bool result = settings_mutations_.SetAnimationStyle(style);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetQualityMode(const std::string& mode) {
  const bool result = settings_mutations_.SetQualityMode(mode);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetGenieStrength(float strength, bool save) {
  const bool result = settings_mutations_.SetGenieStrength(strength, save);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetFadeStrength(const std::string& strength) {
  const bool result = settings_mutations_.SetFadeStrength(strength);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::ResetMotionSettings() {
  const bool result = settings_mutations_.ResetMotionSettings();
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetTargetIndicator(bool enabled) {
  const bool result = settings_mutations_.SetTargetIndicator(enabled);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetSmartSkipUnderLoad(bool enabled) {
  const bool result = settings_mutations_.SetSmartSkipUnderLoad(enabled, [this] {
    effect_policy_.Configure(settings_service_.Get());
  });
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetCloseBehavior(const std::string& close_behavior) {
  const bool result = settings_mutations_.SetCloseBehavior(close_behavior);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetStartupOptions(bool run_at_startup, bool start_minimized) {
  const bool result = settings_mutations_.SetStartupOptions(run_at_startup, start_minimized);
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

bool ApplicationRuntime::SetApplicationExcluded(const std::string& executable_name, bool excluded) {
  const bool result = settings_mutations_.SetApplicationExcluded(executable_name, excluded, [this] {
    effect_policy_.Configure(settings_service_.Get());
    effect_controller_.ApplyExclusionTransitionOverrides(GetOverlayWindow());
  });
  settings_window_.UpdateState(settings_service_.Get());
  return result;
}

namespace {

std::wstring PickSettingsFile(HWND owner, bool save) {
  wchar_t path[MAX_PATH]{};
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFilter = L"JSON settings (*.json)\0*.json\0All files (*.*)\0*.*\0";
  ofn.lpstrFile = path;
  ofn.nMaxFile = static_cast<DWORD>(std::size(path));
  ofn.lpstrDefExt = L"json";
  ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
              (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  ofn.lpstrTitle = save ? L"Export Genie Effect settings" : L"Import Genie Effect settings";
  const BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
  return ok ? std::wstring(path) : std::wstring{};
}

}  // namespace

ui::SettingsFileOperationResult ApplicationRuntime::ExportSettings() {
  const std::wstring path = PickSettingsFile(settings_window_.hwnd(), true);
  if (path.empty()) {
    return ui::SettingsFileOperationResult{.result = ui::SettingsFileResult::kCancelled};
  }
  if (!settings_mutations_.ExportSettingsToFile(path)) {
    return ui::SettingsFileOperationResult{
        .result = ui::SettingsFileResult::kFailed,
        .message = "Could not export settings",
        .is_error = true,
    };
  }
  return ui::SettingsFileOperationResult{
      .result = ui::SettingsFileResult::kSuccess,
      .message = "Settings exported",
  };
}

ui::SettingsFileOperationResult ApplicationRuntime::ImportSettings() {
  const std::wstring path = PickSettingsFile(settings_window_.hwnd(), false);
  if (path.empty()) {
    return ui::SettingsFileOperationResult{.result = ui::SettingsFileResult::kCancelled};
  }
  bool startup_registration_failed = false;
  const bool loaded = settings_mutations_.ImportSettingsFromFile(
      path,
      [this] {
        effect_policy_.Configure(settings_service_.Get());
        RegisterConfiguredHotkeys();
        RefreshEffectRuntimeState();
      },
      &startup_registration_failed);
  settings_window_.UpdateState(settings_service_.Get());
  if (!loaded) {
    return ui::SettingsFileOperationResult{
        .result = ui::SettingsFileResult::kFailed,
        .message = "Could not import settings",
        .is_error = true,
    };
  }
  if (startup_registration_failed) {
    return ui::SettingsFileOperationResult{
        .result = ui::SettingsFileResult::kSuccess,
        .message = "Settings imported, startup registration failed",
        .is_error = true,
    };
  }
  return ui::SettingsFileOperationResult{
      .result = ui::SettingsFileResult::kSuccess,
      .message = "Settings imported",
  };
}

}  // namespace genie::app
