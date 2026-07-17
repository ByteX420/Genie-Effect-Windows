#include "pch.hpp"

#include "ui/settings_view_model.hpp"

namespace genie::ui {

void SettingsViewModel::Apply(const settings::AppSettings& settings) {
  enabled = settings.enabled;
  minimize_duration = settings.minimize_duration;
  restore_duration = settings.restore_duration;
  link_speeds = settings.link_speeds;
  disable_animations_fullscreen = settings.disable_animations_fullscreen;
  disable_effects_battery_saver = settings.disable_effects_battery_saver;
  minimize_easing = settings.minimize_easing;
  restore_easing = settings.restore_easing;
  minimize_custom_bezier = settings.minimize_custom_bezier;
  restore_custom_bezier = settings.restore_custom_bezier;
  animation_style = settings.animation_style;
  quality_mode = settings.quality_mode;
  genie_strength = settings.genie_strength;
  fade_strength = settings.fade_strength;
  show_target_indicator = settings.show_target_indicator;
  close_behavior = settings.close_behavior;
  run_at_startup = settings.run_at_startup;
  start_minimized = settings.start_minimized;
  excluded_applications = settings.excluded_applications;
  hotkeys = settings.hotkeys;
}

void SettingsViewModel::SetPause(bool paused, bool until_restart) {
  temporarily_paused = paused;
  paused_until_restart = paused && until_restart;
}

void SettingsViewModel::SetHotkeyAvailability(settings::HotkeyAction action, bool available) {
  const std::size_t index = static_cast<std::size_t>(action);
  if (index < hotkey_available.size()) hotkey_available[index] = available;
}

}  // namespace genie::ui
