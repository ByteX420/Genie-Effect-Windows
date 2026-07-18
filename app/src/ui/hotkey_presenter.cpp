#include "pch.hpp"

#include "ui/hotkey_presenter.hpp"

namespace genie::ui {

std::string FormatHotkey(const settings::HotkeyBinding& binding) {
  if (binding.virtual_key == 0) return "Disabled";
  std::string text;
  if ((binding.modifiers & MOD_CONTROL) != 0) text += "Ctrl + ";
  if ((binding.modifiers & MOD_ALT) != 0) text += "Alt + ";
  if ((binding.modifiers & MOD_SHIFT) != 0) text += "Shift + ";
  if ((binding.modifiers & MOD_WIN) != 0) text += "Win + ";
  if ((binding.virtual_key >= 'A' && binding.virtual_key <= 'Z') ||
      (binding.virtual_key >= '0' && binding.virtual_key <= '9')) {
    text.push_back(static_cast<char>(binding.virtual_key));
    return text;
  }
  const UINT scan_code = MapVirtualKeyW(binding.virtual_key, MAPVK_VK_TO_VSC);
  wchar_t name[64]{};
  if (GetKeyNameTextW(static_cast<LONG>(scan_code << 16), name, 64) <= 0) return text;
  const int required = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
  if (required <= 1) return text;
  std::string utf8(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8.data(), required, nullptr, nullptr);
  utf8.pop_back();
  return text + utf8;
}

const char* HotkeyUpdateMessage(HotkeyUpdateResult result) {
  switch (result) {
    case HotkeyUpdateResult::kSuccess:
      return "Hotkey updated";
    case HotkeyUpdateResult::kInvalid:
      return "Choose a valid key combination";
    case HotkeyUpdateResult::kDuplicate:
      return "That combination is already used by Minimize Effect";
    case HotkeyUpdateResult::kUnavailable:
      return "Hotkey unavailable; another application may use it";
    case HotkeyUpdateResult::kSaveFailed:
      return "Could not save the hotkey setting";
  }
  return "Hotkey could not be updated";
}

}  // namespace genie::ui
