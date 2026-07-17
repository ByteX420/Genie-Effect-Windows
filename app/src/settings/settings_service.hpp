#pragma once

#include "settings/settings_repository.hpp"

namespace genie::settings {

class SettingsService final {
public:
  explicit SettingsService(SettingsRepository repository = {});

  [[nodiscard]] bool Load();
  [[nodiscard]] const AppSettings& Get() const { return settings_; }
  [[nodiscard]] bool Update(AppSettings proposed);
  void Preview(AppSettings proposed);

private:
  SettingsRepository repository_;
  AppSettings settings_;
};

}  // namespace genie::settings
