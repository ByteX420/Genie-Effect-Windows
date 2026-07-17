#pragma once

#include <optional>

namespace genie::platform {

struct PowerStatus {
  bool on_battery = false;
  bool battery_saver_active = false;
};

[[nodiscard]] std::optional<PowerStatus> QueryPowerStatus();

}  // namespace genie::platform
