#include "pch.hpp"

#include "app/settings_window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <dwmapi.h>
#include <format>
#include <shellapi.h>
#include <tuple>
#include <unordered_set>

#include "animation/easing.hpp"
#include "app/resource.hpp"
#include "app/settings_store.hpp"
#include "app/settings_ui_theme.hpp"
#include "app/settings_ui_widgets.hpp"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "common/debug_log.hpp"
#include "imgui.h"
#include "menu/motion/motion_context.hpp"
#include "menu/theme.hpp"
#include "misc/freetype/imgui_freetype.h"
#include "platform/window_util.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message,
                                                             WPARAM w_param, LPARAM l_param);

namespace genie::app {
namespace {

constexpr wchar_t kSettingsWindowClass[] = L"GenieEffectImGuiSettings";
constexpr wchar_t kPreviewWindowClass[] = L"GenieEffectAnimationPreview";
constexpr int kWindowWidth = static_cast<int>(settings_ui::Metrics::kWindowWidth);
constexpr int kWindowHeight = static_cast<int>(settings_ui::Metrics::kWindowHeight);
constexpr int kMinimumWindowWidth = kWindowWidth;
constexpr int kMinimumWindowHeight = kWindowHeight;
constexpr float kHeaderHeight = settings_ui::Metrics::kTitlebarHeight;
constexpr float kMinimumAnimationDurationSeconds = 0.10f;
constexpr float kMaximumAnimationDurationSeconds = 2.00f;
constexpr UINT kTrayMessage = WM_APP + 100;
constexpr UINT kShowSettingsMessage = WM_APP + 101;
constexpr UINT kTrayIconId = 1;
constexpr UINT_PTR kTrayRetryTimerId = 1;
constexpr UINT kTrayToggleEnabled = 2999;
constexpr UINT kTrayShowSettings = 3000;
constexpr UINT kTrayRepairWindows = 3001;
constexpr UINT kTrayExit = 3002;
constexpr UINT kTrayPauseTenMinutes = 3004;
constexpr UINT kTrayPauseOneHour = 3005;
constexpr UINT kTrayPauseUntilRestart = 3006;
constexpr UINT kTrayResume = 3007;
constexpr int kHotkeyBaseId = 4100;
constexpr ImU32 kPrimaryTextColor = settings_ui::kText;
constexpr ImU32 kSecondaryTextColor = settings_ui::kMutedText;
// Inter type scale (SIL OFL 1.1). Integer px sizes only — half-pixels blur glyphs.
constexpr float kSmallFontSize = 13.0f;  // helpers, values, captions
constexpr float kBodyFontSize = 15.0f;   // labels, buttons, nav
constexpr float kTitleFontSize = 22.0f;  // page titles
constexpr float kPageTitleTextSize = 22.0f;
constexpr float kPageSubtitleTextSize = 13.0f;
constexpr float kSectionTitleTextSize = 15.0f;  // hero row titles (semibold)
constexpr float kLabelTextSize = 15.0f;
constexpr float kValueTextSize = 13.0f;
constexpr float kHelperTextSize = 13.0f;
constexpr float kCaptionTextSize = 12.0f;  // section captions above groups
constexpr DWORD kInitialDeviceRecoveryDelayMs = 250;
constexpr DWORD kMaximumDeviceRecoveryDelayMs = 4000;

bool SystemUiAnimationsEnabled() {
  BOOL enabled = TRUE;
  return SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0) != FALSE &&
         enabled != FALSE;
}

bool SystemTransparencyEnabled() {
  DWORD enabled = 1;
  DWORD size = sizeof(enabled);
  const LSTATUS result = RegGetValueW(
      HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &enabled, &size);
  return result != ERROR_SUCCESS || enabled != 0;
}

using settings_ui::Combo;
using settings_ui::CompactButton;
using settings_ui::DelayedTooltip;
using settings_ui::SegmentSelector;
using settings_ui::Slider;
using settings_ui::Toggle;
using settings_ui::WithAlpha;

std::string HotkeyText(const HotkeyBinding& binding) {
  if (binding.virtual_key == 0) return "Disabled";
  std::string text;
  if ((binding.modifiers & MOD_CONTROL) != 0) text += "Ctrl + ";
  if ((binding.modifiers & MOD_ALT) != 0) text += "Alt + ";
  if ((binding.modifiers & MOD_SHIFT) != 0) text += "Shift + ";
  if ((binding.modifiers & MOD_WIN) != 0) text += "Win + ";
  if (binding.virtual_key >= 'A' && binding.virtual_key <= 'Z') {
    text.push_back(static_cast<char>(binding.virtual_key));
  } else if (binding.virtual_key >= '0' && binding.virtual_key <= '9') {
    text.push_back(static_cast<char>(binding.virtual_key));
  } else {
    const UINT scan_code = MapVirtualKeyW(binding.virtual_key, MAPVK_VK_TO_VSC);
    wchar_t name[64]{};
    if (GetKeyNameTextW(static_cast<LONG>(scan_code << 16), name, 64) > 0) {
      const int required = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
      if (required > 1) {
        std::string utf8(static_cast<size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8.data(), required, nullptr, nullptr);
        utf8.pop_back();
        text += utf8;
      }
    }
  }
  return text;
}

const char* HotkeyResultText(HotkeyUpdateResult result) {
  switch (result) {
    case HotkeyUpdateResult::kSuccess:
      return "Hotkey updated";
    case HotkeyUpdateResult::kInvalid:
      return "Choose a valid key combination";
    case HotkeyUpdateResult::kDuplicate:
      return "That combination is already used by Genie Effect";
    case HotkeyUpdateResult::kUnavailable:
      return "Hotkey unavailable; another application may use it";
    case HotkeyUpdateResult::kSaveFailed:
      return "Could not save the hotkey setting";
  }
  return "Hotkey could not be updated";
}

}  // namespace

SettingsWindow::~SettingsWindow() { Shutdown(); }

bool SettingsWindow::ActivateExistingInstance(DWORD timeout_ms) {
  const ULONGLONG deadline = GetTickCount64() + timeout_ms;
  do {
    const HWND window = FindWindowW(kSettingsWindowClass, nullptr);
    if (window != nullptr) {
      DWORD_PTR ignored = 0;
      return SendMessageTimeoutW(window, kShowSettingsMessage, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK,
                                 1000, &ignored) != 0;
    }
    Sleep(50);
  } while (GetTickCount64() < deadline);
  return false;
}

bool SettingsWindow::Initialize(
    HINSTANCE instance, ToggleCallback toggle_callback, SpeedCallback speed_callback,
    LinkCallback link_callback, FullscreenBehaviorCallback fullscreen_behavior_callback,
    BatterySaverCallback battery_saver_callback, EasingCallback easing_callback,
    CustomBezierCallback custom_bezier_callback, AnimationStyleCallback animation_style_callback,
    QualityModeCallback quality_mode_callback, StrengthCallback strength_callback,
    FadeCallback fade_callback,
    TargetIndicatorCallback target_indicator_callback, CloseBehaviorCallback close_behavior_callback,
    StartupCallback startup_callback, ExclusionCallback exclusion_callback,
    PauseCallback pause_callback, HotkeyUpdateCallback hotkey_update_callback,
    HotkeyActionCallback hotkey_action_callback, DiagnosticsCallback diagnostics_callback,
    DiagnosticsActionCallback diagnostics_action_callback, HealCallback heal_callback,
    ExitCallback exit_callback) {
  toggle_callback_ = std::move(toggle_callback);
  speed_callback_ = std::move(speed_callback);
  link_callback_ = std::move(link_callback);
  fullscreen_behavior_callback_ = std::move(fullscreen_behavior_callback);
  battery_saver_callback_ = std::move(battery_saver_callback);
  easing_callback_ = std::move(easing_callback);
  custom_bezier_callback_ = std::move(custom_bezier_callback);
  animation_style_callback_ = std::move(animation_style_callback);
  quality_mode_callback_ = std::move(quality_mode_callback);
  strength_callback_ = std::move(strength_callback);
  fade_callback_ = std::move(fade_callback);
  target_indicator_callback_ = std::move(target_indicator_callback);
  close_behavior_callback_ = std::move(close_behavior_callback);
  startup_callback_ = std::move(startup_callback);
  exclusion_callback_ = std::move(exclusion_callback);
  pause_callback_ = std::move(pause_callback);
  hotkey_update_callback_ = std::move(hotkey_update_callback);
  hotkey_action_callback_ = std::move(hotkey_action_callback);
  diagnostics_callback_ = std::move(diagnostics_callback);
  diagnostics_action_callback_ = std::move(diagnostics_action_callback);
  heal_callback_ = std::move(heal_callback);
  exit_callback_ = std::move(exit_callback);
  if (!CreateRenderWindow(instance) || !CreateDeviceResources()) return false;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  imgui_context_ready_ = true;
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  RebuildFonts(GetDpiForWindow(hwnd_));
  ApplyStyle();
  if (!ImGui_ImplWin32_Init(hwnd_)) return false;
  imgui_win32_ready_ = true;
  if (!ImGui_ImplDX11_Init(device_.Get(), context_.Get())) return false;
  imgui_dx11_ready_ = true;
  imgui_ready_ = true;
#ifdef _DEBUG
  device_recovery_test_pending_ = GenieEnvFlagEnabled(L"GENIE_TEST_DEVICE_RECOVERY");
#endif
  taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
  UpdateReducedMotion();
  return true;
}

bool SettingsWindow::AddTrayIcon() {
  if (hwnd_ == nullptr || IsWindowVisible(hwnd_)) {
    if (hwnd_ != nullptr) KillTimer(hwnd_, kTrayRetryTimerId);
    return false;
  }
  if (tray_icon_added_) return true;

  NOTIFYICONDATAW tray_icon{};
  tray_icon.cbSize = sizeof(tray_icon);
  tray_icon.hWnd = hwnd_;
  tray_icon.uID = kTrayIconId;
  tray_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  tray_icon.uCallbackMessage = kTrayMessage;
  tray_icon.hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
  const wchar_t* tooltip = L"Genie Effect \u2014 Enabled";
  if (temporarily_paused_) {
    tooltip = paused_until_restart_ ? L"Genie Effect \u2014 Paused until restart"
                                    : L"Genie Effect \u2014 Paused temporarily";
  } else if (!is_enabled_) {
    tooltip = L"Genie Effect \u2014 Paused";
  }
  wcscpy_s(tray_icon.szTip, tooltip);
  tray_icon_added_ = Shell_NotifyIconW(NIM_ADD, &tray_icon) != FALSE;
  if (tray_icon_added_) {
    KillTimer(hwnd_, kTrayRetryTimerId);
  } else {
    SetTimer(hwnd_, kTrayRetryTimerId, 1000, nullptr);
    LogDebug(L"Settings", L"Tray icon add failed; retry scheduled");
  }
  return tray_icon_added_;
}

void SettingsWindow::RemoveTrayIcon() {
  if (hwnd_ == nullptr) return;

  KillTimer(hwnd_, kTrayRetryTimerId);
  if (!tray_icon_added_) return;

  NOTIFYICONDATAW tray_icon{};
  tray_icon.cbSize = sizeof(tray_icon);
  tray_icon.hWnd = hwnd_;
  tray_icon.uID = kTrayIconId;
  Shell_NotifyIconW(NIM_DELETE, &tray_icon);
  tray_icon_added_ = false;
}

void SettingsWindow::Shutdown() {
  FlushPendingSpeedSave();
  CloseAnimationPreview();
  RemoveTrayIcon();
  if (imgui_dx11_ready_) {
    ImGui_ImplDX11_Shutdown();
    imgui_dx11_ready_ = false;
  }
  if (imgui_win32_ready_) {
    ImGui_ImplWin32_Shutdown();
    imgui_win32_ready_ = false;
  }
  if (imgui_context_ready_) {
    ImGui::DestroyContext();
    imgui_context_ready_ = false;
    imgui_ready_ = false;
  }
  ReleaseDeviceResources();
  if (hwnd_ != nullptr) DestroyWindow(hwnd_);
  hwnd_ = nullptr;
}

struct EmbeddedResource {
  void* data = nullptr;
  int size = 0;
};

EmbeddedResource LoadEmbeddedResource(int resource_id) {
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  HRSRC resource =
      FindResourceW(instance, MAKEINTRESOURCEW(resource_id), reinterpret_cast<LPCWSTR>(RT_RCDATA));
  if (resource == nullptr) return {};
  HGLOBAL loaded = LoadResource(instance, resource);
  if (loaded == nullptr) return {};
  const DWORD size = SizeofResource(instance, resource);
  void* data = LockResource(loaded);
  if (data == nullptr || size == 0 || size > static_cast<DWORD>(INT_MAX)) return {};
  return {data, static_cast<int>(size)};
}

std::string LoadEmbeddedText(int resource_id) {
  const EmbeddedResource resource = LoadEmbeddedResource(resource_id);
  if (resource.data == nullptr) return {};
  const char* begin = static_cast<const char*>(resource.data);
  return std::string(begin, begin + resource.size);
}

