#pragma once

namespace genie::ui {

class SettingsWindow;

class SettingsShell final {
public:
  static void Render(SettingsWindow& window);
};

}  // namespace genie::ui
