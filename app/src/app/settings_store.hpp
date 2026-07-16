#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace genie::app {

inline constexpr float kDefaultMinimizeDuration = 0.70f;
inline constexpr float kDefaultRestoreDuration = 0.70f;

enum class HotkeyAction : size_t {
  kToggleEffect,
  kOpenSettings,
  kRepairWindows,
  kCount,
};

struct HotkeyBinding {
  std::uint32_t modifiers = 0;
  std::uint32_t virtual_key = 0;

  bool operator==(const HotkeyBinding&) const = default;
};

struct AppSettings {
  bool enabled = true;
  float minimize_duration = kDefaultMinimizeDuration;
  float restore_duration = kDefaultRestoreDuration;
  bool link_speeds = false;
  bool disable_animations_fullscreen = false;
  bool disable_effects_battery_saver = false;
  std::string minimize_easing = "Ease In Out";
  std::string restore_easing = "Ease In Out";
  std::string animation_style = "Gienie classic";
  float genie_strength = 1.0f;
  std::string fade_strength = "Subtle";
  bool show_target_indicator = false;
  std::string close_behavior = "exit";
  bool start_minimized = false;
  bool run_at_startup = false;
  std::vector<std::string> excluded_applications;
  std::array<HotkeyBinding, static_cast<size_t>(HotkeyAction::kCount)> hotkeys = {
      HotkeyBinding{.modifiers = 0x0001u | 0x0002u, .virtual_key = 'G'},
      HotkeyBinding{},
      HotkeyBinding{},
  };
};

[[nodiscard]] std::wstring SettingsFilePath();
[[nodiscard]] AppSettings LoadSettings();
[[nodiscard]] bool SaveSettings(const AppSettings& settings);
[[nodiscard]] std::optional<std::string> NormalizeExecutableName(std::string_view name);
[[nodiscard]] bool ExecutableNamesEqual(std::string_view left, std::string_view right);
[[nodiscard]] bool ContainsExcludedApplication(const std::vector<std::string>& applications,
                                               std::string_view name);
void NormalizeExcludedApplications(std::vector<std::string>* applications);

}  // namespace genie::app