LRESULT CALLBACK SettingsWindow::PreviewWindowProc(HWND hwnd, UINT message, WPARAM w_param,
                                                   LPARAM l_param) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* settings = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(hwnd, &paint);
      RECT client{};
      GetClientRect(hwnd, &client);
      HBRUSH background_brush = CreateSolidBrush(RGB(20, 20, 22));
      FillRect(dc, &client, background_brush);
      DeleteObject(background_brush);

      RECT accent = client;
      accent.bottom = accent.top + 3;
      HBRUSH accent_brush = CreateSolidBrush(RGB(232, 232, 236));
      FillRect(dc, &accent, accent_brush);
      DeleteObject(accent_brush);

      // GDI preview label: request Inter (falls back to a generic sans if not installed).
      const int font_height = -MulDiv(36, GetDeviceCaps(dc, LOGPIXELSY), 72);
      HFONT font = CreateFontW(font_height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Inter");
      HGDIOBJ old_font = SelectObject(dc, font);
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, RGB(242, 242, 244));
      DrawTextW(dc, L"Preview", -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, old_font);
      DeleteObject(font);
      EndPaint(hwnd, &paint);
      return 0;
    }
    case WM_LBUTTONDOWN: {
      if (settings == nullptr) return 0;
      POINT cursor{};
      RECT window_rect{};
      GetCursorPos(&cursor);
      GetWindowRect(hwnd, &window_rect);
      settings->preview_drag_offset_ =
          POINT{cursor.x - window_rect.left, cursor.y - window_rect.top};
      settings->preview_dragging_ = true;
      SetCapture(hwnd);
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (settings == nullptr || !settings->preview_dragging_ || (w_param & MK_LBUTTON) == 0) {
        return 0;
      }
      POINT cursor{};
      GetCursorPos(&cursor);
      SetWindowPos(hwnd, nullptr, cursor.x - settings->preview_drag_offset_.x,
                   cursor.y - settings->preview_drag_offset_.y, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      return 0;
    }
    case WM_LBUTTONUP:
      if (settings != nullptr) settings->preview_dragging_ = false;
      if (GetCapture() == hwnd) ReleaseCapture();
      return 0;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
      if (settings != nullptr) settings->preview_dragging_ = false;
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    default:
      return DefWindowProcW(hwnd, message, w_param, l_param);
  }
}

void SettingsWindow::StartAnimationPreview() {
  CloseAnimationPreview();

  const HINSTANCE instance = GetModuleHandleW(nullptr);
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = PreviewWindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  window_class.lpszClassName = kPreviewWindowClass;
  if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return;
  }

  HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfoW(monitor, &monitor_info)) return;
  const RECT& work = monitor_info.rcWork;
  const int width = std::min(720, static_cast<int>(work.right - work.left - 80));
  const int height = std::min(420, static_cast<int>(work.bottom - work.top - 80));
  const int x = work.left + (work.right - work.left - width) / 2;
  const int y = work.top + (work.bottom - work.top - height) / 2;

  preview_window_ = CreateWindowExW(WS_EX_APPWINDOW, kPreviewWindowClass, L"Genie Effect Preview",
                                    WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX, x, y, width, height,
                                    nullptr, nullptr, instance, this);
  if (preview_window_ == nullptr) return;

  preview_active_ = true;
  preview_phase_ = 0;
  preview_phase_started_ms_ = GetTickCount64();
  ShowWindow(preview_window_, SW_SHOWNORMAL);
  SetForegroundWindow(preview_window_);
  UpdateWindow(preview_window_);
}

void SettingsWindow::UpdateAnimationPreview() {
  if (!preview_active_) return;
  if (preview_window_ == nullptr || !IsWindow(preview_window_)) {
    preview_window_ = nullptr;
    preview_active_ = false;
    preview_phase_ = 0;
    return;
  }

  const ULONGLONG now = GetTickCount64();
  const ULONGLONG elapsed = now - preview_phase_started_ms_;
  if (preview_phase_ == 0 && elapsed >= 750) {
    ShowWindow(preview_window_, SW_MINIMIZE);
    preview_phase_ = 1;
    preview_phase_started_ms_ = now;
  } else if (preview_phase_ == 1 &&
             elapsed >= static_cast<ULONGLONG>(minimize_duration_seconds_ * 1000.0f) + 700) {
    ShowWindow(preview_window_, SW_RESTORE);
    SetForegroundWindow(preview_window_);
    preview_phase_ = 2;
    preview_phase_started_ms_ = now;
  } else if (preview_phase_ == 2 &&
             elapsed >= static_cast<ULONGLONG>(restore_duration_seconds_ * 1000.0f) + 850) {
    CloseAnimationPreview();
    if (hwnd_ != nullptr && IsWindowVisible(hwnd_)) SetForegroundWindow(hwnd_);
  }
}

void SettingsWindow::CloseAnimationPreview() {
  HWND window = preview_window_;
  preview_window_ = nullptr;
  preview_active_ = false;
  preview_phase_ = 0;
  preview_phase_started_ms_ = 0;
  preview_dragging_ = false;
  if (window != nullptr && IsWindow(window)) DestroyWindow(window);
}

void SettingsWindow::Show(bool show) {
  if (hwnd_ == nullptr) return;
  if (!show) FlushPendingSpeedSave();
  if (show) {
    WindowMotion::System().set(::ui::motion::MotionKey("window", "settings", "alpha"), 0.0f);
    WindowMotion::System().set(::ui::motion::MotionKey("window", "settings", "offset"),
                               ImVec2(0.0f, 6.0f));
    if (IsIconic(hwnd_)) {
      ShowWindow(hwnd_, SW_RESTORE);
    }
    POINT cursor_pos{};
    GetCursorPos(&cursor_pos);
    HMONITOR monitor = MonitorFromPoint(cursor_pos, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
      RECT rect{};
      GetWindowRect(hwnd_, &rect);
      const int w = rect.right - rect.left;
      const int h = rect.bottom - rect.top;
      const int x = info.rcWork.left + (info.rcWork.right - info.rcWork.left - w) / 2;
      const int y = info.rcWork.top + (info.rcWork.bottom - info.rcWork.top - h) / 2;
      SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ShowWindow(hwnd_, SW_SHOW);
    RemoveTrayIcon();
  } else {
    ShowWindow(hwnd_, SW_HIDE);
    if (!AddTrayIcon()) {
      // Never leave the settings inaccessible if Explorer rejects the icon.
      ShowWindow(hwnd_, SW_SHOW);
      ForceRender();
      SetForegroundWindow(hwnd_);
      return;
    }
  }
  if (!show) {
    render_requested_ = false;
    return;
  }

  shown_at_ms_ = GetTickCount64();
  ForceRender();
  SetForegroundWindow(hwnd_);
  Render();
}

void SettingsWindow::UpdateState(const AppSettings& settings) {
  const bool enabled_changed = is_enabled_ != settings.enabled;
  const bool changed =
      enabled_changed ||
      std::abs(minimize_duration_seconds_ - settings.minimize_duration) > 0.0001f ||
      std::abs(restore_duration_seconds_ - settings.restore_duration) > 0.0001f ||
      link_speeds_ != settings.link_speeds ||
      disable_animations_fullscreen_ != settings.disable_animations_fullscreen ||
      disable_effects_battery_saver_ != settings.disable_effects_battery_saver ||
      minimize_easing_ != settings.minimize_easing || restore_easing_ != settings.restore_easing ||
      minimize_custom_bezier_ != settings.minimize_custom_bezier ||
      restore_custom_bezier_ != settings.restore_custom_bezier ||
      animation_style_ != settings.animation_style ||
      quality_mode_ != settings.quality_mode ||
      std::abs(genie_strength_ - settings.genie_strength) > 0.0001f ||
      fade_strength_ != settings.fade_strength ||
      show_target_indicator_ != settings.show_target_indicator ||
      close_behavior_ != settings.close_behavior || run_at_startup_ != settings.run_at_startup ||
      start_minimized_ != settings.start_minimized ||
      excluded_applications_ != settings.excluded_applications || hotkeys_ != settings.hotkeys;
  is_enabled_ = settings.enabled;
  minimize_duration_seconds_ = settings.minimize_duration;
  restore_duration_seconds_ = settings.restore_duration;
  link_speeds_ = settings.link_speeds;
  disable_animations_fullscreen_ = settings.disable_animations_fullscreen;
  disable_effects_battery_saver_ = settings.disable_effects_battery_saver;
  minimize_easing_ = settings.minimize_easing;
  restore_easing_ = settings.restore_easing;
  minimize_custom_bezier_ = settings.minimize_custom_bezier;
  restore_custom_bezier_ = settings.restore_custom_bezier;
  animation_style_ = settings.animation_style;
  quality_mode_ = settings.quality_mode;
  genie_strength_ = settings.genie_strength;
  fade_strength_ = settings.fade_strength;
  show_target_indicator_ = settings.show_target_indicator;
  close_behavior_ = settings.close_behavior;
  run_at_startup_ = settings.run_at_startup;
  start_minimized_ = settings.start_minimized;
  excluded_applications_ = settings.excluded_applications;
  hotkeys_ = settings.hotkeys;
  if (enabled_changed) UpdateTrayTooltip();
  if (changed) ForceRender();
}

void SettingsWindow::UpdateTrayTooltip() {
  if (hwnd_ == nullptr || !tray_icon_added_) return;
  NOTIFYICONDATAW tray_icon{};
  tray_icon.cbSize = sizeof(tray_icon);
  tray_icon.hWnd = hwnd_;
  tray_icon.uID = kTrayIconId;
  tray_icon.uFlags = NIF_TIP;
  const wchar_t* tooltip = L"Genie Effect \u2014 Enabled";
  if (temporarily_paused_) {
    tooltip = paused_until_restart_ ? L"Genie Effect \u2014 Paused until restart"
                                    : L"Genie Effect \u2014 Paused temporarily";
  } else if (!is_enabled_) {
    tooltip = L"Genie Effect \u2014 Paused";
  }
  wcscpy_s(tray_icon.szTip, tooltip);
  Shell_NotifyIconW(NIM_MODIFY, &tray_icon);
}

void SettingsWindow::UpdatePauseState(bool paused, bool until_restart) {
  if (temporarily_paused_ == paused && paused_until_restart_ == until_restart) return;
  temporarily_paused_ = paused;
  paused_until_restart_ = paused && until_restart;
  UpdateTrayTooltip();
  ForceRender();
}

void SettingsWindow::SetHotkeyRegistrationStatus(HotkeyAction action, bool available) {
  const size_t index = static_cast<size_t>(action);
  if (index >= hotkey_available_.size() || hotkey_available_[index] == available) return;
  hotkey_available_[index] = available;
  ForceRender();
}

void SettingsWindow::ShowDiagnosticsPage() {
  selected_page_ = Page::kDiagnostics;
  ForceRender();
}

void SettingsWindow::FlushPendingSpeedSave() {
  const bool speeds_pending = minimize_slider_dirty_ || restore_slider_dirty_ ||
                              minimize_slider_active_ || restore_slider_active_;
  if (speeds_pending) {
    const bool saved = !speed_callback_ ||
                       speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, true);
    RecordSaveResult(saved);
    if (saved) {
      minimize_slider_dirty_ = false;
      restore_slider_dirty_ = false;
    }
  }
  const bool strength_pending = strength_slider_dirty_ || strength_slider_active_;
  if (strength_pending) {
    const bool saved = !strength_callback_ || strength_callback_(genie_strength_, true);
    RecordSaveResult(saved);
    if (saved) strength_slider_dirty_ = false;
  }
  minimize_slider_active_ = false;
  restore_slider_active_ = false;
  strength_slider_active_ = false;
}

void SettingsWindow::RecordSaveResult(bool saved) {
  // Restart toast enter animation from 0 each time so rapid saves re-trigger cleanly.
  auto& motion = WindowMotion::System();
  motion.set(::ui::motion::MotionKey("toast", "save", "show"), 0.0f);
  if (saved) {
    persistence_error_.clear();
    save_feedback_ = "Saved";
    save_feedback_until_ms_ = GetTickCount64() + 2200;
    save_feedback_error_ = false;
  } else {
    persistence_error_ = "Could not save settings. Check folder permissions or disk space.";
    save_feedback_ = "Could not save settings";
    save_feedback_until_ms_ = GetTickCount64() + 5500;
    save_feedback_error_ = true;
    LogDebug(L"Settings", L"Settings window could not persist the requested change");
  }
  ForceRender();
}

void SettingsWindow::HandleCloseRequest() {
  // close_behavior "tray" only hides the window. Everything else must exit the
  // process (same contract as 795f55b2 — close button exits the app).
  if (close_behavior_ == "tray") {
    Show(false);
    return;
  }

  // Hide immediately so the click feels responsive while shutdown runs.
  if (hwnd_ != nullptr && IsWindow(hwnd_)) {
    ShowWindow(hwnd_, SW_HIDE);
  }
  if (exit_callback_) {
    exit_callback_();
  } else {
    // Last resort if the host did not wire ExitCallback.
    PostQuitMessage(0);
  }
}

