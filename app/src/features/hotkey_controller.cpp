#include "pch.hpp"

#include "features/hotkey_controller.hpp"

#include "core/logger.hpp"

namespace minimize::features {

void HotkeyController::SetWindow(HWND window) { manager_.SetWindow(window); }

void HotkeyController::RegisterConfigured(
    const std::function<void(settings::HotkeyAction, bool)>& status) {
  const auto available = manager_.RegisterAll(settings_.Get().hotkeys);
  for (std::size_t index = 0; index < available.size(); ++index) {
    const auto action = static_cast<settings::HotkeyAction>(index);
    status(action, available[index]);
    if (!available[index])
      core::LogDebug(L"Hotkey", L"Configured hotkey could not be registered for action " +
                                    std::to_wstring(index));
  }
}

void HotkeyController::UnregisterAll() { manager_.UnregisterAll(); }

ui::HotkeyUpdateResult HotkeyController::Replace(
    settings::HotkeyAction action, settings::HotkeyBinding binding,
    const std::function<void(settings::HotkeyAction, bool)>& status) {
  const size_t index = static_cast<size_t>(action);
  if (index >= settings_.Get().hotkeys.size()) return ui::HotkeyUpdateResult::kInvalid;
  if (binding.virtual_key == 0) binding.modifiers = 0;
  if (binding.virtual_key == VK_CONTROL || binding.virtual_key == VK_SHIFT ||
      binding.virtual_key == VK_MENU || binding.virtual_key == VK_LWIN ||
      binding.virtual_key == VK_RWIN)
    return ui::HotkeyUpdateResult::kInvalid;
  for (size_t other = 0; other < settings_.Get().hotkeys.size(); ++other) {
    if (other != index && binding.virtual_key != 0 &&
        settings_.Get().hotkeys[other].virtual_key == binding.virtual_key &&
        settings_.Get().hotkeys[other].modifiers == binding.modifiers)
      return ui::HotkeyUpdateResult::kDuplicate;
  }

  const settings::HotkeyBinding previous = settings_.Get().hotkeys[index];
  const auto registration = manager_.Replace(action, previous, binding);
  if (registration == platform::windows::HotkeyRegistrationResult::kInvalid)
    return ui::HotkeyUpdateResult::kInvalid;
  if (registration == platform::windows::HotkeyRegistrationResult::kUnavailable) {
    status(action, previous.virtual_key == 0 || manager_.IsRegistered(action));
    return ui::HotkeyUpdateResult::kUnavailable;
  }

  auto proposed = settings_.Get();
  proposed.hotkeys[index] = binding;
  if (!settings_.Update(std::move(proposed))) {
    const auto rollback = manager_.Replace(action, binding, previous);
    status(action, previous.virtual_key == 0 ||
                       rollback == platform::windows::HotkeyRegistrationResult::kSuccess);
    return ui::HotkeyUpdateResult::kSaveFailed;
  }
  return ui::HotkeyUpdateResult::kSuccess;
}

}  // namespace minimize::features
