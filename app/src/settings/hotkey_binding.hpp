#pragma once

#include <cstddef>
#include <cstdint>

namespace genie::settings {

enum class HotkeyAction : std::size_t {
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

}  // namespace genie::settings
