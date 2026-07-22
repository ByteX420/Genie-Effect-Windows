#pragma once

#include <optional>

namespace minimize::platform {

struct PowerStatus {
  bool on_battery = false;
  bool battery_saver_active = false;
};

[[nodiscard]] std::optional<PowerStatus> QueryPowerStatus();

}  // namespace minimize::platform
