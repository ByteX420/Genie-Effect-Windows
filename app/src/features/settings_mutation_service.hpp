#pragma once

#include <functional>
#include <string>

#include "animation/easing.hpp"
#include "settings/settings_service.hpp"

namespace genie::features {

class SettingsMutationService final {
public:
  explicit SettingsMutationService(settings::SettingsService& settings) : settings_(settings) {}

  bool SetEnabled(bool enabled, const std::function<void()>& applied);
  bool SetAnimationDurations(float minimize, float restore, bool save);
  bool SetLinkSpeeds(bool linked);
  bool SetDisableAnimationsFullscreen(bool enabled, const std::function<void()>& applied);
  bool SetDisableEffectsBatterySaver(bool enabled, const std::function<void()>& applied);
  bool SetEasing(const std::string& minimize, const std::string& restore);
  bool SetCustomEasingBezier(bool minimize, animation::CubicBezier bezier, bool save);
  bool SetAnimationStyle(const std::string& style);
  bool SetQualityMode(const std::string& mode);
  bool SetGenieStrength(float strength, bool save);
  bool SetFadeStrength(const std::string& strength);
  bool SetTargetIndicator(bool enabled);
  bool SetCloseBehavior(const std::string& behavior);
  bool SetStartupOptions(bool run_at_startup, bool start_minimized);
  bool SetApplicationExcluded(const std::string& executable, bool excluded,
                              const std::function<void()>& applied);

private:
  settings::SettingsService& settings_;
};

}  // namespace genie::features
