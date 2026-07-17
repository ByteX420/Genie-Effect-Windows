#pragma once

#include <string>

#include "animation/easing.hpp"
#include "features/diagnostics_service.hpp"
#include "settings/hotkey_binding.hpp"

namespace genie::ui {

enum class TemporaryPauseAction {
  kResume,
  kTenMinutes,
  kOneHour,
  kUntilRestart,
};

enum class HotkeyUpdateResult {
  kSuccess,
  kInvalid,
  kDuplicate,
  kUnavailable,
  kSaveFailed,
};

class SettingsActions {
public:
  virtual ~SettingsActions() = default;

  virtual bool SetEnabled(bool enabled) = 0;
  virtual bool SetAnimationDurations(float minimize, float restore, bool save) = 0;
  virtual bool SetLinkSpeeds(bool linked) = 0;
  virtual bool SetDisableAnimationsFullscreen(bool enabled) = 0;
  virtual bool SetDisableEffectsBatterySaver(bool enabled) = 0;
  virtual bool SetEasing(const std::string& minimize, const std::string& restore) = 0;
  virtual bool SetCustomEasingBezier(bool minimize, animation::CubicBezier bezier, bool save) = 0;
  virtual bool SetAnimationStyle(const std::string& style) = 0;
  virtual bool SetQualityMode(const std::string& mode) = 0;
  virtual bool SetGenieStrength(float strength, bool save) = 0;
  virtual bool SetFadeStrength(const std::string& strength) = 0;
  virtual bool SetTargetIndicator(bool enabled) = 0;
  virtual bool SetCloseBehavior(const std::string& behavior) = 0;
  virtual bool SetStartupOptions(bool run_at_startup, bool start_minimized) = 0;
  virtual bool SetApplicationExcluded(const std::string& executable, bool excluded) = 0;
  virtual void SetTemporaryPause(TemporaryPauseAction action) = 0;
  virtual HotkeyUpdateResult SetHotkey(settings::HotkeyAction action,
                                       settings::HotkeyBinding binding) = 0;
  virtual void ExecuteHotkeyAction(settings::HotkeyAction action) = 0;
  [[nodiscard]] virtual features::DiagnosticsSnapshot GetDiagnostics() const = 0;
  virtual bool ExecuteDiagnosticsAction(features::DiagnosticsAction action) = 0;
  virtual void HealWindows() = 0;
  virtual void RequestExit() = 0;
};

}  // namespace genie::ui
