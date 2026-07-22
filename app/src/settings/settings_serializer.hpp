#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "settings/app_settings.hpp"

namespace minimize::settings {

class SettingsSerializer final {
public:
  [[nodiscard]] static std::optional<AppSettings> Deserialize(std::string_view json);
  [[nodiscard]] static std::string Serialize(const AppSettings& settings);
};

}  // namespace minimize::settings