bool SettingsWindow::CreateRenderWindow(HINSTANCE instance) {
  WNDCLASSEXW window_class{sizeof(window_class)};
  window_class.style = CS_CLASSDC;
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  window_class.lpszClassName = kSettingsWindowClass;
  RegisterClassExW(&window_class);

  const DPI_AWARENESS_CONTEXT old_context =
      SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const UINT dpi = GetDpiForSystem();
  const int width = MulDiv(kWindowWidth, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
  const int height = MulDiv(kWindowHeight, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
  hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, kSettingsWindowClass, L"Genie Effect",
                          WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU, CW_USEDEFAULT,
                          CW_USEDEFAULT, width, height, nullptr, nullptr, instance, this);
  if (old_context != nullptr) SetThreadDpiAwarenessContext(old_context);
  if (hwnd_ == nullptr) return false;
  ChangeWindowMessageFilterEx(hwnd_, kShowSettingsMessage, MSGFLT_ALLOW, nullptr);

  current_dpi_ = GetDpiForWindow(hwnd_);
  ui_scale_ = static_cast<float>(current_dpi_) / USER_DEFAULT_SCREEN_DPI;
  ApplyWindowShape(width, height);
  const DWM_WINDOW_CORNER_PREFERENCE corner_preference = DWMWCP_ROUND;
  DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_preference,
                        sizeof(corner_preference));
  const MARGINS margins{-1};
  DwmExtendFrameIntoClientArea(hwnd_, &margins);
  const BOOL dark_mode = TRUE;
  DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark_mode,
                        sizeof(dark_mode));
  const DWORD mica_backdrop = 2;  // DWMSBT_MAINWINDOW, matching ImGuiBase.
  if (FAILED(DwmSetWindowAttribute(hwnd_, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &mica_backdrop,
                                   sizeof(mica_backdrop)))) {
    const DWORD legacy_mica = 1;
    DwmSetWindowAttribute(hwnd_, 1029 /* DWMWA_MICA_EFFECT */, &legacy_mica, sizeof(legacy_mica));
  }
  return true;
}

bool SettingsWindow::CreateDeviceResources() {
  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferCount = 2;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.OutputWindow = hwnd_;
  desc.SampleDesc.Count = 1;
  desc.Windowed = TRUE;
  // Match the ImGuiBase host: the legacy discard swap effect preserves the
  // transparent client surface that lets the DWM backdrop show through.
  desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  constexpr D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
  D3D_FEATURE_LEVEL level{};
  const HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                                       0, levels, 1, D3D11_SDK_VERSION, &desc,
                                                       &swap_chain_, &device_, &level, &context_);
  if (FAILED(result)) return false;
  return CreateRenderTarget();
}

bool SettingsWindow::CreateRenderTarget() {
  render_target_view_.Reset();
  if (device_ == nullptr || swap_chain_ == nullptr) return false;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
  if (FAILED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return false;
  return SUCCEEDED(
             device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_)) &&
         render_target_view_ != nullptr;
}

void SettingsWindow::CleanupRenderTarget() { render_target_view_.Reset(); }

bool SettingsWindow::IsDeviceLostError(HRESULT hr) {
  return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
         hr == DXGI_ERROR_DEVICE_HUNG || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

void SettingsWindow::ReleaseDeviceResources() {
  if (context_ != nullptr) {
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    context_->ClearState();
  }
  CleanupRenderTarget();
  swap_chain_.Reset();
  context_.Reset();
  device_.Reset();
}

bool SettingsWindow::TryRecoverDeviceResources() {
  if (!device_recovery_pending_) {
    return true;
  }
  const ULONGLONG now = GetTickCount64();
  if (now < next_device_recovery_ms_) {
    return false;
  }

  if (CreateDeviceResources() && ImGui_ImplDX11_Init(device_.Get(), context_.Get())) {
    imgui_dx11_ready_ = true;
    device_recovery_pending_ = false;
    device_recovery_delay_ms_ = kInitialDeviceRecoveryDelayMs;
    render_requested_ = true;
    return true;
  }

  ReleaseDeviceResources();
  next_device_recovery_ms_ = now + device_recovery_delay_ms_;
  device_recovery_delay_ms_ =
      std::min(device_recovery_delay_ms_ * 2, kMaximumDeviceRecoveryDelayMs);
  return false;
}

void SettingsWindow::HandleDeviceLost() {
  if (imgui_dx11_ready_) {
    ImGui_ImplDX11_Shutdown();
    imgui_dx11_ready_ = false;
  }
  ReleaseDeviceResources();
  device_recovery_pending_ = true;
  device_recovery_delay_ms_ = kInitialDeviceRecoveryDelayMs;
  next_device_recovery_ms_ = GetTickCount64();
  TryRecoverDeviceResources();
}

void SettingsWindow::ApplyStyle() { settings_ui::ApplyStyle(ui_scale_); }

void SettingsWindow::RebuildFonts(UINT dpi) {
  current_dpi_ = dpi == 0 ? USER_DEFAULT_SCREEN_DPI : dpi;
  ui_scale_ = static_cast<float>(current_dpi_) / USER_DEFAULT_SCREEN_DPI;
  ImGuiIO& io = ImGui::GetIO();
  if (imgui_dx11_ready_) ImGui_ImplDX11_InvalidateDeviceObjects();
  io.Fonts->Clear();

#ifdef IMGUI_ENABLE_FREETYPE
  // Native hinter + ForceAutoHint reads cleanly for Inter on screen UI sizes.
  io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;
#endif

  ImFontConfig config{};
#ifdef IMGUI_ENABLE_FREETYPE
  config.OversampleH = 1;
  config.OversampleV = 1;
  // Snap glyphs to the pixel grid — critical with bilinear atlas sampling.
  config.PixelSnapH = true;
#else
  config.OversampleH = 3;
  config.OversampleV = 2;
  config.PixelSnapH = true;
#endif
  // Embedded Inter only (SIL OFL 1.1) — no system UI fonts.
  // Weight rule:
  //   Regular  (font_small_/font_body_)   — labels, helpers, buttons, combos, idle nav, values
  //   SemiBold (font_medium_)             — hero/section titles, selected nav, product name
  //   Bold     (font_title_)              — page titles only
  config.FontDataOwnedByAtlas = false;

  // Bake at whole-pixel sizes only. Fractional raster sizes look permanently soft.
  const auto bake_size = [this](float logical) {
    return std::max(1.0f, std::round(logical * ui_scale_));
  };

  const EmbeddedResource regular_resource = LoadEmbeddedResource(IDR_UI_FONT_REGULAR);
  const EmbeddedResource semibold_resource = LoadEmbeddedResource(IDR_UI_FONT_SEMIBOLD);
  const EmbeddedResource bold_resource = LoadEmbeddedResource(IDR_UI_FONT_BOLD);
  const auto add_embedded = [&config](const EmbeddedResource& font, float size) -> ImFont* {
    return font.data == nullptr
               ? nullptr
               : ImGui::GetIO().Fonts->AddFontFromMemoryTTF(font.data, font.size, size, &config);
  };
  font_small_ = add_embedded(regular_resource, bake_size(kSmallFontSize));
  font_body_ = add_embedded(regular_resource, bake_size(kBodyFontSize));
  font_medium_ = add_embedded(semibold_resource, bake_size(kSectionTitleTextSize));
  font_title_ = add_embedded(bold_resource, bake_size(kTitleFontSize));
  if (font_title_ == nullptr)
    font_title_ = add_embedded(semibold_resource, bake_size(kTitleFontSize));

  // Last-resort: load from the app assets folder if RC embedding failed.
  if (font_body_ == nullptr || font_small_ == nullptr || font_medium_ == nullptr ||
      font_title_ == nullptr) {
    ImFontConfig file_config = config;
    file_config.FontDataOwnedByAtlas = true;
    const auto add_file = [&file_config](const char* path, float size) -> ImFont* {
      return ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size, &file_config);
    };
    if (font_small_ == nullptr)
      font_small_ = add_file("assets/fonts/Inter-Regular.ttf", bake_size(kSmallFontSize));
    if (font_body_ == nullptr)
      font_body_ = add_file("assets/fonts/Inter-Regular.ttf", bake_size(kBodyFontSize));
    if (font_medium_ == nullptr)
      font_medium_ = add_file("assets/fonts/Inter-SemiBold.ttf", bake_size(kSectionTitleTextSize));
    if (font_title_ == nullptr)
      font_title_ = add_file("assets/fonts/Inter-Bold.ttf", bake_size(kTitleFontSize));
    if (font_title_ == nullptr)
      font_title_ = add_file("assets/fonts/Inter-SemiBold.ttf", bake_size(kTitleFontSize));
  }

  if (font_body_ == nullptr) font_body_ = io.Fonts->AddFontDefault();
  if (font_small_ == nullptr) font_small_ = font_body_;
  if (font_medium_ == nullptr) font_medium_ = font_body_;
  if (font_title_ == nullptr) font_title_ = font_medium_;
  io.FontDefault = font_body_;
  if (imgui_dx11_ready_) ImGui_ImplDX11_CreateDeviceObjects();
}

void SettingsWindow::ApplyWindowShape(int width, int height) {
  (void)width;
  (void)height;
  SetWindowRgn(hwnd_, nullptr, TRUE);
}

void SettingsWindow::UpdateDpi(UINT dpi) {
  if (dpi == 0 || dpi == current_dpi_) return;
  RebuildFonts(dpi);
  ApplyStyle();
}

void SettingsWindow::UpdateReducedMotion() {
  const bool reduced = !SystemUiAnimationsEnabled();
  WindowMotion::SetReducedMotion(reduced);
}

void SettingsWindow::Render() {
  UpdateAnimationPreview();
#ifdef _DEBUG
  if (device_recovery_test_pending_) {
    device_recovery_test_pending_ = false;
    HandleDeviceLost();
  }
#endif
  if (device_recovery_pending_ && !TryRecoverDeviceResources()) return;
  if (!imgui_ready_ || !imgui_dx11_ready_ || !IsWindowVisible(hwnd_) ||
      render_target_view_ == nullptr)
    return;
  const ULONGLONG now_ms = GetTickCount64();
  const bool is_animating = (now_ms - shown_at_ms_ < 500);
  const bool is_active = (GetForegroundWindow() == hwnd_);
  const bool feedback_active = !save_feedback_.empty() && now_ms < save_feedback_until_ms_;
  if (!is_animating && !is_active && !preview_active_ && !feedback_active &&
      WindowMotion::System().stats().activeTracks == 0 && !render_requested_) {
    return;
  }
  render_requested_ = false;

  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  WindowMotion::BeginFrame(ImGui::GetIO().DeltaTime);
  RenderContents();
  ImGui::Render();
  // Transparent clear is required for DWM's native Mica backdrop. The UI
  // paints the opaque main pane itself and leaves the sidebar translucent.
  constexpr float clear_color[] = {0.0f, 0.0f, 0.0f, 0.0f};
  context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);
  context_->ClearRenderTargetView(render_target_view_.Get(), clear_color);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  const HRESULT present_result = swap_chain_->Present(1, 0);
  if (IsDeviceLostError(present_result)) {
    HandleDeviceLost();
  } else if (FAILED(present_result)) {
    render_requested_ = true;
  }
}

void SettingsWindow::ForceRender() { render_requested_ = true; }

bool SettingsWindow::WantsContinuousRendering() const {
  if (!imgui_ready_ || hwnd_ == nullptr || !IsWindowVisible(hwnd_)) {
    return false;
  }
  const bool appearance_active = GetTickCount64() - shown_at_ms_ < 500;
  return appearance_active || preview_active_ || GetForegroundWindow() == hwnd_ ||
         WindowMotion::System().stats().activeTracks > 0 || render_requested_;
}

