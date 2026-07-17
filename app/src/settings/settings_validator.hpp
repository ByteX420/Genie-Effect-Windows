#pragma once

#include "settings/app_settings.hpp"

namespace genie::settings {

class SettingsValidator final {
public:
  [[nodiscard]] static AppSettings Normalize(AppSettings settings);
  [[nodiscard]] static bool IsValid(const AppSettings& settings);
};

}  // namespace genie::settings
