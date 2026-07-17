#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "settings/app_settings.hpp"

namespace genie::settings {

class SettingsRepository final {
public:
  [[nodiscard]] static std::wstring Path();
  [[nodiscard]] AppSettings Load() const;
  [[nodiscard]] bool Save(const AppSettings& settings) const;
};
}  // namespace genie::settings