void SettingsWindow::RenderContents() {
  const ImVec2 window_size = ImGui::GetIO().DisplaySize;
  const float scale = ui_scale_;
  const auto px = [scale](float value) { return value * scale; };
  settings_ui::MotionContext widget_motion{WindowMotion::System(), WindowMotion::Tokens()};
  float content_alpha =
      WindowMotion::System().value(::ui::motion::MotionKey("window", "settings", "alpha"), 1.0f,
                                   WindowMotion::Tokens().panelEnterFade, 0.0f);
  const ImVec2 window_offset = WindowMotion::System().vec2(
      ::ui::motion::MotionKey("window", "settings", "offset"), ImVec2(0.0f, 0.0f),
      WindowMotion::Tokens().panelEnterOffset, ImVec2(0.0f, 6.0f));
  float y_offset = px(window_offset.y);

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(window_size);
  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::Begin("GenieEffectRoot", nullptr, flags);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 window_origin = ImGui::GetWindowPos();
  const auto window_point = [&window_origin](float x, float y) {
    return ImVec2(window_origin.x + x, window_origin.y + y);
  };

  const float sidebar_width = px(settings_ui::Metrics::kSidebarWidth);

  // Shell: compact rail + solid content pane (outer radius matches DWM round corners).
  const float shell_round = px(settings_ui::Metrics::kWindowRounding);
  ImVec4 main_background = colors::main;
  main_background.w *= content_alpha;
  draw->AddRectFilled(window_point(sidebar_width, 0.0f), window_point(window_size.x, window_size.y),
                      ImGui::GetColorU32(main_background), shell_round,
                      ImDrawFlags_RoundCornersRight);
  ImVec4 sidebar_background = colors::sidebar;
  sidebar_background.w *= content_alpha;
  draw->AddRectFilled(window_origin, window_point(sidebar_width, window_size.y),
                      ImGui::GetColorU32(sidebar_background), shell_round,
                      ImDrawFlags_RoundCornersLeft);
  draw->AddLine(window_point(sidebar_width, 0.0f), window_point(sidebar_width, window_size.y),
                WithAlpha(settings_ui::kBorder, content_alpha * 0.45f));

  switch (settings_ui::DrawTrafficLights(widget_motion, window_origin, scale, content_alpha)) {
    case settings_ui::TrafficLightAction::kClose:
      HandleCloseRequest();
      break;
    case settings_ui::TrafficLightAction::kMinimize:
      ShowWindow(hwnd_, SW_MINIMIZE);
      break;
    case settings_ui::TrafficLightAction::kZoom:
      ShowWindow(hwnd_, IsZoomed(hwnd_) != FALSE ? SW_RESTORE : SW_MAXIMIZE);
      ForceRender();
      break;
    case settings_ui::TrafficLightAction::kNone:
      break;
  }

  // Brand under traffic lights — same left inset as nav.
  {
    const float brand_reveal =
        WindowMotion::System().value(::ui::motion::MotionKey("shell", "brand", "reveal"), 1.0f,
                                     WindowMotion::Tokens().panelEnterFade, 0.0f);
    const float brand_shift = (1.0f - brand_reveal) * px(6.0f);
    const ImVec2 brand = window_point(px(settings_ui::Metrics::kSidebarMargin),
                                      px(settings_ui::Metrics::kSidebarBrandY) + brand_shift);
    draw->AddText(font_small_, font_small_->FontSize,
                  ImVec2(std::floor(brand.x + 0.5f), std::floor(brand.y + 0.5f)),
                  WithAlpha(settings_ui::kMutedText, content_alpha * brand_reveal * 0.85f),
                  "GENIE");
  }

  struct PageEntry {
    Page page;
    const char* label;
    bool section_gap;
  };
  constexpr std::array pages = {
      PageEntry{Page::kGeneral, "Effect", false},
      PageEntry{Page::kAnimation, "Motion", false},
      PageEntry{Page::kApplications, "Apps", false},
      PageEntry{Page::kWindowsIntegration, "System", true},
      PageEntry{Page::kHotkeys, "Hotkeys", false},
      PageEntry{Page::kDiagnostics, "Repair", false},
      PageEntry{Page::kAbout, "About", false},
  };
  float navigation_y = px(settings_ui::Metrics::kNavigationY);
  const float nav_x = px(settings_ui::Metrics::kSidebarMargin);
  const float nav_w = px(settings_ui::Metrics::kSidebarContentWidth);
  const float nav_h = px(settings_ui::Metrics::kNavigationRowHeight);
  for (size_t index = 0; index < pages.size(); ++index) {
    const PageEntry& entry = pages[index];
    if (entry.section_gap) {
      // Quiet divider between primary and secondary nav groups.
      navigation_y += px(10.0f);
      const float div_y = navigation_y - px(5.0f);
      draw->AddLine(window_point(nav_x + px(4.0f), div_y),
                    window_point(nav_x + nav_w - px(4.0f), div_y),
                    WithAlpha(settings_ui::kSeparator, content_alpha * 0.55f));
    }
    const ImVec2 item_position = window_point(nav_x, navigation_y);
    const std::string id = std::format("##settings_page_{}", index);
    if (settings_ui::SidebarItem(widget_motion, id.c_str(), entry.label,
                                 selected_page_ == entry.page, item_position, ImVec2(nav_w, nav_h),
                                 font_body_, font_medium_, scale, content_alpha) &&
        selected_page_ != entry.page) {
      FlushPendingSpeedSave();
      selected_page_ = entry.page;
      reset_page_scroll_ = true;
      WindowMotion::System().set(::ui::motion::MotionKey("page", "content", "alpha"), 0.0f);
      WindowMotion::System().set(::ui::motion::MotionKey("page", "content", "offset"),
                                 ImVec2(0.0f, 14.0f));
    }
    navigation_y +=
        px(settings_ui::Metrics::kNavigationRowHeight + settings_ui::Metrics::kNavigationSpacing);
  }

  // Status chip: same left column as nav/ampel; bottom inset mirrors traffic-light top edge.
  {
    const char* status = temporarily_paused_ ? "Paused" : (is_enabled_ ? "On" : "Off");
    const ImU32 status_color = temporarily_paused_ ? IM_COL32(220, 170, 90, 255)
                               : is_enabled_       ? IM_COL32(120, 200, 140, 255)
                                                   : settings_ui::kMutedText;
    const float status_reveal =
        WindowMotion::System().value(::ui::motion::MotionKey("shell", "status", "reveal"), 1.0f,
                                     WindowMotion::Tokens().fadeMedium, 0.0f);
    ImFont* status_font = font_small_ ? font_small_ : ImGui::GetFont();
    const float status_sz = status_font->FontSize;
    const ImVec2 status_text_size = status_font->CalcTextSizeA(status_sz, FLT_MAX, 0.0f, status);
    const float chip_pad_x = px(8.0f);
    const float chip_pad_y = px(4.0f);
    const float chip_h = status_sz + chip_pad_y * 2.0f;
    const float chip_w = status_text_size.x + chip_pad_x * 2.0f;
    // Ampel top-edge inset ≈ kSidebarMargin; pin chip to the matching bottom inset.
    const float chip_y = window_size.y - px(settings_ui::Metrics::kSidebarStatusBottom) - chip_h +
                         (1.0f - status_reveal) * px(6.0f);
    const ImVec2 chip_min = window_point(nav_x, chip_y);
    const ImVec2 chip_max(chip_min.x + chip_w, chip_min.y + chip_h);
    const float chip_round = chip_h * 0.5f;
    const float a = content_alpha * status_reveal;
    draw->AddRectFilled(chip_min, chip_max, IM_COL32(255, 255, 255, static_cast<int>(12.0f * a)),
                        chip_round);
    draw->AddText(status_font, status_sz,
                  ImVec2(std::floor(chip_min.x + chip_pad_x + 0.5f),
                         settings_ui::CenteredTextTop(status_font, chip_min.y, chip_h)),
                  WithAlpha(status_color, a), status);
  }

  ImGui::SetCursorPos(ImVec2(sidebar_width, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  // Full remaining width — custom overlay scrollbar (not ImGui's edge strip) so the grab
  // stays inside the DWM-rounded shell and does not square off the window corners.
  const float page_width = window_size.x - sidebar_width;
  ImGui::BeginChild("##settings_page", ImVec2(page_width, window_size.y), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar);
  content_alpha *= WindowMotion::System().value(::ui::motion::MotionKey("page", "content", "alpha"),
                                                1.0f, WindowMotion::Tokens().tabFade, 0.0f);
  const ImVec2 page_offset = WindowMotion::System().vec2(
      ::ui::motion::MotionKey("page", "content", "offset"), ImVec2(0.0f, 0.0f),
      WindowMotion::Tokens().tabSlide, ImVec2(0.0f, 12.0f));
  y_offset += px(page_offset.y);
  if (reset_page_scroll_) {
    ImGui::SetScrollY(0.0f);
    reset_page_scroll_ = false;
  }
  draw = ImGui::GetWindowDrawList();
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 origin = ImGui::GetWindowPos();
  // Full child width so left/right page insets stay equal. Reserving ScrollbarSize
  // permanently made the right margin larger than the left (visible on Apps).
  // Scrollbar overlays the right edge over the page inset.
  const float layout_width = size.x;

  const char* page_scope = "general";
  switch (selected_page_) {
    case Page::kGeneral:
      page_scope = "general";
      break;
    case Page::kAnimation:
      page_scope = "motion";
      break;
    case Page::kApplications:
      page_scope = "apps";
      break;
    case Page::kWindowsIntegration:
      page_scope = "system";
      break;
    case Page::kHotkeys:
      page_scope = "hotkeys";
      break;
    case Page::kDiagnostics:
      page_scope = "repair";
      break;
    case Page::kAbout:
      page_scope = "about";
      break;
  }
  settings_ui::PageLayout layout(draw, origin, layout_width, scale, content_alpha,
                                 px(16.0f) + y_offset, &widget_motion, page_scope);
  const float toggle_w = px(settings_ui::Metrics::kToggleWidth);
  const float toggle_h = px(settings_ui::Metrics::kToggleHeight + 4.0f);
  const float btn_h = px(settings_ui::Metrics::kButtonHeight);
  const float combo_h = px(settings_ui::Metrics::kComboHeight);
  const float slider_h = px(settings_ui::Metrics::kSliderHeight);

  const auto selected_index = [](const auto& names, const std::string& value) {
    for (int index = 0; index < static_cast<int>(names.size()); ++index) {
      if (value == names[index]) return index;
    }
    return 0;
  };

  // ── Effect ──────────────────────────────────────────────────────────────
  if (selected_page_ == Page::kGeneral) {
    layout.Title(font_title_, kPageTitleTextSize, "Effect", font_small_, kPageSubtitleTextSize,
                 "Master switch and how the app behaves");

    layout.BeginGroup();
    layout.BeginRow(settings_ui::Metrics::kRowHeightHero);
    layout.ReserveControl(toggle_w);
    layout.RowTitle(font_medium_, kSectionTitleTextSize, "Genie animations", kPrimaryTextColor);
    layout.RowSubtitle(font_small_, kHelperTextSize, "Replace minimize and restore transitions",
                       kSecondaryTextColor);
    {
      const bool previous_enabled = is_enabled_;
      const ImVec2 cursor = layout.ControlCursor(toggle_w, toggle_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Toggle(widget_motion, "##animations_enabled", &is_enabled_, scale, content_alpha) &&
          toggle_callback_) {
        const bool saved = toggle_callback_(is_enabled_);
        if (!saved) is_enabled_ = previous_enabled;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();
    layout.EndGroup();

    layout.SectionCaption(font_small_, kCaptionTextSize, "WHEN CLOSING THIS WINDOW");
    layout.BeginGroup();
    layout.BeginStackRow(20.0f, settings_ui::Metrics::kSegmentHeight);
    layout.RowTitle(font_body_, kLabelTextSize, "Close action", kPrimaryTextColor);
    {
      constexpr std::array close_labels = {"Quit app", "Keep in tray"};
      int close_behavior_segment = close_behavior_ == "tray" ? 1 : 0;
      const float seg_w = layout.content_width();
      layout.SetCursor(layout.content_left(), layout.StackControlY());
      if (SegmentSelector(widget_motion, "##close_behavior", close_labels, &close_behavior_segment,
                          seg_w, font_body_, scale, content_alpha)) {
        const std::string previous_close_behavior = close_behavior_;
        close_behavior_ = close_behavior_segment == 1 ? "tray" : "exit";
        const bool saved = !close_behavior_callback_ || close_behavior_callback_(close_behavior_);
        if (!saved) close_behavior_ = previous_close_behavior;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();
    layout.EndGroup();

    layout.SectionCaption(font_small_, kCaptionTextSize, "STARTUP");
    layout.BeginGroup();
    layout.BeginRow(settings_ui::Metrics::kRowHeight);
    layout.ReserveControl(toggle_w);
    layout.RowTitle(font_body_, kLabelTextSize, "Launch at login", kPrimaryTextColor);
    {
      bool proposed = run_at_startup_;
      const ImVec2 cursor = layout.ControlCursor(toggle_w, toggle_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Toggle(widget_motion, "##run_at_startup", &proposed, scale, content_alpha)) {
        const bool saved = !startup_callback_ || startup_callback_(proposed, start_minimized_);
        if (saved) run_at_startup_ = proposed;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();
    layout.BeginRow(settings_ui::Metrics::kRowHeight);
    layout.ReserveControl(toggle_w);
    layout.RowTitle(font_body_, kLabelTextSize, "Start in tray", kPrimaryTextColor);
    {
      bool proposed = start_minimized_;
      const ImVec2 cursor = layout.ControlCursor(toggle_w, toggle_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Toggle(widget_motion, "##start_minimized", &proposed, scale, content_alpha)) {
        const bool saved = !startup_callback_ || startup_callback_(run_at_startup_, proposed);
        if (saved) start_minimized_ = proposed;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();
    layout.EndGroup();
  }

  // ── Motion ──────────────────────────────────────────────────────────────
  if (selected_page_ == Page::kAnimation) {
    const float action_w = px(92.0f);
    const float action_gap = px(8.0f);
    const float actions_total = action_w * 2.0f + action_gap;
    layout.Title(font_title_, kPageTitleTextSize, "Motion", font_small_, kPageSubtitleTextSize,
                 "Speed, curve and look of the genie", actions_total + px(8.0f));

    // Header actions aligned with the title band.
    {
      const float top = px(16.0f) + y_offset;
      layout.SetCursor(layout.group_right() - actions_total, top + px(2.0f));
      if (CompactButton(widget_motion, "##preview", preview_active_ ? "Running…" : "Preview",
                        ImVec2(action_w, btn_h), font_body_, scale, content_alpha,
                        preview_active_) &&
          !preview_active_) {
        StartAnimationPreview();
      }
      layout.SetCursor(layout.group_right() - action_w, top + px(2.0f));
      if (CompactButton(widget_motion, "##reset_speeds", "Reset", ImVec2(action_w, btn_h),
                        font_body_, scale, content_alpha)) {
        minimize_duration_seconds_ = kDefaultMinimizeDuration;
        restore_duration_seconds_ = kDefaultRestoreDuration;
        minimize_slider_dirty_ = false;
        restore_slider_dirty_ = false;
        if (speed_callback_) {
          RecordSaveResult(
              speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, true));
        }
      }
    }

    layout.SectionCaption(font_small_, kCaptionTextSize, "TIMING");
    layout.BeginGroup();

    layout.BeginStackRow(18.0f, settings_ui::Metrics::kButtonHeight);
    layout.RowTitle(font_body_, kLabelTextSize, "Preset", kPrimaryTextColor);
    {
      // Balanced matches app defaults (0.70 / 0.70) so a preset is selected out of the box.
      constexpr std::array presets = {
          std::tuple{"Snappy", 0.30f, 0.25f},
          std::tuple{"Balanced", 0.70f, 0.70f},
          std::tuple{"Smooth", 1.00f, 0.90f},
          std::tuple{"Film", 1.35f, 1.15f},
      };
      const float gap = px(8.0f);
      const float row_w = layout.content_width();
      const float btn_w = (row_w - gap * static_cast<float>(presets.size() - 1)) /
                          static_cast<float>(presets.size());
      float bx = layout.content_left();
      const float by = layout.StackControlY();
      for (size_t i = 0; i < presets.size(); ++i) {
        const auto& [label, minimize, restore] = presets[i];
        layout.SetCursor(bx, by);
        const std::string id = std::format("##preset_{}", i);
        const bool active = std::abs(minimize_duration_seconds_ - minimize) < 0.0001f &&
                            std::abs(restore_duration_seconds_ - restore) < 0.0001f;
        // Active preset uses SemiBold; idle uses Regular.
        if (CompactButton(widget_motion, id.c_str(), label, ImVec2(btn_w, btn_h),
                          active ? font_medium_ : font_body_, scale, content_alpha, active)) {
          minimize_duration_seconds_ = minimize;
          restore_duration_seconds_ = restore;
          minimize_slider_dirty_ = false;
          restore_slider_dirty_ = false;
          if (speed_callback_) {
            RecordSaveResult(
                speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, true));
          }
        }
        bx += btn_w + gap;
      }
    }
    layout.EndRow();

    const auto run_duration_slider = [&](const char* id, const char* title, float* duration,
                                         bool* active_flag, bool* dirty_flag, bool is_minimize) {
      layout.BeginRow(settings_ui::Metrics::kRowHeight);
      const float slider_w = layout.ControlMaxWidth(340.0f);
      layout.ReserveControl(slider_w);
      layout.RowTitle(font_body_, kLabelTextSize, title, kPrimaryTextColor);
      const ImVec2 cursor = layout.ControlCursor(slider_w, slider_h);
      layout.SetCursor(cursor.x, cursor.y);
      float updated = *duration;
      const bool active = Slider(widget_motion, id, "", &updated, kMinimumAnimationDurationSeconds,
                                 kMaximumAnimationDurationSeconds, slider_w, scale, content_alpha,
                                 font_small_, 0.01f);
      if (active && std::abs(updated - *duration) > 0.0001f) {
        float delta = updated - *duration;
        if (link_speeds_) {
          float* other = is_minimize ? &restore_duration_seconds_ : &minimize_duration_seconds_;
          delta = std::clamp(delta, kMinimumAnimationDurationSeconds - *other,
                             kMaximumAnimationDurationSeconds - *other);
          *other += delta;
        }
        *duration += delta;
        *dirty_flag = true;
        if (speed_callback_) {
          speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, false);
        }
      }
      if (*active_flag && !active && *dirty_flag) {
        bool saved = true;
        if (speed_callback_) {
          saved = speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, true);
        }
        RecordSaveResult(saved);
        if (saved) *dirty_flag = false;
      }
      *active_flag = active;
      layout.EndRow();
    };

    run_duration_slider("##min_duration", "Minimize", &minimize_duration_seconds_,
                        &minimize_slider_active_, &minimize_slider_dirty_, true);
    run_duration_slider("##restore_duration", "Restore", &restore_duration_seconds_,
                        &restore_slider_active_, &restore_slider_dirty_, false);

    layout.BeginRow(settings_ui::Metrics::kRowHeightTall);
    layout.ReserveControl(toggle_w);
    layout.RowTitle(font_body_, kLabelTextSize, "Link durations", kPrimaryTextColor);
    layout.RowSubtitle(font_small_, kHelperTextSize, "Move both sliders together",
                       kSecondaryTextColor);
    {
      bool proposed = link_speeds_;
      const ImVec2 cursor = layout.ControlCursor(toggle_w, toggle_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Toggle(widget_motion, "##link_speeds", &proposed, scale, content_alpha)) {
        const bool previous = link_speeds_;
        link_speeds_ = proposed;
        const bool saved = !link_callback_ || link_callback_(link_speeds_);
        if (!saved) link_speeds_ = previous;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();
    layout.EndGroup();

    layout.SectionCaption(font_small_, kCaptionTextSize, "STYLE");
    layout.BeginGroup();
    constexpr std::array easing_names = {
        "Linear", "Ease In", "Ease Out", "Ease In Out", "Cubic", "Back", "Elastic", "Custom",
    };
    constexpr std::array style_names = {
        "Gienie classic",
        "Gienie curvy",
        "Squash",
    };
    // Same preferred width as duration sliders so the right edge lines up across groups.
    const float combo_w = layout.ControlMaxWidth(340.0f);
    const float graph_h = px(168.0f);

    const auto combo_row = [&](const char* id, const char* title, int* index,
                               std::span<const char* const> items, auto on_change) {
      layout.BeginRow(settings_ui::Metrics::kRowHeight);
      layout.ReserveControl(combo_w);
      layout.RowTitle(font_body_, kLabelTextSize, title, kPrimaryTextColor);
      const ImVec2 cursor = layout.ControlCursor(combo_w, combo_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Combo(widget_motion, id, "", index, items, ImVec2(combo_w, combo_h), font_small_,
                font_body_, scale, content_alpha)) {
        on_change();
      }
      layout.EndRow();
    };

    const auto easing_block = [&](const char* combo_id, const char* graph_id, const char* title,
                                  std::string* easing_name, animation::CubicBezier* bezier,
                                  bool* bezier_dirty, bool is_minimize) {
      int easing_index = selected_index(easing_names, *easing_name);
      combo_row(combo_id, title, &easing_index, easing_names, [&] {
        const std::string previous = *easing_name;
        *easing_name = easing_names[easing_index];
        const bool saved =
            !easing_callback_ || easing_callback_(minimize_easing_, restore_easing_);
        if (!saved) *easing_name = previous;
        RecordSaveResult(saved);
      });
      if (*easing_name != "Custom") return;

      // Full-width stack under the combo: editable cubic-bezier graph.
      layout.BeginStackRow(0.0f, 168.0f + 8.0f);
      {
        const float graph_w = layout.content_width();
        const ImVec2 cursor = layout.ToScreen(layout.content_left(), layout.StackControlY());
        // SetCursor expects layout-local coords (ToScreen already applied origin/scroll).
        layout.SetCursor(layout.content_left(), layout.StackControlY());
        (void)cursor;
        bool graph_changed = false;
        const bool graph_active =
            settings_ui::EasingGraphEditor(widget_motion, graph_id, bezier,
                                           ImVec2(graph_w, graph_h), scale, content_alpha,
                                           &graph_changed, font_small_);
        DelayedTooltip(
            "Drag the two handles to shape the easing curve. Hold Shift for finer steps.", scale);
        if (graph_changed) {
          *bezier_dirty = true;
          if (custom_bezier_callback_) {
            custom_bezier_callback_(is_minimize, *bezier, false);
          }
          ForceRender();
        }
        bool& was_active = is_minimize ? minimize_bezier_active_ : restore_bezier_active_;
        if (was_active && !graph_active && *bezier_dirty) {
          const bool saved =
              !custom_bezier_callback_ || custom_bezier_callback_(is_minimize, *bezier, true);
          RecordSaveResult(saved);
          if (saved) *bezier_dirty = false;
        }
        was_active = graph_active;
      }
      layout.EndRow();
    };

    {
      int style_index = selected_index(style_names, animation_style_);
      combo_row("##animation_style", "Animation", &style_index, style_names, [&] {
        const std::string previous = animation_style_;
        animation_style_ = style_names[style_index];
        const bool saved =
            !animation_style_callback_ || animation_style_callback_(animation_style_);
        if (!saved) animation_style_ = previous;
        RecordSaveResult(saved);
      });
    }
    easing_block("##minimize_easing", "##minimize_bezier_graph", "Minimize easing",
                 &minimize_easing_, &minimize_custom_bezier_, &minimize_bezier_dirty_, true);
    easing_block("##restore_easing", "##restore_bezier_graph", "Restore easing", &restore_easing_,
                 &restore_custom_bezier_, &restore_bezier_dirty_, false);
    layout.EndGroup();

    layout.SectionCaption(font_small_, kCaptionTextSize, "LOOK");
    layout.BeginGroup();
    layout.BeginStackRow(20.0f, settings_ui::Metrics::kSegmentHeight);
    layout.RowTitle(font_body_, kLabelTextSize, "Quality", kPrimaryTextColor);
    layout.RowSubtitle(font_small_, kHelperTextSize, "Mesh resolution and power usage",
                       kSecondaryTextColor);
    {
      constexpr std::array quality_labels = {"Automatic", "Best quality", "Power saving"};
      int quality_segment = quality_mode_ == "best_quality" ? 1
                            : quality_mode_ == "power_saving" ? 2
                                                             : 0;
      const float seg_w = layout.content_width();
      layout.SetCursor(layout.content_left(), layout.StackControlY());
      if (SegmentSelector(widget_motion, "##quality_mode", quality_labels, &quality_segment, seg_w,
                          font_body_, scale, content_alpha)) {
        const std::string previous = quality_mode_;
        quality_mode_ = quality_segment == 1   ? "best_quality"
                        : quality_segment == 2 ? "power_saving"
                                               : "automatic";
        const bool saved = !quality_mode_callback_ || quality_mode_callback_(quality_mode_);
        if (!saved) quality_mode_ = previous;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();

    layout.BeginRow(settings_ui::Metrics::kRowHeight);
    {
      const float slider_w = layout.ControlMaxWidth(340.0f);
      layout.ReserveControl(slider_w);
      layout.RowTitle(font_body_, kLabelTextSize, "Strength", kPrimaryTextColor);
      const ImVec2 cursor = layout.ControlCursor(slider_w, slider_h);
      layout.SetCursor(cursor.x, cursor.y);
      // Same spring-fill Slider path as Minimize/Restore durations (smooth pearl travel).
      float proposed = genie_strength_;
      const bool active =
          Slider(widget_motion, "##genie_strength", "", &proposed, 0.25f, 1.0f, slider_w, scale,
                 content_alpha, font_small_, 0.01f, 100.0f, 0, "%");
      DelayedTooltip("How strongly the window bends toward the taskbar target.", scale);
      if (active && std::abs(proposed - genie_strength_) > 0.0001f) {
        genie_strength_ = proposed;
        strength_slider_dirty_ = true;
        if (strength_callback_) strength_callback_(genie_strength_, false);
      }
      if (strength_slider_active_ && !active && strength_slider_dirty_) {
        const bool saved = !strength_callback_ || strength_callback_(genie_strength_, true);
        RecordSaveResult(saved);
        if (saved) strength_slider_dirty_ = false;
      }
      strength_slider_active_ = active;
    }
    layout.EndRow();

    layout.BeginRow(settings_ui::Metrics::kRowHeight);
    {
      constexpr std::array fade_names = {"No fade", "Subtle", "Strong"};
      int fade_index = selected_index(fade_names, fade_strength_);
      layout.ReserveControl(combo_w);
      layout.RowTitle(font_body_, kLabelTextSize, "Fade", kPrimaryTextColor);
      const ImVec2 cursor = layout.ControlCursor(combo_w, combo_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Combo(widget_motion, "##fade_strength", "", &fade_index, fade_names,
                ImVec2(combo_w, combo_h), font_small_, font_body_, scale, content_alpha)) {
        const std::string previous = fade_strength_;
        fade_strength_ = fade_names[fade_index];
        const bool saved = !fade_callback_ || fade_callback_(fade_strength_);
        if (!saved) fade_strength_ = previous;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();

    layout.BeginRow(settings_ui::Metrics::kRowHeightTall);
    layout.ReserveControl(toggle_w);
    layout.RowTitle(font_body_, kLabelTextSize, "Target indicator", kPrimaryTextColor);
    layout.RowSubtitle(font_small_, kHelperTextSize, "Flash the taskbar slot during minimize",
                       kSecondaryTextColor);
    {
      bool proposed = show_target_indicator_;
      const ImVec2 cursor = layout.ControlCursor(toggle_w, toggle_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Toggle(widget_motion, "##target_indicator", &proposed, scale, content_alpha)) {
        const bool saved = !target_indicator_callback_ || target_indicator_callback_(proposed);
        if (saved) show_target_indicator_ = proposed;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();
    layout.EndGroup();
  }

  // ── Apps ────────────────────────────────────────────────────────────────
  if (selected_page_ == Page::kApplications) {
    layout.Title(font_title_, kPageTitleTextSize, "Apps", font_small_, kPageSubtitleTextSize,
                 "Skip the effect for selected programs");

    const ULONGLONG now = GetTickCount64();
    if (now - last_active_apps_refresh_ms_ > 2000 || cached_active_apps_.empty()) {
      cached_active_apps_ = GetActiveApplications();
      last_active_apps_refresh_ms_ = now;
    }

    struct AppItem {
      std::string name;
      bool is_excluded = false;
      bool is_active = false;
    };
    std::vector<AppItem> items;
    for (const std::string& excluded : excluded_applications_) {
      items.push_back({excluded, true, false});
    }
    for (const std::string& active : cached_active_apps_) {
      auto it = std::find_if(items.begin(), items.end(), [&active](const AppItem& item) {
        return ExecutableNamesEqual(item.name, active);
      });
      if (it != items.end()) {
        it->is_active = true;
      } else {
        items.push_back({active, false, true});
      }
    }
    std::sort(items.begin(), items.end(),
              [](const AppItem& a, const AppItem& b) { return a.name < b.name; });

    std::string filter = exclusion_input_.data();
    std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char character) {
      return static_cast<char>(std::tolower(character));
    });

    std::vector<AppItem> filtered_items;
    for (const auto& item : items) {
      if (filter.empty()) {
        filtered_items.push_back(item);
      } else {
        std::string name_lower = item.name;
        std::transform(
            name_lower.begin(), name_lower.end(), name_lower.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (name_lower.find(filter) != std::string::npos) {
          filtered_items.push_back(item);
        }
      }
    }

    // Search field fills the stack control band exactly → equal card padding top/bottom.
    layout.BeginGroup();
    {
      const float field_h_logical = 36.0f;
      layout.BeginStackRow(0.0f, field_h_logical);
      const float field_w = layout.content_width();
      const float field_h = layout.StackControlHeight();
      const float font_px = font_body_ ? font_body_->FontSize : px(15.0f);
      const float pad_y = std::max(6.0f * scale, (field_h - font_px) * 0.5f);
      layout.SetCursor(layout.content_left(), layout.StackControlY());
      if (font_body_) ImGui::PushFont(font_body_);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(12.0f), pad_y));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, px(1.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                          settings_ui::Metrics::kControlRounding * scale);
      ImGui::PushStyleColor(ImGuiCol_FrameBg,
                            ImGui::ColorConvertU32ToFloat4(settings_ui::kPanelHeader));
      ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(settings_ui::kBorder));
      ImGui::SetNextItemWidth(field_w);
      ImGui::InputTextWithHint("##app_search", "Filter apps…", exclusion_input_.data(),
                               exclusion_input_.size());
      ImGui::PopStyleColor(2);
      ImGui::PopStyleVar(3);
      if (font_body_) ImGui::PopFont();
      layout.EndRow();
    }
    layout.EndGroup();

    const std::string apps_caption = std::format("{} APPS", filtered_items.size());
    layout.SectionCaption(font_small_, kCaptionTextSize, apps_caption.c_str());

    // Dynamic height, no nested scrollbar — page scroll only.
    layout.BeginGroup();
    if (filtered_items.empty()) {
      layout.BeginRow(settings_ui::Metrics::kRowHeight);
      layout.RowTitle(font_small_, kHelperTextSize,
                      filter.empty() ? "No applications found" : "No matches", kSecondaryTextColor);
      layout.EndRow();
    } else {
      for (size_t i = 0; i < filtered_items.size(); ++i) {
        const bool inactive = !filtered_items[i].is_active;
        if (inactive) {
          layout.BeginRow(settings_ui::Metrics::kRowHeightTall);
          layout.ReserveControl(toggle_w);
          layout.RowTitle(font_body_, kLabelTextSize, filtered_items[i].name.c_str(),
                          kSecondaryTextColor);
          layout.RowSubtitle(font_small_, kHelperTextSize, "inactive", kSecondaryTextColor);
        } else {
          layout.BeginRow(settings_ui::Metrics::kRowHeight);
          layout.ReserveControl(toggle_w);
          layout.RowTitle(font_body_, kLabelTextSize, filtered_items[i].name.c_str(),
                          kPrimaryTextColor);
        }
        {
          const ImVec2 cursor = layout.ControlCursor(toggle_w, toggle_h);
          layout.SetCursor(cursor.x, cursor.y);
          const std::string toggle_id = std::format("##toggle_exclude_{}", i);
          bool excluded = filtered_items[i].is_excluded;
          if (Toggle(widget_motion, toggle_id.c_str(), &excluded, scale, content_alpha)) {
            if (exclusion_callback_) {
              if (exclusion_callback_(filtered_items[i].name, excluded)) {
                exclusion_error_.clear();
                if (excluded) {
                  if (std::find(excluded_applications_.begin(), excluded_applications_.end(),
                                filtered_items[i].name) == excluded_applications_.end()) {
                    excluded_applications_.push_back(filtered_items[i].name);
                  }
                } else {
                  std::erase(excluded_applications_, filtered_items[i].name);
                }
              } else {
                exclusion_error_ = "Could not update exclusion.";
              }
            }
          }
        }
        layout.EndRow();
      }
    }
    layout.EndGroup();

    const std::string& visible_error =
        persistence_error_.empty() ? exclusion_error_ : persistence_error_;
    if (!visible_error.empty()) {
      {
        const ImVec2 p = layout.ToScreen(layout.content_left(), layout.y());
        draw->AddText(font_small_, font_small_->FontSize,
                      ImVec2(std::floor(p.x + 0.5f), std::floor(p.y + 0.5f)),
                      WithAlpha(IM_COL32(235, 120, 120, 255), content_alpha),
                      visible_error.c_str());
      }
      layout.Gap(22.0f);
    }
  }

  // ── System ──────────────────────────────────────────────────────────────
  if (selected_page_ == Page::kWindowsIntegration) {
    layout.Title(font_title_, kPageTitleTextSize, "System", font_small_, kPageSubtitleTextSize,
                 "Fullscreen and power behavior");

    layout.SectionCaption(font_small_, kCaptionTextSize, "WINDOWS");
    layout.BeginGroup();
    layout.BeginRow(settings_ui::Metrics::kRowHeightTall);
    layout.ReserveControl(toggle_w);
    layout.RowTitle(font_body_, kLabelTextSize, "Pause in fullscreen", kPrimaryTextColor);
    layout.RowSubtitle(font_small_, kHelperTextSize,
                       "Use native transitions during exclusive fullscreen", kSecondaryTextColor);
    {
      const bool previous = disable_animations_fullscreen_;
      const ImVec2 cursor = layout.ControlCursor(toggle_w, toggle_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Toggle(widget_motion, "##disable_fullscreen_animations", &disable_animations_fullscreen_,
                 scale, content_alpha)) {
        const bool saved = !fullscreen_behavior_callback_ ||
                           fullscreen_behavior_callback_(disable_animations_fullscreen_);
        if (!saved) disable_animations_fullscreen_ = previous;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();
    layout.EndGroup();

    layout.SectionCaption(font_small_, kCaptionTextSize, "POWER");
    layout.BeginGroup();
    layout.BeginRow(settings_ui::Metrics::kRowHeight);
    layout.ReserveControl(toggle_w);
    layout.RowTitle(font_body_, kLabelTextSize, "Disable in battery saver", kPrimaryTextColor);
    {
      bool proposed = disable_effects_battery_saver_;
      const ImVec2 cursor = layout.ControlCursor(toggle_w, toggle_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (Toggle(widget_motion, "##disable_in_battery_saver", &proposed, scale, content_alpha)) {
        const bool saved = !battery_saver_callback_ || battery_saver_callback_(proposed);
        if (saved) disable_effects_battery_saver_ = proposed;
        RecordSaveResult(saved);
      }
    }
    layout.EndRow();
    layout.EndGroup();
  }

  // ── Hotkeys ─────────────────────────────────────────────────────────────
  if (selected_page_ == Page::kHotkeys) {
    layout.Title(font_title_, kPageTitleTextSize, "Hotkeys", font_small_, kPageSubtitleTextSize,
                 "Click Change, then press a combination");

    constexpr std::array labels = {
        "Toggle effect",
        "Open settings",
        "Repair windows",
    };
    layout.BeginGroup();
    for (size_t index = 0; index < labels.size(); ++index) {
      layout.BeginRow(settings_ui::Metrics::kRowHeightTall);
      const float change_w = px(86.0f);
      const float disable_w = px(78.0f);
      const float gap = px(8.0f);
      const float total = change_w + gap + disable_w;
      layout.ReserveControl(total);
      layout.RowTitle(font_body_, kLabelTextSize, labels[index], kPrimaryTextColor);
      const std::string binding_text =
          editing_hotkey_ == static_cast<int>(index) ? "Press keys…" : HotkeyText(hotkeys_[index]);
      const ImU32 binding_color = hotkey_available_[index] || hotkeys_[index].virtual_key == 0
                                      ? kSecondaryTextColor
                                      : IM_COL32(235, 120, 120, 255);
      layout.RowSubtitle(font_small_, kValueTextSize, binding_text.c_str(), binding_color);

      const ImVec2 base = layout.ControlCursor(total, btn_h);
      layout.SetCursor(base.x, base.y);
      const std::string change_id = std::format("##change_hotkey_{}", index);
      if (CompactButton(widget_motion, change_id.c_str(), "Change", ImVec2(change_w, btn_h),
                        font_body_, scale, content_alpha)) {
        editing_hotkey_ = static_cast<int>(index);
        hotkey_feedback_ = "Press a combination, or Esc to cancel";
      }
      layout.SetCursor(base.x + change_w + gap, base.y);
      const std::string disable_id = std::format("##disable_hotkey_{}", index);
      if (CompactButton(widget_motion, disable_id.c_str(), "Clear", ImVec2(disable_w, btn_h),
                        font_body_, scale, content_alpha)) {
        const HotkeyUpdateResult result =
            hotkey_update_callback_
                ? hotkey_update_callback_(static_cast<HotkeyAction>(index), HotkeyBinding{})
                : HotkeyUpdateResult::kInvalid;
        hotkey_feedback_ = HotkeyResultText(result);
        editing_hotkey_ = -1;
      }
      layout.EndRow();
    }
    layout.EndGroup();

    if (!hotkey_feedback_.empty()) {
      {
        const ImVec2 p = layout.ToScreen(layout.content_left(), layout.y());
        draw->AddText(font_small_, font_small_->FontSize,
                      ImVec2(std::floor(p.x + 0.5f), std::floor(p.y + 0.5f)),
                      WithAlpha(kSecondaryTextColor, content_alpha), hotkey_feedback_.c_str());
      }
      layout.Gap(20.0f);
    }
  }

  // ── Repair ──────────────────────────────────────────────────────────────
  if (selected_page_ == Page::kDiagnostics) {
    const ULONGLONG now = GetTickCount64();
    if (diagnostics_callback_ != nullptr &&
        (diagnostics_.effect.empty() || now - last_diagnostics_refresh_ms_ >= 500)) {
      diagnostics_ = diagnostics_callback_();
      last_diagnostics_refresh_ms_ = now;
    }

    layout.Title(font_title_, kPageTitleTextSize, "Repair", font_small_, kPageSubtitleTextSize,
                 "Status, recovery tools, and system info");

    // Same label/value row language as the rest of settings (not a dense debug table).
    const auto status_row = [&](const char* title, const std::string& value) {
      layout.BeginRow(settings_ui::Metrics::kRowHeight);
      layout.ReserveControl(layout.content_width() * 0.55f);
      layout.RowTitle(font_body_, kLabelTextSize, title, kPrimaryTextColor);
      layout.RowValue(font_small_, kValueTextSize, value.c_str(), kSecondaryTextColor);
      layout.EndRow();
    };

    layout.SectionCaption(font_small_, kCaptionTextSize, "STATUS");
    layout.BeginGroup();
    status_row("Effect", diagnostics_.effect);
    status_row("Hook", diagnostics_.hook);
    status_row("Renderer", diagnostics_.renderer);
    status_row("D3D device", diagnostics_.d3d_device);
    status_row("Animations", diagnostics_.active_animations);
    status_row("Watchdog", diagnostics_.watchdog);
    layout.EndGroup();

    layout.SectionCaption(font_small_, kCaptionTextSize, "DISPLAY");
    layout.BeginGroup();
    status_row("Refresh rate", diagnostics_.display_refresh);
    status_row("Monitor", diagnostics_.window_monitor);
    status_row("Taskbar", diagnostics_.taskbar);
    status_row("Startup repair", diagnostics_.startup_repair);
    layout.EndGroup();

    layout.SectionCaption(font_small_, kCaptionTextSize, "MACHINE");
    layout.BeginGroup();
    status_row("Windows", diagnostics_.windows_version);
    status_row("GPU", diagnostics_.graphics_adapter);
    status_row("Displays", diagnostics_.monitor_configuration);
    status_row("Log folder", diagnostics_.log_folder_size);
    layout.EndGroup();

    // Action rows match Hotkeys: label left, button right.
    constexpr std::array actions = {
        std::tuple{"Copy report", "Copy a full diagnostics dump", DiagnosticsAction::kCopy},
        std::tuple{"Open logs", "Reveal the log folder in Explorer",
                   DiagnosticsAction::kOpenLogFolder},
        std::tuple{"Repair windows", "Recover stuck or orphaned windows",
                   DiagnosticsAction::kRepairWindows},
        std::tuple{"Restart renderer", "Rebuild the Direct3D overlay path",
                   DiagnosticsAction::kRestartRenderer},
    };
    layout.SectionCaption(font_small_, kCaptionTextSize, "TOOLS");
    layout.BeginGroup();
    for (size_t index = 0; index < actions.size(); ++index) {
      const auto& [title, helper, action] = actions[index];
      layout.BeginRow(settings_ui::Metrics::kRowHeightTall);
      const float action_w = px(108.0f);
      layout.ReserveControl(action_w);
      layout.RowTitle(font_body_, kLabelTextSize, title, kPrimaryTextColor);
      layout.RowSubtitle(font_small_, kHelperTextSize, helper, kSecondaryTextColor);
      const ImVec2 cursor = layout.ControlCursor(action_w, btn_h);
      layout.SetCursor(cursor.x, cursor.y);
      const std::string id = std::format("##diagnostics_action_{}", index);
      const char* button_label = action == DiagnosticsAction::kCopy            ? "Copy"
                                 : action == DiagnosticsAction::kOpenLogFolder ? "Open"
                                 : action == DiagnosticsAction::kRepairWindows ? "Repair"
                                                                               : "Restart";
      if (CompactButton(widget_motion, id.c_str(), button_label, ImVec2(action_w, btn_h),
                        font_body_, scale, content_alpha)) {
        const bool succeeded = diagnostics_action_callback_ && diagnostics_action_callback_(action);
        diagnostics_feedback_ = succeeded ? "Done" : "Failed";
        last_diagnostics_refresh_ms_ = 0;
      }
      layout.EndRow();
    }
    layout.EndGroup();

    if (!diagnostics_feedback_.empty()) {
      {
        const ImVec2 p = layout.ToScreen(layout.content_left(), layout.y());
        draw->AddText(font_small_, font_small_->FontSize,
                      ImVec2(std::floor(p.x + 0.5f), std::floor(p.y + 0.5f)),
                      WithAlpha(kSecondaryTextColor, content_alpha), diagnostics_feedback_.c_str());
      }
      layout.Gap(18.0f);
    }
  }

  // ── About ───────────────────────────────────────────────────────────────
  if (selected_page_ == Page::kAbout) {
    // Always pull a fresh snapshot so Build reflects the live PE product version
    // (not a one-shot static string left over from first open).
    if (diagnostics_callback_ != nullptr) {
      const ULONGLONG now = GetTickCount64();
      if (diagnostics_.version.empty() || now - last_diagnostics_refresh_ms_ >= 500) {
        diagnostics_ = diagnostics_callback_();
        last_diagnostics_refresh_ms_ = now;
      }
    }
    layout.Title(font_title_, kPageTitleTextSize, "About", font_small_, kPageSubtitleTextSize,
                 "Product info and open-source licenses");

    // Row title is "Build" — show the product version string as-is (no "Version " prefix).
    const std::string version_text = diagnostics_.version.empty() ? "—" : diagnostics_.version;

    layout.SectionCaption(font_small_, kCaptionTextSize, "PRODUCT");
    layout.BeginGroup();
    layout.BeginRow(settings_ui::Metrics::kRowHeightHero);
    layout.RowTitle(font_medium_, kSectionTitleTextSize, "Genie Effect", kPrimaryTextColor);
    layout.RowSubtitle(font_small_, kHelperTextSize, "Native genie minimize for Windows",
                       kSecondaryTextColor);
    layout.EndRow();
    layout.BeginRow(settings_ui::Metrics::kRowHeight);
    layout.ReserveControl(layout.content_width() * 0.55f);
    layout.RowTitle(font_body_, kLabelTextSize, "Build", kPrimaryTextColor);
    layout.RowValue(font_small_, kValueTextSize, version_text.c_str(), kSecondaryTextColor);
    layout.EndRow();
    layout.EndGroup();

    layout.SectionCaption(font_small_, kCaptionTextSize, "FONTS");
    layout.BeginGroup();
    layout.BeginRow(settings_ui::Metrics::kRowHeightTall);
    {
      const float license_w = px(112.0f);
      layout.ReserveControl(license_w);
      layout.RowTitle(font_body_, kLabelTextSize, "Inter", kPrimaryTextColor);
      layout.RowSubtitle(font_small_, kHelperTextSize, "SIL Open Font License 1.1",
                         kSecondaryTextColor);
      const ImVec2 cursor = layout.ControlCursor(license_w, btn_h);
      layout.SetCursor(cursor.x, cursor.y);
      if (CompactButton(widget_motion, "##font_license", "License", ImVec2(license_w, btn_h),
                        font_body_, scale, content_alpha)) {
        ImGui::OpenPopup("Inter License");
      }
    }
    layout.EndRow();
    layout.EndGroup();

    // License modal — same card language as settings groups (panel fill, quiet border, Inter).
    {
      const ImGuiViewport* viewport = ImGui::GetMainViewport();
      const ImVec2 license_size(std::min(px(560.0f), viewport->WorkSize.x - px(48.0f)),
                                std::min(px(460.0f), viewport->WorkSize.y - px(48.0f)));
      ImGui::SetNextWindowSize(license_size, ImGuiCond_Appearing);
      ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                     viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                              ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(22.0f), px(20.0f)));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, px(settings_ui::Metrics::kCardRounding));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, std::max(1.0f, scale));
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(px(8.0f), px(8.0f)));
      ImGui::PushStyleColor(ImGuiCol_PopupBg, colors::panel);
      ImGui::PushStyleColor(ImGuiCol_Border, colors::border);
      ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
      ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));

      // No window scrollbar — body child owns scroll so the modal chrome stays clean.
      if (ImGui::BeginPopupModal("Inter License", nullptr,
                                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();

        ImFont* title_font = font_medium_ ? font_medium_ : font_body_;
        ImGui::PushFont(title_font);
        ImGui::TextUnformatted("Inter");
        ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_Text, colors::textDim);
        if (font_small_) ImGui::PushFont(font_small_);
        ImGui::TextUnformatted("SIL Open Font License 1.1");
        if (font_small_) ImGui::PopFont();
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, px(4.0f)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, px(4.0f)));

        const float close_h = btn_h;
        const float footer_gap = px(14.0f);
        // Pin body height so header+body+footer fill the modal exactly (no window overflow scroll).
        const float body_h =
            std::max(px(120.0f), ImGui::GetContentRegionAvail().y - close_h - footer_gap);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::BeginChild("##font_license_text", ImVec2(0.0f, body_h), ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoBackground)) {
          static const std::string license = LoadEmbeddedText(IDR_UI_FONT_LICENSE);
          ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, px(6.0f)));
          ImGui::PushStyleColor(ImGuiCol_Text, colors::textDim);
          if (font_small_) ImGui::PushFont(font_small_);
          ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
          ImGui::TextUnformatted(license.empty() ? "The embedded font license could not be loaded."
                                                 : license.c_str());
          ImGui::PopTextWrapPos();
          if (font_small_) ImGui::PopFont();
          ImGui::PopStyleColor();
          ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::Dummy(ImVec2(0.0f, footer_gap - px(4.0f)));
        const float close_w = px(120.0f);
        {
          const float avail = ImGui::GetContentRegionAvail().x;
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - close_w) * 0.5f));
        }
        if (CompactButton(widget_motion, "##close_font_license", "Close", ImVec2(close_w, close_h),
                          font_body_, scale, 1.0f)) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      ImGui::PopStyleColor(4);
      ImGui::PopStyleVar(4);
    }
  }

  ImGui::SetCursorPos(
      ImVec2(0.0f, layout.content_bottom() + px(settings_ui::Metrics::kScrollBottomPadding)));
  ImGui::Dummy(ImVec2(1.0f, 1.0f));

  const float scroll_y = ImGui::GetScrollY();
  const float scroll_max = ImGui::GetScrollMaxY();
  const float fade_height = px(settings_ui::Metrics::kScrollFadeHeight);
  // Keep fades clear of the overlay scrollbar strip on the right edge.
  const float scrollbar_reserve = px(10.0f);
  const float fade_right = origin.x + size.x - scrollbar_reserve;
  if (scroll_y > 0.5f) {
    draw->AddRectFilledMultiColor(origin, ImVec2(fade_right, origin.y + fade_height),
                                  WithAlpha(settings_ui::kMainBackground, content_alpha),
                                  WithAlpha(settings_ui::kMainBackground, content_alpha),
                                  WithAlpha(settings_ui::kMainBackground, 0.0f),
                                  WithAlpha(settings_ui::kMainBackground, 0.0f));
  }
  if (scroll_y + 0.5f < scroll_max) {
    draw->AddRectFilledMultiColor(ImVec2(origin.x, origin.y + size.y - fade_height),
                                  ImVec2(fade_right, origin.y + size.y),
                                  WithAlpha(settings_ui::kMainBackground, 0.0f),
                                  WithAlpha(settings_ui::kMainBackground, 0.0f),
                                  WithAlpha(settings_ui::kMainBackground, content_alpha),
                                  WithAlpha(settings_ui::kMainBackground, content_alpha));
  }

  // Overlay scrollbar: inset by shell rounding so it follows the rounded window, not a
  // full-height square strip that fights DWM corner radius.
  if (scroll_max > 0.5f && content_alpha > 0.02f) {
    const float bar_w = px(5.0f);
    const float edge_pad = px(4.0f);
    // Match outer shell radius so the thumb never enters the curved corner zone.
    const float y_inset = shell_round + px(2.0f);
    const float track_x = origin.x + size.x - edge_pad - bar_w;
    const float track_top = origin.y + y_inset;
    const float track_bot = origin.y + size.y - y_inset;
    const float track_h = std::max(1.0f, track_bot - track_top);
    const float view_h = size.y;
    const float content_h = view_h + scroll_max;
    const float grab_h =
        std::clamp(track_h * (view_h / std::max(content_h, 1.0f)), px(28.0f), track_h);
    const float grab_travel = std::max(0.0f, track_h - grab_h);
    const float grab_t = scroll_max > 0.0f ? std::clamp(scroll_y / scroll_max, 0.0f, 1.0f) : 0.0f;
    const float grab_y = track_top + grab_travel * grab_t;

    const ImVec2 track_min(track_x, track_top);
    const ImVec2 track_max(track_x + bar_w, track_bot);
    const ImVec2 grab_min(track_x, grab_y);
    const ImVec2 grab_max(track_x + bar_w, grab_y + grab_h);

    // Hit target slightly wider than the visual thumb for easier grabbing.
    const ImVec2 hit_min(track_x - px(4.0f), track_top);
    const ImVec2 hit_max(track_x + bar_w + px(2.0f), track_bot);
    const bool track_hovered = ImGui::IsMouseHoveringRect(hit_min, hit_max, false);
    const bool grab_hovered = ImGui::IsMouseHoveringRect(grab_min, grab_max, false);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID drag_id = ImGui::GetID("##page_scrollbar_drag");
    const ImGuiID grab_off_id = ImGui::GetID("##page_scrollbar_grab_off");
    bool dragging = storage->GetBool(drag_id, false);
    float grab_off = storage->GetFloat(grab_off_id, 0.0f);
    const ImGuiIO& io = ImGui::GetIO();

    if (track_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      if (!grab_hovered && grab_travel > 0.0f) {
        const float click_t =
            std::clamp((io.MousePos.y - track_top - grab_h * 0.5f) / grab_travel, 0.0f, 1.0f);
        ImGui::SetScrollY(click_t * scroll_max);
      }
      // Recompute grab after possible jump so drag offset stays correct.
      const float gy =
          track_top +
          grab_travel *
              (scroll_max > 0.0f ? std::clamp(ImGui::GetScrollY() / scroll_max, 0.0f, 1.0f) : 0.0f);
      grab_off = io.MousePos.y - gy;
      dragging = true;
    }
    if (dragging) {
      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragging = false;
      } else if (grab_travel > 0.0f) {
        const float new_t =
            std::clamp((io.MousePos.y - grab_off - track_top) / grab_travel, 0.0f, 1.0f);
        ImGui::SetScrollY(new_t * scroll_max);
      }
    }
    storage->SetBool(drag_id, dragging);
    storage->SetFloat(grab_off_id, grab_off);

    if (track_hovered || dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    // Fresh grab rect after possible SetScrollY this frame.
    const float live_scroll = ImGui::GetScrollY();
    const float live_t =
        scroll_max > 0.0f ? std::clamp(live_scroll / scroll_max, 0.0f, 1.0f) : 0.0f;
    const float live_grab_y = track_top + grab_travel * live_t;
    const ImVec2 live_grab_min(track_x, live_grab_y);
    const ImVec2 live_grab_max(track_x + bar_w, live_grab_y + grab_h);

    const float hover_amt = (grab_hovered || dragging || track_hovered) ? 1.0f : 0.0f;
    const float grab_alpha = (0.16f + 0.14f * hover_amt) * content_alpha;
    // FG list: above page content + scroll fades; still clipped by the HWND/DWM round.
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    fg->AddRectFilled(live_grab_min, live_grab_max,
                      ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, grab_alpha)), bar_w * 0.5f);
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();

  // Save toast — foreground draw list so it sits above page, popups, and scroll fades.
  // Position: top-center of the content pane (right of sidebar), not bottom-right.
  {
    const ULONGLONG now = GetTickCount64();
    const bool toast_live = !save_feedback_.empty() && now < save_feedback_until_ms_;
    auto& motion = WindowMotion::System();
    const auto& tokens = WindowMotion::Tokens();
    const auto show_key = ::ui::motion::MotionKey("toast", "save", "show");
    const float show = motion.value(show_key, toast_live ? 1.0f : 0.0f,
                                    toast_live ? tokens.fadeFast : tokens.popupClose, 0.0f);

    if (!toast_live && show <= 0.02f) {
      if (!save_feedback_.empty()) {
        save_feedback_.clear();
        motion.forget(show_key);
      }
    } else if (show > 0.01f && !save_feedback_.empty()) {
      ImFont* toast_font = font_small_ ? font_small_ : ImGui::GetFont();
      const float font_sz = toast_font->FontSize;
      const ImVec2 text_size =
          toast_font->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, save_feedback_.c_str());
      const bool is_error = save_feedback_error_;
      const float pad_x = px(14.0f);
      const float pad_y = px(8.0f);
      const float icon_slot = is_error ? 0.0f : px(16.0f);
      const float toast_h = text_size.y + pad_y * 2.0f;
      const float toast_w = text_size.x + pad_x * 2.0f + icon_slot;
      // Content pane (second half): horizontally centered, near the top.
      const float content_left = sidebar_width;
      const float content_center_x = content_left + (window_size.x - content_left) * 0.5f;
      const float top_margin = px(14.0f);
      const float slide = (1.0f - show) * px(-10.0f);  // enters from above
      const float toast_x = content_center_x - toast_w * 0.5f;
      const float toast_y = top_margin + slide;
      const ImVec2 toast_min = window_point(toast_x, toast_y);
      const ImVec2 toast_max(toast_min.x + toast_w, toast_min.y + toast_h);
      const float rounding = toast_h * 0.5f;
      const float a = content_alpha * show;

      ImDrawList* fg = ImGui::GetForegroundDrawList();
      fg->AddRectFilled(ImVec2(toast_min.x, toast_min.y + px(1.5f)),
                        ImVec2(toast_max.x, toast_max.y + px(1.5f)),
                        IM_COL32(0, 0, 0, static_cast<int>(70.0f * a)), rounding);
      fg->AddRectFilled(toast_min, toast_max, IM_COL32(28, 28, 30, static_cast<int>(250.0f * a)),
                        rounding);
      fg->AddRect(toast_min, toast_max,
                  IM_COL32(is_error ? 90 : 52, is_error ? 40 : 52, is_error ? 42 : 55,
                           static_cast<int>(255.0f * a)),
                  rounding, 0, std::max(1.0f, scale));

      float text_x = toast_min.x + pad_x;
      if (!is_error) {
        const ImVec2 cc(toast_min.x + pad_x + px(5.0f), toast_min.y + toast_h * 0.5f);
        const float s = px(3.2f);
        fg->PathLineTo(ImVec2(cc.x - s, cc.y));
        fg->PathLineTo(ImVec2(cc.x - s * 0.2f, cc.y + s * 0.85f));
        fg->PathLineTo(ImVec2(cc.x + s * 1.15f, cc.y - s * 0.85f));
        fg->PathStroke(IM_COL32(150, 210, 165, static_cast<int>(255.0f * a)), 0,
                       std::max(1.2f, 1.4f * scale));
        text_x += icon_slot;
      }

      const ImU32 text_col = is_error ? IM_COL32(235, 140, 140, static_cast<int>(255.0f * a))
                                      : IM_COL32(220, 220, 224, static_cast<int>(255.0f * a));
      fg->AddText(toast_font, font_sz,
                  ImVec2(std::floor(text_x + 0.5f),
                         settings_ui::CenteredTextTop(toast_font, toast_min.y, toast_h)),
                  text_col, save_feedback_.c_str());
    }
  }

  ImGui::End();
}

