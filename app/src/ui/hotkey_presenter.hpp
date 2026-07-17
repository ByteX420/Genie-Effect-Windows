#pragma once

#include <string>

#include "settings/hotkey_binding.hpp"
#include "ui/settings_actions.hpp"

namespace genie::ui {

[[nodiscard]] std::string FormatHotkey(const settings::HotkeyBinding& binding);
[[nodiscard]] const char* HotkeyUpdateMessage(HotkeyUpdateResult result);

}  // namespace genie::ui
