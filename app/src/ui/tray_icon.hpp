#pragma once

#include <string_view>
#include <windows.h>

namespace minimize::ui {

class SettingsViewModel;

enum class TrayCommand {
  kNone,
  kShowSettings,
  kToggleEnabled,
  kResume,
  kPauseTenMinutes,
  kPauseOneHour,
  kPauseUntilRestart,
  kRepairWindows,
  kExit,
};

class TrayIcon final {
public:
  static constexpr UINT kCallbackMessage = WM_APP + 100;
  static constexpr UINT_PTR kRetryTimerId = 1;

  void Initialize();
  [[nodiscard]] bool Add(HWND owner, const SettingsViewModel& view_model);
  void Remove(HWND owner);
  void UpdateTooltip(HWND owner, const SettingsViewModel& view_model);
  void ShowUpdateAvailable(HWND owner, std::wstring_view version);
  [[nodiscard]] bool IsTaskbarCreatedMessage(UINT message) const;
  void OnTaskbarCreated();
  [[nodiscard]] TrayCommand HandleCallback(HWND owner, LPARAM parameter,
                                           const SettingsViewModel& view_model) const;
  [[nodiscard]] bool added() const { return added_; }

private:
  [[nodiscard]] static const wchar_t* Tooltip(const SettingsViewModel& view_model);

  bool added_ = false;
  UINT taskbar_created_message_ = 0;
};

}  // namespace minimize::ui
