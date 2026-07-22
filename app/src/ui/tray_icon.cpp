#include "pch.hpp"

#include "ui/tray_icon.hpp"

#include <shellapi.h>

#include "core/logger.hpp"
#include "ui/settings_view_model.hpp"

namespace minimize::ui {
namespace {

constexpr UINT kIconId = 1;
constexpr UINT kToggleEnabled = 2999;
constexpr UINT kShowSettings = 3000;
constexpr UINT kRepairWindows = 3001;
constexpr UINT kExit = 3002;
constexpr UINT kPauseTenMinutes = 3004;
constexpr UINT kPauseOneHour = 3005;
constexpr UINT kPauseUntilRestart = 3006;
constexpr UINT kResume = 3007;

}  // namespace

void TrayIcon::Initialize() {
  taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
}

const wchar_t* TrayIcon::Tooltip(const SettingsViewModel& view_model) {
  if (view_model.temporarily_paused) {
    return view_model.paused_until_restart ? L"Minimize Effect \u2014 Paused until restart"
                                           : L"Minimize Effect \u2014 Paused temporarily";
  }
  return view_model.enabled ? L"Minimize Effect \u2014 Enabled" : L"Minimize Effect \u2014 Paused";
}

bool TrayIcon::Add(HWND owner, const SettingsViewModel& view_model) {
  if (owner == nullptr || IsWindowVisible(owner)) {
    if (owner != nullptr) KillTimer(owner, kRetryTimerId);
    return false;
  }
  if (added_) return true;
  NOTIFYICONDATAW icon{};
  icon.cbSize = sizeof(icon);
  icon.hWnd = owner;
  icon.uID = kIconId;
  icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  icon.uCallbackMessage = kCallbackMessage;
  icon.hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
  wcscpy_s(icon.szTip, Tooltip(view_model));
  added_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
  if (added_) {
    KillTimer(owner, kRetryTimerId);
  } else {
    SetTimer(owner, kRetryTimerId, 1000, nullptr);
    core::LogDebug(L"Tray", L"Tray icon add failed; retry scheduled");
  }
  return added_;
}

void TrayIcon::Remove(HWND owner) {
  if (owner == nullptr) return;
  KillTimer(owner, kRetryTimerId);
  if (!added_) return;
  NOTIFYICONDATAW icon{};
  icon.cbSize = sizeof(icon);
  icon.hWnd = owner;
  icon.uID = kIconId;
  Shell_NotifyIconW(NIM_DELETE, &icon);
  added_ = false;
}

void TrayIcon::UpdateTooltip(HWND owner, const SettingsViewModel& view_model) {
  if (owner == nullptr || !added_) return;
  NOTIFYICONDATAW icon{};
  icon.cbSize = sizeof(icon);
  icon.hWnd = owner;
  icon.uID = kIconId;
  icon.uFlags = NIF_TIP;
  wcscpy_s(icon.szTip, Tooltip(view_model));
  Shell_NotifyIconW(NIM_MODIFY, &icon);
}

bool TrayIcon::IsTaskbarCreatedMessage(UINT message) const {
  return taskbar_created_message_ != 0 && message == taskbar_created_message_;
}

void TrayIcon::OnTaskbarCreated() { added_ = false; }

TrayCommand TrayIcon::HandleCallback(HWND owner, LPARAM parameter,
                                     const SettingsViewModel& view_model) const {
  if (parameter == WM_LBUTTONUP || parameter == WM_LBUTTONDBLCLK) {
    return TrayCommand::kShowSettings;
  }
  if (parameter != WM_RBUTTONUP) return TrayCommand::kNone;
  HMENU menu = CreatePopupMenu();
  if (menu == nullptr) return TrayCommand::kNone;
  AppendMenuW(menu, MF_STRING | (view_model.enabled ? MF_CHECKED : MF_UNCHECKED), kToggleEnabled,
              L"Minimize Effect Enabled");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  if (view_model.temporarily_paused) {
    AppendMenuW(menu, MF_STRING, kResume, L"Resume Minimize Effect");
  } else {
    const UINT pause_flags = view_model.enabled ? MF_STRING : MF_STRING | MF_GRAYED;
    AppendMenuW(menu, pause_flags, kPauseTenMinutes, L"Pause for 10 minutes");
    AppendMenuW(menu, pause_flags, kPauseOneHour, L"Pause for 1 hour");
    AppendMenuW(menu, pause_flags, kPauseUntilRestart, L"Pause until next restart");
  }
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kShowSettings, L"Settings");
  AppendMenuW(menu, MF_STRING, kRepairWindows, L"Repair Windows");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kExit, L"Exit");
  POINT cursor{};
  GetCursorPos(&cursor);
  SetForegroundWindow(owner);
  const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                       cursor.x, cursor.y, 0, owner, nullptr);
  DestroyMenu(menu);
  switch (selected) {
    case kToggleEnabled:
      return TrayCommand::kToggleEnabled;
    case kShowSettings:
      return TrayCommand::kShowSettings;
    case kRepairWindows:
      return TrayCommand::kRepairWindows;
    case kExit:
      return TrayCommand::kExit;
    case kPauseTenMinutes:
      return TrayCommand::kPauseTenMinutes;
    case kPauseOneHour:
      return TrayCommand::kPauseOneHour;
    case kPauseUntilRestart:
      return TrayCommand::kPauseUntilRestart;
    case kResume:
      return TrayCommand::kResume;
    default:
      return TrayCommand::kNone;
  }
}

}  // namespace minimize::ui
