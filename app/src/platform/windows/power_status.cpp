#include "pch.hpp"

#include "platform/windows/power_status.hpp"

#include <windows.h>

namespace genie::platform {

std::optional<PowerStatus> QueryPowerStatus() {
  SYSTEM_POWER_STATUS status{};
  if (!GetSystemPowerStatus(&status)) return std::nullopt;
  return PowerStatus{
      .on_battery = status.ACLineStatus == 0,
      .battery_saver_active = status.SystemStatusFlag != 0,
  };
}

}  // namespace genie::platform
