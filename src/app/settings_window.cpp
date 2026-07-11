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
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "common/debug_log.hpp"
#include "imgui.h"
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
constexpr ImU32 kBorderColor = settings_ui::kBorder;
constexpr ImU32 kTrackColor = IM_COL32(45, 45, 48, 255);
constexpr ImU32 kAccentColor = settings_ui::kActiveItem;
constexpr ImU32 kPrimaryTextColor = settings_ui::kText;
constexpr ImU32 kSecondaryTextColor = settings_ui::kMutedText;
constexpr float kSmallFontSize = 15.0f;
constexpr float kBodyFontSize = 17.0f;
constexpr float kTitleFontSize = 26.0f;
constexpr float kPageTitleTextSize = 26.0f;
constexpr float kPageSubtitleTextSize = 14.0f;
constexpr float kSectionTitleTextSize = 17.0f;
constexpr float kLabelTextSize = 15.0f;
constexpr float kValueTextSize = 14.0f;
constexpr float kHelperTextSize = 13.0f;
constexpr float kAppearanceDurationMs = 130.0f;
constexpr DWORD kInitialDeviceRecoveryDelayMs = 250;
constexpr DWORD kMaximumDeviceRecoveryDelayMs = 4000;

std::string SystemFontPath(const wchar_t* file_name) {
  wchar_t windows_directory[MAX_PATH]{};
  const UINT length = GetWindowsDirectoryW(windows_directory, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return {};

  std::wstring path(windows_directory, length);
  path += L"\\Fonts\\";
  path += file_name;

  const int utf8_length = WideCharToMultiByte(
      CP_UTF8, 0, path.data(), static_cast<int>(path.size()), nullptr, 0, nullptr, nullptr);
  if (utf8_length <= 0) return {};

  std::string utf8_path(static_cast<size_t>(utf8_length), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), utf8_path.data(),
                          utf8_length, nullptr, nullptr) != utf8_length) {
    return {};
  }
  return utf8_path;
}

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

ImVec4 Color(unsigned int hex, float alpha = 1.0f) {
  return ImVec4(((hex >> 16) & 0xff) / 255.0f, ((hex >> 8) & 0xff) / 255.0f, (hex & 0xff) / 255.0f,
                alpha);
}

ImU32 WithAlpha(ImU32 color, float alpha) {
  const auto clamped = static_cast<ImU32>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
  return (color & 0x00ffffffu) | (clamped << 24u);
}

ImU32 Blend(ImU32 from, ImU32 to, float amount) {
  const ImVec4 a = ImGui::ColorConvertU32ToFloat4(from);
  const ImVec4 b = ImGui::ColorConvertU32ToFloat4(to);
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount, a.z + (b.z - a.z) * amount,
             a.w + (b.w - a.w) * amount));
}

float Animate(ImGuiID id, float target, float speed = 16.0f) {
  ImGuiStorage* storage = ImGui::GetStateStorage();
  const float current = storage->GetFloat(id, target);
  const float blend = 1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime);
  const float next = current + (target - current) * blend;
  storage->SetFloat(id, next);
  return next;
}

void DelayedTooltip(const char* text, float scale) {
  if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) || ImGui::IsAnyItemActive()) return;
  ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(360.0f * scale, FLT_MAX));
  ImGui::BeginTooltip();
  ImGui::PushTextWrapPos(340.0f * scale);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
  ImGui::EndTooltip();
}

bool Toggle(const char* id, bool* value, float scale, float alpha) {
  const ImVec2 size(38.0f * scale, 22.0f * scale);
  const bool changed = ImGui::InvisibleButton(id, size);
  if (changed) *value = !*value;
  if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

  const float state = Animate(ImGui::GetID(id), *value ? 1.0f : 0.0f);
  const bool focused = ImGui::IsItemFocused();
  const float hover =
      Animate(ImGui::GetID("hover"), ImGui::IsItemHovered() || focused ? 1.0f : 0.0f, 20.0f);
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();

  const ImU32 off = IM_COL32(92, 92, 97, 255);
  const ImU32 on = settings_ui::kAppleGreen;
  const ImU32 track = Blend(off, on, state);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(min, max, WithAlpha(track, alpha), size.y * 0.5f);
  if (hover > 0.01f) {
    draw->AddRect(min, max, WithAlpha(IM_COL32_WHITE, hover * 0.22f * alpha), size.y * 0.5f, 0,
                  std::max(1.0f, scale));
  }
  if (focused) {
    draw->AddRect(ImVec2(min.x - 2.0f * scale, min.y - 2.0f * scale),
                  ImVec2(max.x + 2.0f * scale, max.y + 2.0f * scale),
                  WithAlpha(kAccentColor, alpha * 0.85f), size.y * 0.5f, 0, std::max(1.0f, scale));
  }
  const float thumb_radius = 9.0f * scale;
  const float thumb_x = min.x + 11.0f * scale + state * 16.0f * scale;
  draw->AddCircleFilled(ImVec2(thumb_x, min.y + size.y * 0.5f), thumb_radius,
                        WithAlpha(IM_COL32_WHITE, alpha), 24);
  return changed;
}

bool Slider(const char* id, float* value, float minimum, float maximum, float width, float scale,
            float alpha, float default_value, float step) {
  const float height = 20.0f * scale;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float track_inset = 5.0f * scale;
  const float start = origin.x + track_inset;
  const float end = origin.x + width - track_inset;
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool focused = ImGui::IsItemFocused();
  bool adjusted = false;
  if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
    *value += step * (ImGui::GetIO().KeyShift ? 10.0f : 1.0f) * ImGui::GetIO().MouseWheel;
    adjusted = true;
  }
  if (focused &&
      (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))) {
    const float direction = ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? 1.0f : -1.0f;
    *value += step * (ImGui::GetIO().KeyShift ? 10.0f : 1.0f) * direction;
    adjusted = true;
  }
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    *value = default_value;
    adjusted = true;
  }
  *value = std::clamp(*value, minimum, maximum);
  const bool active = ImGui::IsItemActive() || adjusted;
  if (active && !adjusted && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    const float ratio = std::clamp((ImGui::GetIO().MousePos.x - start) / (end - start), 0.0f, 1.0f);
    *value = minimum + (maximum - minimum) * ratio;
  }
  const float target = (*value - minimum) / (maximum - minimum);
  const ImGuiID slider_id = ImGui::GetID(id);
  float visual = target;
  if (active) {
    // A slider should stay attached to the cursor while dragging.  Keep the
    // eased value for external updates, but never let it trail live input.
    ImGui::GetStateStorage()->SetFloat(slider_id, target);
  } else {
    visual = Animate(slider_id, target, 19.0f);
  }
  const float hover =
      Animate(ImGui::GetID("slider_hover"), ImGui::IsItemHovered() || active ? 1.0f : 0.0f);
  const float track_y = origin.y + height * 0.5f;
  const float knob_x = start + (end - start) * visual;
  ImDrawList* draw = ImGui::GetWindowDrawList();

  const float track_h = 4.0f * scale;
  draw->AddRectFilled(ImVec2(start, track_y - track_h * 0.5f),
                      ImVec2(end, track_y + track_h * 0.5f), WithAlpha(kTrackColor, alpha),
                      track_h * 0.5f);
  draw->AddRectFilled(ImVec2(start, track_y - track_h * 0.5f),
                      ImVec2(knob_x, track_y + track_h * 0.5f), WithAlpha(kAccentColor, alpha),
                      track_h * 0.5f);

  const float knob_radius = (6.5f + hover * 0.45f) * scale;
  draw->AddCircleFilled(ImVec2(knob_x, track_y + scale), knob_radius + scale,
                        WithAlpha(IM_COL32(0, 0, 0, 75), alpha), 24);
  draw->AddCircleFilled(ImVec2(knob_x, track_y), knob_radius, WithAlpha(kPrimaryTextColor, alpha),
                        24);
  return active;
}

