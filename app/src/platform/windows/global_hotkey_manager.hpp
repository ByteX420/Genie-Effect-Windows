#pragma once

#include <array>
#include <windows.h>

#include "settings/hotkey_binding.hpp"

namespace genie::platform::windows {

enum class HotkeyRegistrationResult {
  kSuccess,
  kInvalid,
  kDuplicate,
  kUnavailable,
};

class GlobalHotkeyManager final {
public:
  GlobalHotkeyManager() = default;
  ~GlobalHotkeyManager();

  GlobalHotkeyManager(const GlobalHotkeyManager&) = delete;
  GlobalHotkeyManager& operator=(const GlobalHotkeyManager&) = delete;

  void SetWindow(HWND window);
  void UnregisterAll();
  [[nodiscard]] std::array<bool, static_cast<std::size_t>(settings::HotkeyAction::kCount)>
  RegisterAll(const std::array<settings::HotkeyBinding,
                               static_cast<std::size_t>(settings::HotkeyAction::kCount)>& bindings);
  [[nodiscard]] HotkeyRegistrationResult Replace(settings::HotkeyAction action,
                                                 settings::HotkeyBinding previous,
                                                 settings::HotkeyBinding replacement);
  [[nodiscard]] bool Restore(settings::HotkeyAction action, settings::HotkeyBinding binding);
  [[nodiscard]] bool IsRegistered(settings::HotkeyAction action) const;

private:
  [[nodiscard]] static bool IsValid(settings::HotkeyBinding binding);
  [[nodiscard]] static int Identifier(settings::HotkeyAction action);
  [[nodiscard]] bool Register(settings::HotkeyAction action, settings::HotkeyBinding binding);

  HWND window_ = nullptr;
  std::array<bool, static_cast<std::size_t>(settings::HotkeyAction::kCount)> registered_{};
};

}  // namespace genie::platform::windows
