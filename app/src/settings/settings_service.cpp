#include "pch.hpp"

#include "settings/settings_service.hpp"

#include <utility>

#include "settings/settings_validator.hpp"

namespace minimize::settings {

SettingsService::SettingsService(SettingsRepository repository)
    : repository_(std::move(repository)) {}

bool SettingsService::Load() {
  settings_ = SettingsValidator::Normalize(repository_.Load());
  return true;
}

bool SettingsService::Update(AppSettings proposed) {
  proposed = SettingsValidator::Normalize(std::move(proposed));
  if (!repository_.Save(proposed)) return false;
  settings_ = std::move(proposed);
  return true;
}

void SettingsService::Preview(AppSettings proposed) {
  settings_ = SettingsValidator::Normalize(std::move(proposed));
}

}  // namespace minimize::settings
