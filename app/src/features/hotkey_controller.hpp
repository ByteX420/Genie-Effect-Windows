#pragma once

#include <functional>

#include "platform/windows/global_hotkey_manager.hpp"
#include "settings/settings_service.hpp"
#include "ui/settings_actions.hpp"

namespace genie::features {

class HotkeyController final {
public:
  HotkeyController(settings::SettingsService& settings,
                   platform::windows::GlobalHotkeyManager& manager)
      : settings_(settings), manager_(manager) {}

  void SetWindow(HWND window);
  void RegisterConfigured(const std::function<void(settings::HotkeyAction, bool)>& status);
  void UnregisterAll();
  [[nodiscard]] ui::HotkeyUpdateResult Replace(
      settings::HotkeyAction action, settings::HotkeyBinding binding,
      const std::function<void(settings::HotkeyAction, bool)>& status);

private:
  settings::SettingsService& settings_;
  platform::windows::GlobalHotkeyManager& manager_;
};

}  // namespace genie::features