bool CompactButton(const char* id, const char* label, const ImVec2& size, ImFont* font, float scale,
                   float alpha) {
  const ImVec2 min = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool focused = ImGui::IsItemFocused();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  const bool held = ImGui::IsItemActive();
  const float hover = Animate(ImGui::GetID(id), hovered || focused ? 1.0f : 0.0f, 22.0f);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  ImU32 background = Blend(kTrackColor, kBorderColor, hover);
  if (held) background = Blend(background, kAccentColor, 0.35f);
  draw->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), WithAlpha(background, alpha),
                      3.0f * scale);
  draw->AddRect(min, ImVec2(min.x + size.x, min.y + size.y),
                WithAlpha(Blend(kBorderColor, kAccentColor, focused ? 0.9f : hover * 0.45f), alpha),
                5.0f * scale, 0, std::max(1.0f, scale));
  const ImVec2 text_size = font->CalcTextSizeA(kValueTextSize * scale, FLT_MAX, 0.0f, label);
  draw->AddText(
      font, kValueTextSize * scale,
      ImVec2(min.x + (size.x - text_size.x) * 0.5f, min.y + (size.y - text_size.y) * 0.5f),
      WithAlpha(kPrimaryTextColor, alpha), label);
  return pressed;
}

bool LinkButton(const char* id, bool linked, float scale, float alpha) {
  const ImVec2 size(24.0f * scale, 20.0f * scale);
  const ImVec2 min = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool focused = ImGui::IsItemFocused();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  const float hover = Animate(ImGui::GetID(id), hovered || focused ? 1.0f : 0.0f, 22.0f);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImU32 background =
      linked ? Blend(kTrackColor, kAccentColor, 0.75f) : Blend(kTrackColor, kBorderColor, hover);
  draw->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), WithAlpha(background, alpha),
                      3.0f * scale);
  const ImU32 icon = WithAlpha(kPrimaryTextColor, alpha);
  const float radius = 3.4f * scale;
  const ImVec2 left(min.x + 9.0f * scale, min.y + 10.0f * scale);
  const ImVec2 right(min.x + 15.0f * scale, min.y + 10.0f * scale);
  draw->AddCircle(left, radius, icon, 12, 1.4f * scale);
  draw->AddCircle(right, radius, icon, 12, 1.4f * scale);
  draw->AddLine(ImVec2(left.x + 1.5f * scale, left.y), ImVec2(right.x - 1.5f * scale, right.y),
                icon, 1.4f * scale);
  if (hovered) ImGui::SetTooltip(linked ? "Unlink speeds" : "Link speeds");
  return pressed;
}

bool RadioChoice(const char* id, const char* label, bool selected, ImFont* font, float scale,
                 float alpha) {
  const ImVec2 min = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(id, ImVec2(260.0f * scale, 22.0f * scale));
  const bool hovered = ImGui::IsItemHovered();
  const bool focused = ImGui::IsItemFocused();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 center(min.x + 8.0f * scale, min.y + 11.0f * scale);
  draw->AddCircleFilled(center, 7.0f * scale,
                        WithAlpha(selected ? kAccentColor : IM_COL32(58, 58, 62, 255), alpha), 20);
  draw->AddCircle(center, 7.0f * scale,
                  WithAlpha(selected ? IM_COL32(45, 155, 255, 255)
                                     : (hovered || focused ? IM_COL32(170, 170, 175, 255)
                                                           : IM_COL32(108, 108, 112, 255)),
                            alpha),
                  20, std::max(1.0f, scale));
  if (selected) {
    draw->AddCircleFilled(center, 2.6f * scale, WithAlpha(IM_COL32_WHITE, alpha), 14);
  }
  draw->AddText(font, 15.0f * scale, ImVec2(min.x + 24.0f * scale, min.y + 1.0f * scale),
                WithAlpha(kPrimaryTextColor, alpha), label);
  return pressed;
}

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
    LinkCallback link_callback, AdaptiveDurationCallback adaptive_duration_callback,
    FullscreenBehaviorCallback fullscreen_behavior_callback,
    PowerBehaviorCallback power_behavior_callback, EasingCallback easing_callback,
    AnimationStyleCallback animation_style_callback, StrengthCallback strength_callback,
    FadeCallback fade_callback, TargetIndicatorCallback target_indicator_callback,
    WindowsAnimationPreferenceCallback windows_animation_preference_callback,
    CloseBehaviorCallback close_behavior_callback, StartupCallback startup_callback,
    ExclusionCallback exclusion_callback, PauseCallback pause_callback,
    HotkeyUpdateCallback hotkey_update_callback, HotkeyActionCallback hotkey_action_callback,
    DiagnosticsCallback diagnostics_callback, DiagnosticsActionCallback diagnostics_action_callback,
    HealCallback heal_callback, ExitCallback exit_callback) {
  toggle_callback_ = std::move(toggle_callback);
  speed_callback_ = std::move(speed_callback);
  link_callback_ = std::move(link_callback);
  adaptive_duration_callback_ = std::move(adaptive_duration_callback);
  fullscreen_behavior_callback_ = std::move(fullscreen_behavior_callback);
  power_behavior_callback_ = std::move(power_behavior_callback);
  easing_callback_ = std::move(easing_callback);
  animation_style_callback_ = std::move(animation_style_callback);
  strength_callback_ = std::move(strength_callback);
  fade_callback_ = std::move(fade_callback);
  target_indicator_callback_ = std::move(target_indicator_callback);
  windows_animation_preference_callback_ = std::move(windows_animation_preference_callback);
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
  AddTrayIcon();
  return true;
}