LRESULT CALLBACK SettingsWindow::WindowProc(HWND hwnd, UINT message, WPARAM w_param,
                                            LPARAM l_param) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* settings = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  const float scale = settings == nullptr ? 1.0f : settings->ui_scale_;

  if (settings != nullptr && settings->taskbar_created_message_ != 0 &&
      message == settings->taskbar_created_message_) {
    settings->tray_icon_added_ = false;
    if (!IsWindowVisible(hwnd)) settings->AddTrayIcon();
    return 0;
  }

  // Avoid DefWindowProc's modal caption-move loop. It blocks the application
  // thread and freezes active Genie animations while the settings window is dragged.
  if (settings != nullptr && message == WM_LBUTTONDOWN) {
    const POINT client_point{static_cast<short>(LOWORD(l_param)),
                             static_cast<short>(HIWORD(l_param))};
    RECT client{};
    GetClientRect(hwnd, &client);
    // Keep the whole traffic-light cluster out of the custom titlebar drag zone.
    const LONG traffic_lights_end = static_cast<LONG>(140.0f * scale);
    const LONG header_actions_start = client.right - static_cast<LONG>(220.0f * scale);
    if (client_point.y >= 0 && client_point.y < static_cast<LONG>(kHeaderHeight * scale) &&
        client_point.x >= traffic_lights_end && client_point.x < header_actions_start) {
      POINT cursor{};
      RECT window_rect{};
      GetCursorPos(&cursor);
      GetWindowRect(hwnd, &window_rect);
      settings->window_drag_offset_ =
          POINT{cursor.x - window_rect.left, cursor.y - window_rect.top};
      settings->window_dragging_ = true;
      SetCapture(hwnd);
      return 0;
    }
  }
  if (settings != nullptr && message == WM_MOUSEMOVE && settings->window_dragging_) {
    if ((w_param & MK_LBUTTON) == 0) {
      settings->window_dragging_ = false;
      if (GetCapture() == hwnd) ReleaseCapture();
      return 0;
    }
    POINT cursor{};
    GetCursorPos(&cursor);
    SetWindowPos(hwnd, nullptr, cursor.x - settings->window_drag_offset_.x,
                 cursor.y - settings->window_drag_offset_.y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    settings->ForceRender();
    return 0;
  }
  if (settings != nullptr && message == WM_LBUTTONUP && settings->window_dragging_) {
    settings->window_dragging_ = false;
    if (GetCapture() == hwnd) ReleaseCapture();
    return 0;
  }
  if (settings != nullptr && (message == WM_CAPTURECHANGED || message == WM_CANCELMODE)) {
    settings->window_dragging_ = false;
  }

  if (settings != nullptr && settings->editing_hotkey_ >= 0 &&
      (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)) {
    const UINT virtual_key = static_cast<UINT>(w_param);
    if (virtual_key == VK_ESCAPE) {
      settings->editing_hotkey_ = -1;
      settings->hotkey_feedback_ = "Hotkey edit cancelled";
      settings->ForceRender();
      return 0;
    }
    if (virtual_key == VK_CONTROL || virtual_key == VK_LCONTROL || virtual_key == VK_RCONTROL ||
        virtual_key == VK_MENU || virtual_key == VK_LMENU || virtual_key == VK_RMENU ||
        virtual_key == VK_SHIFT || virtual_key == VK_LSHIFT || virtual_key == VK_RSHIFT ||
        virtual_key == VK_LWIN || virtual_key == VK_RWIN) {
      return 0;
    }
    std::uint32_t modifiers = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= MOD_CONTROL;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= MOD_ALT;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= MOD_SHIFT;
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0) {
      modifiers |= MOD_WIN;
    }
    const HotkeyAction action = static_cast<HotkeyAction>(settings->editing_hotkey_);
    const HotkeyUpdateResult result =
        settings->hotkey_update_callback_
            ? settings->hotkey_update_callback_(
                  action, HotkeyBinding{.modifiers = modifiers, .virtual_key = virtual_key})
            : HotkeyUpdateResult::kInvalid;
    settings->hotkey_feedback_ = HotkeyResultText(result);
    if (result == HotkeyUpdateResult::kSuccess) settings->editing_hotkey_ = -1;
    settings->ForceRender();
    return 0;
  }

  if (message == kTrayMessage && settings != nullptr) {
    if (l_param == WM_LBUTTONUP || l_param == WM_LBUTTONDBLCLK) {
      settings->Show(true);
      return 0;
    }
    if (l_param == WM_RBUTTONUP) {
      HMENU menu = CreatePopupMenu();
      if (menu != nullptr) {
        AppendMenuW(menu, MF_STRING | (settings->is_enabled_ ? MF_CHECKED : MF_UNCHECKED),
                    kTrayToggleEnabled, L"Genie Effect Enabled");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        if (settings->temporarily_paused_) {
          AppendMenuW(menu, MF_STRING, kTrayResume, L"Resume Genie Effect");
        } else {
          const UINT pause_flags = settings->is_enabled_ ? MF_STRING : MF_STRING | MF_GRAYED;
          AppendMenuW(menu, pause_flags, kTrayPauseTenMinutes, L"Pause for 10 minutes");
          AppendMenuW(menu, pause_flags, kTrayPauseOneHour, L"Pause for 1 hour");
          AppendMenuW(menu, pause_flags, kTrayPauseUntilRestart, L"Pause until next restart");
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kTrayShowSettings, L"Settings");
        AppendMenuW(menu, MF_STRING, kTrayRepairWindows, L"Repair Windows");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kTrayExit, L"Exit");
        POINT cursor{};
        GetCursorPos(&cursor);
        SetForegroundWindow(hwnd);
        const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                             cursor.x, cursor.y, 0, hwnd, nullptr);
        DestroyMenu(menu);
        if (selected == kTrayToggleEnabled && settings->toggle_callback_) {
          settings->toggle_callback_(!settings->is_enabled_);
        } else if (selected == kTrayResume && settings->pause_callback_) {
          settings->pause_callback_(TemporaryPauseAction::kResume);
        } else if (selected == kTrayPauseTenMinutes && settings->pause_callback_) {
          settings->pause_callback_(TemporaryPauseAction::kTenMinutes);
        } else if (selected == kTrayPauseOneHour && settings->pause_callback_) {
          settings->pause_callback_(TemporaryPauseAction::kOneHour);
        } else if (selected == kTrayPauseUntilRestart && settings->pause_callback_) {
          settings->pause_callback_(TemporaryPauseAction::kUntilRestart);
        } else if (selected == kTrayShowSettings) {
          settings->Show(true);
        } else if (selected == kTrayRepairWindows && settings->heal_callback_) {
          settings->heal_callback_();
        } else if (selected == kTrayExit && settings->exit_callback_) {
          settings->exit_callback_();
        }
      }
      return 0;
    }
  }

  if (settings != nullptr && settings->imgui_ready_) {
    bool needs_render = false;
    switch (message) {
      case WM_MOUSEMOVE:
      case WM_MOUSELEAVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_RBUTTONDOWN:
      case WM_RBUTTONUP:
      case WM_MBUTTONDOWN:
      case WM_MBUTTONUP:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_CHAR:
      case WM_SETFOCUS:
      case WM_KILLFOCUS:
      case WM_CAPTURECHANGED:
      case WM_CANCELMODE:
      case WM_PAINT:
        needs_render = true;
        break;
    }
    const bool imgui_handled = ImGui_ImplWin32_WndProcHandler(hwnd, message, w_param, l_param) != 0;
    if (needs_render) settings->ForceRender();
    if (imgui_handled) {
      return TRUE;
    }
  }

  switch (message) {
    case WM_HOTKEY: {
      if (settings != nullptr && settings->editing_hotkey_ >= 0) return 0;
      const int index = static_cast<int>(w_param) - kHotkeyBaseId;
      if (settings != nullptr && index >= 0 && index < static_cast<int>(HotkeyAction::kCount) &&
          settings->hotkey_action_callback_) {
        settings->hotkey_action_callback_(static_cast<HotkeyAction>(index));
      }
      return 0;
    }
    case kShowSettingsMessage:
      if (settings != nullptr) settings->Show(true);
      return 0;
    case WM_GETMINMAXINFO: {
      auto* limits = reinterpret_cast<MINMAXINFO*>(l_param);
      const UINT dpi = settings == nullptr ? USER_DEFAULT_SCREEN_DPI : settings->current_dpi_;
      limits->ptMinTrackSize.x =
          MulDiv(kMinimumWindowWidth, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
      limits->ptMinTrackSize.y =
          MulDiv(kMinimumWindowHeight, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
      return 0;
    }
    case WM_TIMER:
      if (settings != nullptr && w_param == kTrayRetryTimerId) {
        settings->AddTrayIcon();
        return 0;
      }
      return DefWindowProcW(hwnd, message, w_param, l_param);
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      if (settings != nullptr) settings->ForceRender();
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_DPICHANGED: {
      const auto* suggested = reinterpret_cast<const RECT*>(l_param);
      SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                   suggested->right - suggested->left, suggested->bottom - suggested->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      if (settings != nullptr) {
        settings->UpdateDpi(LOWORD(w_param));
        settings->ForceRender();
      }
      return 0;
    }
    case WM_SETTINGCHANGE:
      if (settings != nullptr) {
        settings->UpdateReducedMotion();
        settings->ForceRender();
      }
      return 0;
    case WM_SIZE:
      if (settings != nullptr && settings->swap_chain_ != nullptr && w_param != SIZE_MINIMIZED) {
        const UINT width = LOWORD(l_param);
        const UINT height = HIWORD(l_param);
        if (width == 0 || height == 0) return 0;
        settings->context_->OMSetRenderTargets(0, nullptr, nullptr);
        settings->context_->ClearState();
        settings->CleanupRenderTarget();
        const HRESULT resize_result =
            settings->swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (IsDeviceLostError(resize_result)) {
          settings->HandleDeviceLost();
        } else if (SUCCEEDED(resize_result)) {
          if (settings->CreateRenderTarget()) {
            settings->ApplyWindowShape(width, height);
            settings->ForceRender();
          } else if (settings->device_ != nullptr &&
                     FAILED(settings->device_->GetDeviceRemovedReason())) {
            settings->HandleDeviceLost();
          }
        } else if (FAILED(resize_result)) {
          // Keep the previous surface alive if the resize was rejected (for
          // example while Windows is changing the monitor topology).
          if (settings->device_ != nullptr && FAILED(settings->device_->GetDeviceRemovedReason())) {
            settings->HandleDeviceLost();
          } else if (settings->CreateRenderTarget()) {
            settings->ForceRender();
          }
        }
      }
      return 0;
    case WM_CLOSE:
      if (settings != nullptr) {
        settings->HandleCloseRequest();
      }
      return 0;
    case WM_NCHITTEST:
      return HTCLIENT;
    default:
      return DefWindowProcW(hwnd, message, w_param, l_param);
  }
}

std::vector<std::string> SettingsWindow::GetActiveApplications() {
  std::unordered_set<std::string> unique_apps;
  for (HWND hwnd : platform::EnumerateTopLevelWindows()) {
    if (platform::IsInterestingTopLevelWindow(hwnd)) {
      std::optional<std::string> exe_name = platform::GetWindowExecutableName(hwnd);
      if (exe_name.has_value() && !exe_name->empty()) {
        std::optional<std::string> normalized = NormalizeExecutableName(*exe_name);
        if (normalized.has_value()) {
          unique_apps.insert(*normalized);
        }
      }
    }
  }
  std::vector<std::string> result(unique_apps.begin(), unique_apps.end());
  std::sort(result.begin(), result.end());
  return result;
}

}  // namespace genie::app
