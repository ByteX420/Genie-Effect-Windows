#pragma once

namespace minimize::ui {

class SettingsWindow;

class UpdatePresenter final {
public:
  static void Render(SettingsWindow& window);

private:
  static void DrawUpdateWorkspace(SettingsWindow& window);
};

}  // namespace minimize::ui