bool SettingsWindow::AddTrayIcon() {
  if (hwnd_ == nullptr) return false;
  NOTIFYICONDATAW tray_icon{};
  tray_icon.cbSize = sizeof(tray_icon);
  tray_icon.hWnd = hwnd_;
  tray_icon.uID = kTrayIconId;
  tray_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  tray_icon.uCallbackMessage = kTrayMessage;
  tray_icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
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

void SettingsWindow::Shutdown() {
  FlushPendingSpeedSave();
  CloseAnimationPreview();
  if (hwnd_ != nullptr && tray_icon_added_) {
    NOTIFYICONDATAW tray_icon{};
    tray_icon.cbSize = sizeof(tray_icon);
    tray_icon.hWnd = hwnd_;
    tray_icon.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &tray_icon);
    tray_icon_added_ = false;
  }
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

struct EmbeddedFont {
  void* data = nullptr;
  int size = 0;
};

EmbeddedFont LoadEmbeddedFont(int resource_id) {
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
  if (resource == nullptr) return {};
  HGLOBAL loaded = LoadResource(instance, resource);
  if (loaded == nullptr) return {};
  const DWORD size = SizeofResource(instance, resource);
  void* data = LockResource(loaded);
  if (data == nullptr || size == 0 || size > static_cast<DWORD>(INT_MAX)) return {};
  return {data, static_cast<int>(size)};
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
      HBRUSH background_brush = CreateSolidBrush(RGB(30, 30, 33));
      FillRect(dc, &client, background_brush);
      DeleteObject(background_brush);

      RECT accent = client;
      accent.bottom = accent.top + 6;
      HBRUSH accent_brush = CreateSolidBrush(RGB(0, 122, 204));
      FillRect(dc, &accent, accent_brush);
      DeleteObject(accent_brush);

      const int font_height = -MulDiv(42, GetDeviceCaps(dc, LOGPIXELSY), 72);
      HFONT font = CreateFontW(font_height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
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
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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
  } else {
    ShowWindow(hwnd_, SW_HIDE);
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
      link_speeds_ != settings.link_speeds || adaptive_duration_ != settings.adaptive_duration ||
      disable_animations_fullscreen_ != settings.disable_animations_fullscreen ||
      reduce_effects_on_battery_ != settings.reduce_effects_on_battery ||
      disable_effects_battery_saver_ != settings.disable_effects_battery_saver ||
      minimize_easing_ != settings.minimize_easing || restore_easing_ != settings.restore_easing ||
      animation_style_ != settings.animation_style ||
      std::abs(genie_strength_ - settings.genie_strength) > 0.0001f ||
      fade_strength_ != settings.fade_strength ||
      show_target_indicator_ != settings.show_target_indicator ||
      follow_windows_animation_preference_ != settings.follow_windows_animation_preference ||
      close_behavior_ != settings.close_behavior || run_at_startup_ != settings.run_at_startup ||
      start_minimized_ != settings.start_minimized ||
      excluded_applications_ != settings.excluded_applications || hotkeys_ != settings.hotkeys;
  is_enabled_ = settings.enabled;
  minimize_duration_seconds_ = settings.minimize_duration;
  restore_duration_seconds_ = settings.restore_duration;
  link_speeds_ = settings.link_speeds;
  adaptive_duration_ = settings.adaptive_duration;
  disable_animations_fullscreen_ = settings.disable_animations_fullscreen;
  reduce_effects_on_battery_ = settings.reduce_effects_on_battery;
  disable_effects_battery_saver_ = settings.disable_effects_battery_saver;
  minimize_easing_ = settings.minimize_easing;
  restore_easing_ = settings.restore_easing;
  animation_style_ = settings.animation_style;
  genie_strength_ = settings.genie_strength;
  fade_strength_ = settings.fade_strength;
  show_target_indicator_ = settings.show_target_indicator;
  follow_windows_animation_preference_ = settings.follow_windows_animation_preference;
  close_behavior_ = settings.close_behavior;
  run_at_startup_ = settings.run_at_startup;
  start_minimized_ = settings.start_minimized;
  excluded_applications_ = settings.excluded_applications;
  hotkeys_ = settings.hotkeys;
  if (enabled_changed) UpdateTrayTooltip();
  if (changed) ForceRender();
}

void SettingsWindow::UpdateTrayTooltip() {
  if (hwnd_ == nullptr) return;
  if (!tray_icon_added_) {
    AddTrayIcon();
    return;
  }
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
  if (saved) {
    persistence_error_.clear();
    save_feedback_ = "Saved";
    save_feedback_until_ms_ = GetTickCount64() + 1800;
    save_feedback_error_ = false;
  } else {
    persistence_error_ = "Could not save settings. Check folder permissions or disk space.";
    save_feedback_ = "Could not save settings";
    save_feedback_until_ms_ = GetTickCount64() + 6000;
    save_feedback_error_ = true;
    LogDebug(L"Settings", L"Settings window could not persist the requested change");
  }
  ForceRender();
}

void SettingsWindow::HandleCloseRequest() {
  if (close_behavior_ == "tray") {
    Show(false);
  } else if (exit_callback_) {
    exit_callback_();
  } else {
    Show(false);
  }
}

bool SettingsWindow::CreateRenderWindow(HINSTANCE instance) {
  WNDCLASSEXW window_class{sizeof(window_class)};
  window_class.style = CS_CLASSDC;
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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

  ImFontConfig config{};
  config.OversampleH = 3;
  config.OversampleV = 2;
  config.PixelSnapH = false;
  const float scale = ui_scale_;
  config.FontDataOwnedByAtlas = false;
  const EmbeddedFont regular_resource = LoadEmbeddedFont(IDR_SFPRO_REGULAR);
  const EmbeddedFont bold_resource = LoadEmbeddedFont(IDR_SFPRO_BOLD);
  const auto add_embedded = [&config](const EmbeddedFont& font, float size) -> ImFont* {
    return font.data == nullptr
               ? nullptr
               : ImGui::GetIO().Fonts->AddFontFromMemoryTTF(font.data, font.size, size, &config);
  };
  font_small_ = add_embedded(regular_resource, kSmallFontSize * scale);
  font_body_ = add_embedded(regular_resource, kBodyFontSize * scale);
  font_medium_ = add_embedded(bold_resource, kSectionTitleTextSize * scale);
  font_title_ = add_embedded(bold_resource, kTitleFontSize * scale);

  const std::string regular_font = SystemFontPath(L"segoeui.ttf");
  const std::string semibold_font = SystemFontPath(L"seguisb.ttf");
  const std::string title_font = SystemFontPath(L"segoeuib.ttf");
  ImFontConfig file_config = config;
  file_config.FontDataOwnedByAtlas = true;
  const auto add_font = [&file_config](const std::string& path, float size) -> ImFont* {
    return path.empty()
               ? nullptr
               : ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size, &file_config);
  };
  if (font_small_ == nullptr) font_small_ = add_font(regular_font, kSmallFontSize * scale);
  if (font_body_ == nullptr) font_body_ = add_font(regular_font, kBodyFontSize * scale);
  if (font_medium_ == nullptr)
    font_medium_ = add_font(semibold_font, kSectionTitleTextSize * scale);
  if (font_title_ == nullptr) font_title_ = add_font(title_font, kTitleFontSize * scale);
  if (font_body_ == nullptr) font_body_ = io.Fonts->AddFontDefault();
  if (font_small_ == nullptr) font_small_ = font_body_;
  if (font_medium_ == nullptr) font_medium_ = font_body_;
  if (font_title_ == nullptr) font_title_ = font_medium_;
  if (imgui_dx11_ready_) ImGui_ImplDX11_CreateDeviceObjects();
}

void SettingsWindow::ApplyWindowShape(int width, int height) {
  if (IsZoomed(hwnd_) != FALSE) {
    SetWindowRgn(hwnd_, nullptr, TRUE);
    return;
  }
  const int radius = std::max(1, static_cast<int>(24.0f * ui_scale_));
  HRGN rounded_region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius);
  if (rounded_region != nullptr && SetWindowRgn(hwnd_, rounded_region, TRUE) == 0) {
    DeleteObject(rounded_region);
  }
}

void SettingsWindow::UpdateDpi(UINT dpi) {
  if (dpi == 0 || dpi == current_dpi_) return;
  RebuildFonts(dpi);
  ApplyStyle();
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
  if (!is_animating && !is_active && !preview_active_ && !feedback_active && !render_requested_) {
    return;
  }
  render_requested_ = false;

  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  RenderContents();
  ImGui::Render();
  // Transparent clear is required for DWM's native Mica backdrop. The UI
  // paints the opaque main pane itself and leaves the sidebar translucent.
  constexpr float clear_color[] = {0.0f, 0.0f, 0.0f, 0.0f};
  context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);
  context_->ClearRenderTargetView(render_target_view_.Get(), clear_color);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  const HRESULT present_result = swap_chain_->Present(0, 0);
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
         render_requested_;
}

