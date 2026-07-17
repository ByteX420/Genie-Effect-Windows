#pragma once

#include <cstdint>
#include <string_view>

#include "settings/app_settings.hpp"

namespace genie::features {

struct RenderingPressure {
  int active_animations = 0;
  float last_capture_duration_ms = 0.0f;
  unsigned int recent_missed_frames = 0;
  unsigned int recent_device_failures = 0;
  bool renderer_recovering = false;
};

class EffectPolicy final {
public:
  void Configure(const settings::AppSettings& settings);

  void SetEnabled(bool enabled);
  [[nodiscard]] bool SetFullscreenSuppressed(bool suppressed);
  [[nodiscard]] bool SetPowerState(bool on_battery, bool battery_saver_active);

  [[nodiscard]] bool IsActive(bool temporarily_paused) const;
  [[nodiscard]] bool IsExcluded(std::string_view executable_name) const;
  [[nodiscard]] int SelectMeshSegmentCount(int width, int height,
                                           const RenderingPressure& pressure) const;
  [[nodiscard]] bool on_battery() const { return on_battery_; }
  [[nodiscard]] bool battery_saver_active() const { return battery_saver_active_; }

private:
  settings::AppSettings settings_;
  bool fullscreen_suppressed_ = false;
  bool on_battery_ = false;
  bool battery_saver_active_ = false;
};

}  // namespace genie::features
