#pragma once

#include <string>

#include "settings/hotkey_binding.hpp"
#include "ui/settings_actions.hpp"

namespace minimize::ui {

[[nodiscard]] std::string FormatHotkey(const settings::HotkeyBinding& binding);
[[nodiscard]] const char* HotkeyUpdateMessage(HotkeyUpdateResult result);

}  // namespace minimize::ui