void SettingsWindow::RenderContents() {
  const ImVec2 window_size = ImGui::GetIO().DisplaySize;
  const float scale = ui_scale_;
  const auto px = [scale](float value) { return value * scale; };
  const float elapsed = static_cast<float>(GetTickCount64() - shown_at_ms_);
  const float appearance =
      SystemUiAnimationsEnabled() ? 1.0f - std::exp(-elapsed / kAppearanceDurationMs) : 1.0f;
  const float content_alpha = std::clamp(appearance, 0.0f, 1.0f);
  const float y_offset = px(6.0f) * (1.0f - content_alpha);

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
  const float rounding = px(settings_ui::Metrics::kWindowRounding);
  settings_ui::DrawGradientShadow(
      draw, window_origin, ImVec2(window_origin.x + window_size.x, window_origin.y + window_size.y),
      rounding, 0.35f * content_alpha, scale);
  draw->AddRectFilled(window_point(sidebar_width, 0.0f), window_point(window_size.x, window_size.y),
                      WithAlpha(settings_ui::kMainBackground, content_alpha), rounding,
                      ImDrawFlags_RoundCornersRight);
  const int sidebar_alpha = SystemTransparencyEnabled() ? 180 : 255;
  draw->AddRectFilled(window_origin, window_point(sidebar_width, window_size.y),
                      WithAlpha(IM_COL32(40, 42, 40, 255),
                                content_alpha * static_cast<float>(sidebar_alpha) / 255.0f),
                      rounding, ImDrawFlags_RoundCornersLeft);
  draw->AddLine(window_point(sidebar_width, 0.0f), window_point(sidebar_width, window_size.y),
                WithAlpha(IM_COL32(0, 0, 0, 100), content_alpha));

  switch (settings_ui::DrawTrafficLights(window_origin, scale, content_alpha)) {
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

  struct PageEntry {
    Page page;
    const char* label;
    settings_ui::Icon icon;
    bool section_gap;
  };
  constexpr std::array pages = {
      PageEntry{Page::kGeneral, "General", settings_ui::Icon::kGeneral, false},
      PageEntry{Page::kAnimation, "Animation", settings_ui::Icon::kAnimation, false},
      PageEntry{Page::kApplications, "Applications", settings_ui::Icon::kApplications, false},
      PageEntry{Page::kWindowsIntegration, "Windows & System", settings_ui::Icon::kWindows, true},
      PageEntry{Page::kHotkeys, "Hotkeys", settings_ui::Icon::kHotkeys, false},
      PageEntry{Page::kDiagnostics, "Diagnostics & Repair", settings_ui::Icon::kDiagnostics, false},
      PageEntry{Page::kAbout, "About", settings_ui::Icon::kAbout, false},
  };
  float navigation_y = px(settings_ui::Metrics::kNavigationY);
  for (size_t index = 0; index < pages.size(); ++index) {
    const PageEntry& entry = pages[index];
    if (entry.section_gap) navigation_y += px(16.0f);
    const ImVec2 item_position = window_point(px(12.5f), navigation_y);
    const std::string id = std::format("##settings_page_{}", index);
    if (settings_ui::SidebarItem(id.c_str(), entry.label, entry.icon, selected_page_ == entry.page,
                                 item_position,
                                 ImVec2(px(settings_ui::Metrics::kSidebarContentWidth),
                                        px(settings_ui::Metrics::kNavigationRowHeight)),
                                 font_body_, scale, content_alpha)) {
      FlushPendingSpeedSave();
      selected_page_ = entry.page;
      reset_page_scroll_ = true;
    }
    navigation_y +=
        px(settings_ui::Metrics::kNavigationRowHeight + settings_ui::Metrics::kNavigationSpacing);
  }

  ImGui::SetCursorPos(ImVec2(sidebar_width, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ImGui::ColorConvertU32ToFloat4(settings_ui::kMainBackground));
  float page_content_bottom = 386.0f;
  switch (selected_page_) {
    case Page::kGeneral:
      page_content_bottom = 402.0f;
      break;
    case Page::kAnimation:
      page_content_bottom = 782.0f;
      break;
    case Page::kApplications:
      page_content_bottom = 730.0f;
      break;
    case Page::kHotkeys:
      page_content_bottom = 326.0f;
      break;
    case Page::kWindowsIntegration:
      page_content_bottom = 364.0f;
      break;
    case Page::kDiagnostics:
      page_content_bottom = 706.0f;
      break;
    case Page::kAbout:
      page_content_bottom = 328.0f;
      break;
  }
  const float page_width = window_size.x - sidebar_width - px(settings_ui::Metrics::kScrollGutter);
  const bool page_scrollable =
      px(page_content_bottom + settings_ui::Metrics::kScrollBottomPadding) > window_size.y;
  const ImGuiWindowFlags page_flags =
      page_scrollable ? ImGuiWindowFlags_AlwaysVerticalScrollbar : ImGuiWindowFlags_None;
  ImGui::BeginChild("##settings_page", ImVec2(page_width, window_size.y), ImGuiChildFlags_None,
                    page_flags);
  if (reset_page_scroll_) {
    ImGui::SetScrollY(0.0f);
    reset_page_scroll_ = false;
  }
  draw = ImGui::GetWindowDrawList();
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 origin = ImGui::GetWindowPos();
  const auto point = [&origin](float x, float y) {
    return ImVec2(origin.x + x, origin.y + y - ImGui::GetScrollY());
  };

  if (selected_page_ == Page::kGeneral) {
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(76.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(146.0f) + y_offset), scale,
        content_alpha);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(166.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(266.0f) + y_offset), scale,
        content_alpha);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(286.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(402.0f) + y_offset), scale,
        content_alpha);
    draw->AddText(font_title_, px(kPageTitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(20.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "General");

    draw->AddText(font_small_, px(kPageSubtitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(51.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha),
                  "Basic app behavior and startup options");

    // Row 1: Animations (y = 84)
    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(88.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Effect");
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(116.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Enable Genie animations");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(38.0f),
                               px(111.0f) + y_offset));
    const bool previous_enabled = is_enabled_;
    if (Toggle("##animations_enabled", &is_enabled_, scale, content_alpha) && toggle_callback_) {
      const bool saved = toggle_callback_(is_enabled_);
      if (!saved) is_enabled_ = previous_enabled;
      RecordSaveResult(saved);
    }

    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(177.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Closing settings");
    ImGui::SetCursorPos(ImVec2(px(settings_ui::Metrics::kContentInset), px(202.0f) + y_offset));
    if (RadioChoice("##close_exit", "Exit Genie Effect", close_behavior_ == "exit", font_body_,
                    scale, content_alpha)) {
      const std::string previous_close_behavior = close_behavior_;
      close_behavior_ = "exit";
      const bool saved = !close_behavior_callback_ || close_behavior_callback_(close_behavior_);
      if (!saved) close_behavior_ = previous_close_behavior;
      RecordSaveResult(saved);
    }
    ImGui::SetCursorPos(ImVec2(px(settings_ui::Metrics::kContentInset), px(232.0f) + y_offset));
    if (RadioChoice("##close_tray", "Minimize to system tray", close_behavior_ == "tray",
                    font_body_, scale, content_alpha)) {
      const std::string previous_close_behavior = close_behavior_;
      close_behavior_ = "tray";
      const bool saved = !close_behavior_callback_ || close_behavior_callback_(close_behavior_);
      if (!saved) close_behavior_ = previous_close_behavior;
      RecordSaveResult(saved);
    }

    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(297.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Startup");
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(324.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Start with Windows");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(38.0f),
                               px(321.0f) + y_offset));
    bool proposed_run_at_startup = run_at_startup_;
    if (Toggle("##run_at_startup", &proposed_run_at_startup, scale, content_alpha)) {
      const bool saved =
          !startup_callback_ || startup_callback_(proposed_run_at_startup, start_minimized_);
      if (saved) {
        run_at_startup_ = proposed_run_at_startup;
      }
      RecordSaveResult(saved);
    }
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(366.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Start minimized to tray");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(38.0f),
                               px(363.0f) + y_offset));
    bool proposed_start_minimized = start_minimized_;
    if (Toggle("##start_minimized", &proposed_start_minimized, scale, content_alpha)) {
      const bool saved =
          !startup_callback_ || startup_callback_(run_at_startup_, proposed_start_minimized);
      if (saved) {
        start_minimized_ = proposed_start_minimized;
      }
      RecordSaveResult(saved);
    }
  }

  if (selected_page_ == Page::kAnimation) {
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(76.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(238.0f) + y_offset), scale,
        content_alpha);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(258.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(338.0f) + y_offset), scale,
        content_alpha);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(358.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(566.0f) + y_offset), scale,
        content_alpha);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(586.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(782.0f) + y_offset), scale,
        content_alpha);
    draw->AddText(font_title_, px(kPageTitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(20.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Animation");
    draw->AddText(font_small_, px(kPageSubtitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(51.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha),
                  "Timing, motion and visual appearance");

    // Row 2: Minimize Duration (y = 92)
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(92.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha), "Minimize Duration");

    // Value text on the right
    const std::string min_duration_text = std::format("{:.2f}s", minimize_duration_seconds_);
    const ImVec2 min_duration_size =
        font_small_->CalcTextSizeA(px(kValueTextSize), FLT_MAX, 0.0f, min_duration_text.c_str());
    const ImVec2 min_value_pos(
        size.x - px(settings_ui::Metrics::kContentInset) - min_duration_size.x,
        px(92.0f) + y_offset);
    if (numeric_edit_field_ == 0) {
      ImGui::SetCursorPos(ImVec2(size.x - px(110.0f), px(86.0f) + y_offset));
      ImGui::SetNextItemWidth(px(68.0f));
      ImGui::SetKeyboardFocusHere();
      const bool apply = ImGui::InputFloat("##min_duration_input", &numeric_edit_value_, 0, 0,
                                           "%.2f", ImGuiInputTextFlags_EnterReturnsTrue);
      if (apply) {
        const float value = std::clamp(numeric_edit_value_, kMinimumAnimationDurationSeconds,
                                       kMaximumAnimationDurationSeconds);
        minimize_duration_seconds_ = value;
        if (link_speeds_) restore_duration_seconds_ = value;
        RecordSaveResult(!speed_callback_ || speed_callback_(minimize_duration_seconds_,
                                                             restore_duration_seconds_, true));
        numeric_edit_field_ = -1;
      } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        numeric_edit_field_ = -1;
      }
    } else {
      draw->AddText(font_small_, px(kValueTextSize), point(min_value_pos.x, min_value_pos.y),
                    WithAlpha(kSecondaryTextColor, content_alpha), min_duration_text.c_str());
      ImGui::SetCursorPos(min_value_pos);
      ImGui::InvisibleButton("##edit_min_duration", ImVec2(min_duration_size.x, px(20.0f)));
      if (ImGui::IsItemClicked()) {
        numeric_edit_field_ = 0;
        numeric_edit_value_ = minimize_duration_seconds_;
      }
    }

    // Slider (Right-aligned next to value text)
    const float slider_w = px(120.0f);
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) -
                                   min_duration_size.x - slider_w - px(8.0f),
                               px(92.0f) + y_offset));
    float updated_min_duration = minimize_duration_seconds_;
    const bool minimize_slider_active =
        Slider("##min_duration", &updated_min_duration, kMinimumAnimationDurationSeconds,
               kMaximumAnimationDurationSeconds, slider_w, scale, content_alpha, 0.70f, 0.01f);
    if (minimize_slider_active &&
        std::abs(updated_min_duration - minimize_duration_seconds_) > 0.0001f) {
      float delta = updated_min_duration - minimize_duration_seconds_;
      if (link_speeds_) {
        delta = std::clamp(delta, kMinimumAnimationDurationSeconds - restore_duration_seconds_,
                           kMaximumAnimationDurationSeconds - restore_duration_seconds_);
        restore_duration_seconds_ += delta;
      }
      minimize_duration_seconds_ += delta;
      minimize_slider_dirty_ = true;
      if (speed_callback_) {
        speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, false);
      }
    }
    if (minimize_slider_active_ && !minimize_slider_active && minimize_slider_dirty_) {
      bool saved = true;
      if (speed_callback_) {
        saved = speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, true);
      }
      RecordSaveResult(saved);
      if (saved) minimize_slider_dirty_ = false;
    }
    minimize_slider_active_ = minimize_slider_active;

    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(24.0f),
                               px(112.0f) + y_offset));
    if (LinkButton("##link_speeds", link_speeds_, scale, content_alpha)) {
      const bool previous_link_speeds = link_speeds_;
      link_speeds_ = !link_speeds_;
      const bool saved = !link_callback_ || link_callback_(link_speeds_);
      if (!saved) link_speeds_ = previous_link_speeds;
      RecordSaveResult(saved);
    }

    // Row 3: Restore Duration (y = 132)
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(132.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha), "Restore Duration");

    // Value text on the right
    const std::string restore_duration_text = std::format("{:.2f}s", restore_duration_seconds_);
    const ImVec2 restore_duration_size = font_small_->CalcTextSizeA(
        px(kValueTextSize), FLT_MAX, 0.0f, restore_duration_text.c_str());
    const ImVec2 restore_value_pos(
        size.x - px(settings_ui::Metrics::kContentInset) - restore_duration_size.x,
        px(132.0f) + y_offset);
    if (numeric_edit_field_ == 1) {
      ImGui::SetCursorPos(ImVec2(size.x - px(110.0f), px(126.0f) + y_offset));
      ImGui::SetNextItemWidth(px(68.0f));
      ImGui::SetKeyboardFocusHere();
      const bool apply = ImGui::InputFloat("##restore_duration_input", &numeric_edit_value_, 0, 0,
                                           "%.2f", ImGuiInputTextFlags_EnterReturnsTrue);
      if (apply) {
        const float value = std::clamp(numeric_edit_value_, kMinimumAnimationDurationSeconds,
                                       kMaximumAnimationDurationSeconds);
        restore_duration_seconds_ = value;
        if (link_speeds_) minimize_duration_seconds_ = value;
        RecordSaveResult(!speed_callback_ || speed_callback_(minimize_duration_seconds_,
                                                             restore_duration_seconds_, true));
        numeric_edit_field_ = -1;
      } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        numeric_edit_field_ = -1;
      }
    } else {
      draw->AddText(font_small_, px(kValueTextSize),
                    point(restore_value_pos.x, restore_value_pos.y),
                    WithAlpha(kSecondaryTextColor, content_alpha), restore_duration_text.c_str());
      ImGui::SetCursorPos(restore_value_pos);
      ImGui::InvisibleButton("##edit_restore_duration", ImVec2(restore_duration_size.x, px(20.0f)));
      if (ImGui::IsItemClicked()) {
        numeric_edit_field_ = 1;
        numeric_edit_value_ = restore_duration_seconds_;
      }
    }

    // Slider (Right-aligned next to value text)
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) -
                                   restore_duration_size.x - slider_w - px(8.0f),
                               px(132.0f) + y_offset));
    float updated_restore_duration = restore_duration_seconds_;
    const bool restore_slider_active =
        Slider("##restore_duration", &updated_restore_duration, kMinimumAnimationDurationSeconds,
               kMaximumAnimationDurationSeconds, slider_w, scale, content_alpha, 0.70f, 0.01f);
    if (restore_slider_active &&
        std::abs(updated_restore_duration - restore_duration_seconds_) > 0.0001f) {
      float delta = updated_restore_duration - restore_duration_seconds_;
      if (link_speeds_) {
        delta = std::clamp(delta, kMinimumAnimationDurationSeconds - minimize_duration_seconds_,
                           kMaximumAnimationDurationSeconds - minimize_duration_seconds_);
        minimize_duration_seconds_ += delta;
      }
      restore_duration_seconds_ += delta;
      restore_slider_dirty_ = true;
      if (speed_callback_) {
        speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, false);
      }
    }
    if (restore_slider_active_ && !restore_slider_active && restore_slider_dirty_) {
      bool saved = true;
      if (speed_callback_) {
        saved = speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, true);
      }
      RecordSaveResult(saved);
      if (saved) restore_slider_dirty_ = false;
    }
    restore_slider_active_ = restore_slider_active;

    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(174.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha), "Presets");
    constexpr std::array presets = {
        std::tuple{"Snappy", 0.30f, 0.25f},
        std::tuple{"Balanced", 0.55f, 0.45f},
        std::tuple{"Smooth", 0.80f, 0.70f},
        std::tuple{"Cinematic", 1.20f, 1.00f},
    };
    const float preset_width = px(78.0f);
    const float preset_gap = px(6.0f);
    float preset_x = px(settings_ui::Metrics::kContentInset);
    for (size_t i = 0; i < presets.size(); ++i) {
      const auto& [label, minimize, restore] = presets[i];
      ImGui::SetCursorPos(ImVec2(preset_x, px(196.0f) + y_offset));
      const std::string id = std::format("##preset_{}", i);
      if (CompactButton(id.c_str(), label, ImVec2(preset_width, px(28.0f)), font_body_, scale,
                        content_alpha)) {
        minimize_duration_seconds_ = minimize;
        restore_duration_seconds_ = restore;
        minimize_slider_dirty_ = false;
        restore_slider_dirty_ = false;
        if (speed_callback_) {
          RecordSaveResult(
              speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, true));
        }
      }
      preset_x += preset_width + preset_gap;
    }

    ImGui::SetCursorPos(
        ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(72.0f), px(18.0f) + y_offset));
    if (CompactButton("##reset_speeds", "Reset", ImVec2(px(72.0f), px(24.0f)), font_body_, scale,
                      content_alpha)) {
      minimize_duration_seconds_ = kDefaultMinimizeDuration;
      restore_duration_seconds_ = kDefaultRestoreDuration;
      minimize_slider_dirty_ = false;
      restore_slider_dirty_ = false;
      if (speed_callback_) {
        RecordSaveResult(
            speed_callback_(minimize_duration_seconds_, restore_duration_seconds_, true));
      }
    }

    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(274.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Test the animation");
    draw->AddText(font_small_, px(kHelperTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(300.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha),
                  "Opens a real window, minimizes it, restores it, then closes it.");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(112.0f),
                               px(270.0f) + y_offset));
    if (CompactButton("##preview", preview_active_ ? "Previewing..." : "Start preview",
                      ImVec2(px(112.0f), px(30.0f)), font_body_, scale, content_alpha) &&
        !preview_active_) {
      StartAnimationPreview();
    }

    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(374.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Motion");
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(406.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha),
                  "Adapt duration to animation distance");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(38.0f),
                               px(403.0f) + y_offset));
    const bool previous_adaptive_duration = adaptive_duration_;
    if (Toggle("##adaptive_duration", &adaptive_duration_, scale, content_alpha)) {
      const bool saved =
          !adaptive_duration_callback_ || adaptive_duration_callback_(adaptive_duration_);
      if (!saved) adaptive_duration_ = previous_adaptive_duration;
      RecordSaveResult(saved);
    }
    DelayedTooltip("Short trips finish faster; the selected duration remains the maximum.", scale);

    constexpr std::array easing_names = {
        "Linear", "Ease In", "Ease Out", "Ease In Out", "Cubic", "Back", "Elastic",
    };
    constexpr std::array style_names = {
        "Classic Genie", "Soft", "Snappy", "Elastic", "Linear",
    };
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(450.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Animation style");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(150.0f),
                               px(446.0f) + y_offset));
    ImGui::SetNextItemWidth(px(150.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(8.0f), px(5.0f)));
    if (ImGui::BeginCombo("##animation_style", animation_style_.c_str())) {
      for (const char* name : style_names) {
        const bool selected = animation_style_ == name;
        if (ImGui::Selectable(name, selected)) {
          const std::string previous = animation_style_;
          animation_style_ = name;
          const bool saved =
              !animation_style_callback_ || animation_style_callback_(animation_style_);
          if (!saved) animation_style_ = previous;
          RecordSaveResult(saved);
        }
        if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    const auto easing_combo = [&](const char* id, const char* label, std::string* value,
                                  float row_y, bool minimize) {
      draw->AddText(font_body_, px(kLabelTextSize),
                    point(px(settings_ui::Metrics::kContentInset), px(row_y) + y_offset),
                    WithAlpha(kPrimaryTextColor, content_alpha), label);
      ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(150.0f),
                                 px(row_y - 4.0f) + y_offset));
      ImGui::SetNextItemWidth(px(150.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(8.0f), px(5.0f)));
      if (ImGui::BeginCombo(id, value->c_str())) {
        for (const char* name : easing_names) {
          const bool selected = *value == name;
          if (ImGui::Selectable(name, selected)) {
            const std::string previous = *value;
            *value = name;
            const bool saved =
                !easing_callback_ || easing_callback_(minimize ? *value : minimize_easing_,
                                                      minimize ? restore_easing_ : *value);
            if (!saved) *value = previous;
            RecordSaveResult(saved);
          }
          if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      ImGui::PopStyleVar();
    };
    easing_combo("##minimize_easing", "Minimize easing", &minimize_easing_, 494.0f, true);
    easing_combo("##restore_easing", "Restore easing", &restore_easing_, 538.0f, false);

    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(602.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Visual effects");
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(634.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Genie Strength");
    const std::string strength_text = std::format("{:.0f}%", genie_strength_ * 100.0f);
    const ImVec2 strength_value_pos(size.x - px(90.0f), px(634.0f) + y_offset);
    if (numeric_edit_field_ == 2) {
      ImGui::SetCursorPos(ImVec2(size.x - px(82.0f), px(628.0f) + y_offset));
      ImGui::SetNextItemWidth(px(64.0f));
      ImGui::SetKeyboardFocusHere();
      const bool apply = ImGui::InputFloat("##strength_input", &numeric_edit_value_, 0, 0, "%.2f",
                                           ImGuiInputTextFlags_EnterReturnsTrue);
      if (apply) {
        genie_strength_ = std::clamp(numeric_edit_value_, 0.25f, 1.0f);
        RecordSaveResult(!strength_callback_ || strength_callback_(genie_strength_, true));
        numeric_edit_field_ = -1;
      } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        numeric_edit_field_ = -1;
      }
    } else {
      draw->AddText(font_small_, px(kValueTextSize),
                    point(strength_value_pos.x, strength_value_pos.y),
                    WithAlpha(kSecondaryTextColor, content_alpha), strength_text.c_str());
      ImGui::SetCursorPos(strength_value_pos);
      ImGui::InvisibleButton("##edit_strength", ImVec2(px(52.0f), px(20.0f)));
      if (ImGui::IsItemClicked()) {
        numeric_edit_field_ = 2;
        numeric_edit_value_ = genie_strength_;
      }
    }
    ImGui::SetCursorPos(ImVec2(size.x - px(314.0f), px(630.0f) + y_offset));
    float proposed_strength = genie_strength_;
    const bool strength_active = Slider("##genie_strength", &proposed_strength, 0.25f, 1.0f,
                                        px(200.0f), scale, content_alpha, 1.0f, 0.01f);
    DelayedTooltip("Controls how strongly the window bends toward its taskbar target.", scale);
    if (strength_active && std::abs(proposed_strength - genie_strength_) > 0.0001f) {
      genie_strength_ = proposed_strength;
      strength_slider_dirty_ = true;
      if (strength_callback_) strength_callback_(genie_strength_, false);
    }
    if (strength_slider_active_ && !strength_active && strength_slider_dirty_) {
      const bool saved = !strength_callback_ || strength_callback_(genie_strength_, true);
      RecordSaveResult(saved);
      if (saved) strength_slider_dirty_ = false;
    }
    strength_slider_active_ = strength_active;

    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(678.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Fade during animation");
    ImGui::SetCursorPos(ImVec2(size.x - px(232.0f), px(671.0f) + y_offset));
    ImGui::SetNextItemWidth(px(190.0f));
    if (ImGui::BeginCombo("##fade_strength", fade_strength_.c_str())) {
      for (const char* choice : {"No fade", "Subtle", "Strong"}) {
        const bool selected = fade_strength_ == choice;
        if (ImGui::Selectable(choice, selected)) {
          const std::string previous = fade_strength_;
          fade_strength_ = choice;
          const bool saved = !fade_callback_ || fade_callback_(fade_strength_);
          if (!saved) fade_strength_ = previous;
          RecordSaveResult(saved);
        }
        if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(722.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Show taskbar target indicator");
    ImGui::SetCursorPos(ImVec2(size.x - px(80.0f), px(719.0f) + y_offset));
    bool proposed_indicator = show_target_indicator_;
    if (Toggle("##target_indicator", &proposed_indicator, scale, content_alpha)) {
      const bool saved =
          !target_indicator_callback_ || target_indicator_callback_(proposed_indicator);
      if (saved) show_target_indicator_ = proposed_indicator;
      RecordSaveResult(saved);
    }
    draw->AddText(font_small_, px(kHelperTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(750.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha),
                  "Briefly highlights where the window will minimize.");
  }

  if (selected_page_ == Page::kApplications) {
    const float applications_content_width =
        size.x - px(settings_ui::Metrics::kContentInset * 2.0f);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(76.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(730.0f) + y_offset), scale,
        content_alpha);
    draw->AddText(font_title_, px(kPageTitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(20.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Applications");
    draw->AddText(font_small_, px(kPageSubtitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(51.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha),
                  "Choose which applications should skip the effect");

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

    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(84.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Find an application");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(8.0f), px(5.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, px(1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Color(0x25262B));
    ImGui::PushStyleColor(ImGuiCol_Border, Color(0x3F3F46));
    ImGui::SetCursorPos(ImVec2(px(settings_ui::Metrics::kContentInset), px(108.0f) + y_offset));
    ImGui::SetNextItemWidth(applications_content_width);
    ImGui::InputTextWithHint("##app_search", "Search applications...", exclusion_input_.data(),
                             exclusion_input_.size());
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::SetCursorPos(ImVec2(px(settings_ui::Metrics::kContentInset), px(158.0f) + y_offset));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    if (ImGui::BeginChild("##excluded_list", ImVec2(applications_content_width, px(552.0f)),
                          ImGuiChildFlags_None)) {
      if (filtered_items.empty()) {
        ImGui::SetCursorPos(ImVec2(px(8.0f), px(8.0f)));
        ImGui::PushFont(font_small_);
        ImGui::TextDisabled(filter.empty() ? "No applications found" : "No matching applications");
        ImGui::PopFont();
      } else {
        const float child_width = ImGui::GetWindowSize().x;
        for (size_t i = 0; i < filtered_items.size(); ++i) {
          const float row_y = px(static_cast<float>(i) * 44.0f);
          ImGui::SetCursorPos(ImVec2(px(8.0f), row_y + px(12.0f)));
          ImGui::PushFont(font_body_);
          if (filtered_items[i].is_active) {
            ImGui::TextUnformatted(filtered_items[i].name.c_str());
          } else {
            ImGui::TextDisabled("%s (inactive)", filtered_items[i].name.c_str());
          }
          ImGui::PopFont();

          ImGui::SetCursorPos(ImVec2(child_width - px(50.0f), row_y + px(10.0f)));
          const std::string toggle_id = std::format("##toggle_exclude_{}", i);
          bool excluded = filtered_items[i].is_excluded;
          if (Toggle(toggle_id.c_str(), &excluded, scale, content_alpha)) {
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
          if (i + 1 < filtered_items.size()) {
            ImDrawList* child_draw = ImGui::GetWindowDrawList();
            const ImVec2 child_origin = ImGui::GetWindowPos();
            const float separator_y = child_origin.y + row_y + px(43.0f) - ImGui::GetScrollY();
            child_draw->AddLine(ImVec2(child_origin.x + px(8.0f), separator_y),
                                ImVec2(child_origin.x + child_width - px(8.0f), separator_y),
                                WithAlpha(settings_ui::kSeparator, content_alpha));
          }
        }
      }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    const std::string& visible_error =
        persistence_error_.empty() ? exclusion_error_ : persistence_error_;
    if (!visible_error.empty()) {
      draw->AddText(font_small_, px(kHelperTextSize),
                    point(px(settings_ui::Metrics::kContentInset), px(714.0f) + y_offset),
                    WithAlpha(IM_COL32(235, 120, 120, 255), content_alpha), visible_error.c_str());
    }
  }

  if (selected_page_ == Page::kHotkeys) {
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(82.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(292.0f) + y_offset), scale,
        content_alpha);
    draw->AddText(font_title_, px(kPageTitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(20.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Hotkeys");
    draw->AddText(font_small_, px(kPageSubtitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(51.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha),
                  "Click Change, then press the new key combination.");
    constexpr std::array labels = {
        "Toggle Genie Effect",
        "Open Settings",
        "Repair Windows",
    };
    for (size_t index = 0; index < labels.size(); ++index) {
      const float row_y = 96.0f + static_cast<float>(index) * 66.0f;
      if (index > 0) {
        settings_ui::DrawSeparator(
            draw, point(px(settings_ui::Metrics::kContentInset), px(row_y - 14.0f) + y_offset),
            point(size.x - px(settings_ui::Metrics::kContentInset), px(row_y - 14.0f) + y_offset),
            content_alpha);
      }
      draw->AddText(font_body_, px(kLabelTextSize),
                    point(px(settings_ui::Metrics::kContentInset), px(row_y) + y_offset),
                    WithAlpha(kPrimaryTextColor, content_alpha), labels[index]);
      const std::string binding_text = editing_hotkey_ == static_cast<int>(index)
                                           ? "Press keys..."
                                           : HotkeyText(hotkeys_[index]);
      const ImU32 binding_color = hotkey_available_[index] || hotkeys_[index].virtual_key == 0
                                      ? kSecondaryTextColor
                                      : IM_COL32(235, 120, 120, 255);
      draw->AddText(font_small_, px(kValueTextSize),
                    point(px(settings_ui::Metrics::kContentInset), px(row_y + 25.0f) + y_offset),
                    WithAlpha(binding_color, content_alpha), binding_text.c_str());
      ImGui::SetCursorPos(ImVec2(size.x - px(198.0f), px(row_y + 12.0f) + y_offset));
      const std::string change_id = std::format("##change_hotkey_{}", index);
      if (CompactButton(change_id.c_str(), "Change", ImVec2(px(76.0f), px(28.0f)), font_body_,
                        scale, content_alpha)) {
        editing_hotkey_ = static_cast<int>(index);
        hotkey_feedback_ = "Press a key combination, or Escape to cancel";
      }
      ImGui::SetCursorPos(ImVec2(size.x - px(114.0f), px(row_y + 12.0f) + y_offset));
      const std::string disable_id = std::format("##disable_hotkey_{}", index);
      if (CompactButton(disable_id.c_str(), "Disable", ImVec2(px(72.0f), px(28.0f)), font_body_,
                        scale, content_alpha)) {
        const HotkeyUpdateResult result =
            hotkey_update_callback_
                ? hotkey_update_callback_(static_cast<HotkeyAction>(index), HotkeyBinding{})
                : HotkeyUpdateResult::kInvalid;
        hotkey_feedback_ = HotkeyResultText(result);
        editing_hotkey_ = -1;
      }
    }
    if (!hotkey_feedback_.empty()) {
      draw->AddText(font_small_, px(kHelperTextSize),
                    point(px(settings_ui::Metrics::kContentInset),
                          px(96.0f + static_cast<float>(labels.size()) * 66.0f + 12.0f) + y_offset),
                    WithAlpha(kSecondaryTextColor, content_alpha), hotkey_feedback_.c_str());
    }
  }

  if (selected_page_ == Page::kWindowsIntegration) {
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(76.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(226.0f) + y_offset), scale,
        content_alpha);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(246.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(364.0f) + y_offset), scale,
        content_alpha);
    draw->AddText(font_title_, px(kPageTitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(20.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Windows & System");
    draw->AddText(font_small_, px(kPageSubtitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(51.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha), "System behavior and power usage");
    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(82.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Windows behavior");
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(116.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha),
                  "Disable animations while a fullscreen application is active");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(38.0f),
                               px(113.0f) + y_offset));
    const bool previous_fullscreen_behavior = disable_animations_fullscreen_;
    if (Toggle("##disable_fullscreen_animations", &disable_animations_fullscreen_, scale,
               content_alpha)) {
      const bool saved = !fullscreen_behavior_callback_ ||
                         fullscreen_behavior_callback_(disable_animations_fullscreen_);
      if (!saved) disable_animations_fullscreen_ = previous_fullscreen_behavior;
      RecordSaveResult(saved);
    }
    DelayedTooltip("Uses normal Windows behavior while a true fullscreen app is active.", scale);
    draw->AddText(font_small_, px(kHelperTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(145.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha),
                  "Normal maximized windows continue to use Genie animations.");

    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(190.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha),
                  "Follow the Windows animation preference");
    ImGui::SetCursorPos(ImVec2(size.x - px(80.0f), px(187.0f) + y_offset));
    bool proposed_follow = follow_windows_animation_preference_;
    if (Toggle("##follow_windows_animations", &proposed_follow, scale, content_alpha)) {
      const bool saved = !windows_animation_preference_callback_ ||
                         windows_animation_preference_callback_(proposed_follow);
      if (saved) follow_windows_animation_preference_ = proposed_follow;
      RecordSaveResult(saved);
    }

    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(262.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Power");
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(296.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Reduce effects on battery");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(38.0f),
                               px(293.0f) + y_offset));
    bool proposed_reduce_on_battery = reduce_effects_on_battery_;
    if (Toggle("##reduce_on_battery", &proposed_reduce_on_battery, scale, content_alpha)) {
      const bool saved =
          !power_behavior_callback_ ||
          power_behavior_callback_(proposed_reduce_on_battery, disable_effects_battery_saver_);
      if (saved) reduce_effects_on_battery_ = proposed_reduce_on_battery;
      RecordSaveResult(saved);
    }
    DelayedTooltip("Reduces capture and rendering work while running on battery.", scale);

    draw->AddText(font_body_, px(kLabelTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(342.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Disable effects in battery saver");
    ImGui::SetCursorPos(ImVec2(size.x - px(settings_ui::Metrics::kContentInset) - px(38.0f),
                               px(339.0f) + y_offset));
    bool proposed_disable_in_saver = disable_effects_battery_saver_;
    if (Toggle("##disable_in_battery_saver", &proposed_disable_in_saver, scale, content_alpha)) {
      const bool saved =
          !power_behavior_callback_ ||
          power_behavior_callback_(reduce_effects_on_battery_, proposed_disable_in_saver);
      if (saved) disable_effects_battery_saver_ = proposed_disable_in_saver;
      RecordSaveResult(saved);
    }
  }

  if (selected_page_ == Page::kDiagnostics) {
    const ULONGLONG now = GetTickCount64();
    if (diagnostics_callback_ != nullptr &&
        (diagnostics_.status.empty() || now - last_diagnostics_refresh_ms_ >= 500)) {
      diagnostics_ = diagnostics_callback_();
      last_diagnostics_refresh_ms_ = now;
    }
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(64.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(382.0f) + y_offset), scale,
        content_alpha);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(402.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(530.0f) + y_offset), scale,
        content_alpha);
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(550.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(706.0f) + y_offset), scale,
        content_alpha);
    draw->AddText(font_title_, px(kPageTitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(20.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Diagnostics & Repair");
    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(76.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Runtime");
    const std::array runtime_rows = {
        std::pair{"Status", &diagnostics_.status},
        std::pair{"Effect", &diagnostics_.effect},
        std::pair{"Pause state", &diagnostics_.pause},
        std::pair{"Hook", &diagnostics_.hook},
        std::pair{"Renderer", &diagnostics_.renderer},
        std::pair{"D3D Device", &diagnostics_.d3d_device},
        std::pair{"Active animations", &diagnostics_.active_animations},
        std::pair{"Watchdog", &diagnostics_.watchdog},
        std::pair{"Display refresh", &diagnostics_.display_refresh},
        std::pair{"Window monitor", &diagnostics_.window_monitor},
        std::pair{"Taskbar detected", &diagnostics_.taskbar},
        std::pair{"Safe Mode", &diagnostics_.safe_mode},
        std::pair{"Startup repair", &diagnostics_.startup_repair},
    };
    for (size_t index = 0; index < runtime_rows.size(); ++index) {
      const float row_y = 104.0f + static_cast<float>(index) * 20.0f;
      draw->AddText(font_small_, px(kValueTextSize),
                    point(px(settings_ui::Metrics::kContentInset), px(row_y) + y_offset),
                    WithAlpha(kSecondaryTextColor, content_alpha), runtime_rows[index].first);
      draw->AddText(font_small_, px(kValueTextSize), point(px(210.0f), px(row_y) + y_offset),
                    WithAlpha(kPrimaryTextColor, content_alpha),
                    runtime_rows[index].second->c_str());
    }

    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(414.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "System");
    const std::array system_rows = {
        std::pair{"Windows", &diagnostics_.windows_version},
        std::pair{"Graphics adapter", &diagnostics_.graphics_adapter},
        std::pair{"Displays", &diagnostics_.monitor_configuration},
        std::pair{"Log folder size", &diagnostics_.log_folder_size},
    };
    for (size_t index = 0; index < system_rows.size(); ++index) {
      const float row_y = 444.0f + static_cast<float>(index) * 20.0f;
      draw->AddText(font_small_, px(kValueTextSize),
                    point(px(settings_ui::Metrics::kContentInset), px(row_y) + y_offset),
                    WithAlpha(kSecondaryTextColor, content_alpha), system_rows[index].first);
      draw->AddText(font_small_, px(kValueTextSize), point(px(210.0f), px(row_y) + y_offset),
                    WithAlpha(kPrimaryTextColor, content_alpha),
                    system_rows[index].second->c_str());
    }
    constexpr std::array actions = {
        std::pair{"Copy Report", DiagnosticsAction::kCopy},
        std::pair{"Open Logs", DiagnosticsAction::kOpenLogFolder},
        std::pair{"Repair Windows", DiagnosticsAction::kRepairWindows},
        std::pair{"Restart Renderer", DiagnosticsAction::kRestartRenderer},
    };
    draw->AddText(font_medium_, px(kSectionTitleTextSize),
                  point(px(settings_ui::Metrics::kContentInset), px(562.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "Actions");
    const float action_gap = px(8.0f);
    const float action_width =
        (size.x - px(settings_ui::Metrics::kContentInset * 2.0f) - action_gap) * 0.5f;
    for (size_t index = 0; index < actions.size(); ++index) {
      const float button_x = px(settings_ui::Metrics::kContentInset) +
                             static_cast<float>(index % 2) * (action_width + action_gap);
      const float button_y = px(590.0f + static_cast<float>(index / 2) * 38.0f) + y_offset;
      ImGui::SetCursorPos(ImVec2(button_x, button_y));
      const std::string id = std::format("##diagnostics_action_{}", index);
      if (CompactButton(id.c_str(), actions[index].first, ImVec2(action_width, px(30.0f)),
                        font_small_, scale, content_alpha)) {
        const bool succeeded =
            diagnostics_action_callback_ && diagnostics_action_callback_(actions[index].second);
        diagnostics_feedback_ = succeeded ? "Action completed" : "Action failed";
        last_diagnostics_refresh_ms_ = 0;
      }
    }
    if (diagnostics_.safe_mode == "On") {
      ImGui::SetCursorPos(ImVec2(px(settings_ui::Metrics::kContentInset), px(666.0f) + y_offset));
      if (CompactButton("##exit_safe_mode", "Start Normally", ImVec2(action_width, px(28.0f)),
                        font_small_, scale, content_alpha)) {
        const bool succeeded = diagnostics_action_callback_ &&
                               diagnostics_action_callback_(DiagnosticsAction::kExitSafeMode);
        diagnostics_feedback_ =
            succeeded ? "Normal animation runtime started" : "Could not leave Safe Mode";
        last_diagnostics_refresh_ms_ = 0;
      }
    }
    if (!diagnostics_feedback_.empty()) {
      const float feedback_x =
          diagnostics_.safe_mode == "On"
              ? px(settings_ui::Metrics::kContentInset) + action_width + action_gap
              : px(settings_ui::Metrics::kContentInset);
      draw->AddText(font_small_, px(kHelperTextSize), point(feedback_x, px(672.0f) + y_offset),
                    WithAlpha(kSecondaryTextColor, content_alpha), diagnostics_feedback_.c_str());
    }
  }

  if (selected_page_ == Page::kAbout) {
    if (diagnostics_.version.empty() && diagnostics_callback_ != nullptr) {
      diagnostics_ = diagnostics_callback_();
    }
    settings_ui::DrawCard(
        draw, point(px(settings_ui::Metrics::kPageInset), px(68.0f) + y_offset),
        point(size.x - px(settings_ui::Metrics::kPageInset), px(328.0f) + y_offset), scale,
        content_alpha);
    draw->AddText(font_title_, px(kPageTitleTextSize),
                  point(px(settings_ui::Metrics::kPageInset), px(20.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), "About");
    const char* product = "Genie Effect";
    const ImVec2 product_size =
        font_title_->CalcTextSizeA(px(kPageTitleTextSize), FLT_MAX, 0.0f, product);
    draw->AddText(font_title_, px(kPageTitleTextSize),
                  point(size.x * 0.5f - product_size.x * 0.5f, px(120.0f) + y_offset),
                  WithAlpha(kPrimaryTextColor, content_alpha), product);
    const char* description = "A lightweight, native Genie animation for Windows.";
    const ImVec2 description_size =
        font_body_->CalcTextSizeA(px(kLabelTextSize), FLT_MAX, 0.0f, description);
    draw->AddText(font_body_, px(kLabelTextSize),
                  point(size.x * 0.5f - description_size.x * 0.5f, px(168.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha), description);
    const std::string version = diagnostics_.version.empty()
                                    ? "Windows application"
                                    : std::format("Version {}", diagnostics_.version);
    const ImVec2 version_size =
        font_small_->CalcTextSizeA(px(kValueTextSize), FLT_MAX, 0.0f, version.c_str());
    draw->AddText(font_small_, px(kValueTextSize),
                  point(size.x * 0.5f - version_size.x * 0.5f, px(210.0f) + y_offset),
                  WithAlpha(kSecondaryTextColor, content_alpha), version.c_str());
  }

  // Draw-list content does not expand ImGui's scroll range on its own. A real
  // layout spacer guarantees that every page can scroll past its final card,
  // preserving the macOS-style safe area instead of pinning the card to the
  // bottom edge of the window.
  ImGui::SetCursorPos(
      ImVec2(0.0f, px(page_content_bottom + settings_ui::Metrics::kScrollBottomPadding)));
  ImGui::Dummy(ImVec2(1.0f, 1.0f));

  const float scroll_y = ImGui::GetScrollY();
  const float scroll_max = ImGui::GetScrollMaxY();
  const float fade_height = px(settings_ui::Metrics::kScrollFadeHeight);
  const float fade_right = origin.x + size.x - px(settings_ui::Metrics::kScrollGutter);
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

  if (!save_feedback_.empty() && GetTickCount64() < save_feedback_until_ms_) {
    const ImU32 color =
        save_feedback_error_ ? IM_COL32(235, 120, 120, 255) : IM_COL32(120, 205, 145, 255);
    const ImVec2 text_size = ImGui::CalcTextSize(save_feedback_.c_str());
    draw->AddRectFilled(ImVec2(origin.x + size.x - text_size.x - px(50.0f),
                               origin.y + size.y - px(settings_ui::Metrics::kContentInset)),
                        ImVec2(origin.x + size.x - px(18.0f), origin.y + size.y - px(14.0f)),
                        WithAlpha(IM_COL32(34, 37, 43, 255), content_alpha), px(6.0f));
    draw->AddText(
        font_small_, px(kHelperTextSize),
        ImVec2(origin.x + size.x - text_size.x - px(34.0f), origin.y + size.y - px(35.0f)),
        WithAlpha(color, content_alpha), save_feedback_.c_str());
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();

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
    settings->AddTrayIcon();
    return 0;
  }

  // Avoid DefWindowProc's modal caption-move loop. It blocks the application
  // thread and freezes active Genie animations while the settings window is dragged.
  if (settings != nullptr && message == WM_LBUTTONDOWN) {
    const POINT client_point{static_cast<short>(LOWORD(l_param)),
                             static_cast<short>(HIWORD(l_param))};
    RECT client{};
    GetClientRect(hwnd, &client);
    const LONG traffic_lights_end = static_cast<LONG>(112.0f * scale);
    const LONG header_actions_start = client.right - static_cast<LONG>(120.0f * scale);
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
