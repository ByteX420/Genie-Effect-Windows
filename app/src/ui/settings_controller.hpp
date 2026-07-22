#pragma once

#include "ui/settings_actions.hpp"
#include "ui/settings_view_model.hpp"

namespace minimize::ui {

class SettingsController final {
public:
  explicit SettingsController(SettingsActions& actions) : actions_(actions) {}

  [[nodiscard]] SettingsActions& actions() const { return actions_; }
  [[nodiscard]] SettingsViewModel& view_model() { return view_model_; }
  [[nodiscard]] const SettingsViewModel& view_model() const { return view_model_; }

private:
  SettingsActions& actions_;
  SettingsViewModel view_model_;
};

}  // namespace minimize::ui
