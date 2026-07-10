#include "pch.hpp"

#include "app/settings_window.hpp"

#include <cmath>
#include <dwmapi.h>
#include <format>
#include <shellapi.h>

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "common/debug_log.hpp"
#include "imgui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message,
                                                             WPARAM w_param, LPARAM l_param);

namespace genie::app {
namespace {

constexpr wchar_t kSettingsWindowClass[] = L"GenieEffectImGuiSettings";
constexpr int kWindowWidth = 400;
constexpr int kWindowHeight = 140;
constexpr float kMinimumAnimationDurationSeconds = 0.10f;
constexpr float kMaximumAnimationDurationSeconds = 2.00f;
constexpr UINT kTrayMessage = WM_APP + 100;
constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayShowSettings = 3000;
constexpr UINT kTrayRepairWindows = 3001;
constexpr UINT kTrayExit = 3002;
constexpr ImU32 kBackgroundColor = IM_COL32(30, 30, 31, 255);
constexpr ImU32 kBorderColor = IM_COL32(63, 63, 70, 255);
constexpr ImU32 kTrackColor = IM_COL32(45, 45, 48, 255);
constexpr ImU32 kAccentColor = IM_COL32(0, 122, 204, 255);
constexpr ImU32 kPrimaryTextColor = IM_COL32(241, 241, 241, 255);
constexpr ImU32 kSecondaryTextColor = IM_COL32(200, 200, 200, 255);
constexpr float kSmallFontSize = 12.5f;
constexpr float kBodyFontSize = 14.5f;
constexpr float kTitleFontSize = 21.0f;
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

bool Toggle(const char* id, bool* value, float scale, float alpha) {
  const ImVec2 size(36.0f * scale, 20.0f * scale);
  ImGui::InvisibleButton(id, size);
  const bool changed = ImGui::IsItemClicked();
  if (changed) *value = !*value;

  const float state = Animate(ImGui::GetID(id), *value ? 1.0f : 0.0f);
  const float hover = Animate(ImGui::GetID("hover"), ImGui::IsItemHovered() ? 1.0f : 0.0f, 20.0f);
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();

  const ImU32 off = kBorderColor;
  const ImU32 on = kAccentColor;
  const ImU32 track = Blend(off, on, state);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(min, max, WithAlpha(track, alpha), 0.0f);
  if (hover > 0.01f) {
    draw->AddRect(min, max, WithAlpha(IM_COL32(85, 85, 85, 255), hover * alpha), 0.0f, 0, scale);
  }
  const float thumb_w = 12.0f * scale;
  const float thumb_h = 16.0f * scale;
  const float thumb_y = min.y + 2.0f * scale;
  const float thumb_x = min.x + (2.0f + state * (size.x / scale - thumb_w / scale - 4.0f)) * scale;
  draw->AddRectFilled(ImVec2(thumb_x, thumb_y), ImVec2(thumb_x + thumb_w, thumb_y + thumb_h),
                      WithAlpha(kPrimaryTextColor, alpha), 0.0f);
  return changed;
}

bool Slider(const char* id, float* value, float minimum, float maximum, float width, float scale,
            float alpha) {
  const float height = 20.0f * scale;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float track_inset = 5.0f * scale;
  const float start = origin.x + track_inset;
  const float end = origin.x + width - track_inset;
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool active = ImGui::IsItemActive();
  if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
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
                      ImVec2(end, track_y + track_h * 0.5f), WithAlpha(kTrackColor, alpha), 0.0f);
  draw->AddRectFilled(ImVec2(start, track_y - track_h * 0.5f),
                      ImVec2(knob_x, track_y + track_h * 0.5f), WithAlpha(kAccentColor, alpha),
                      0.0f);

  const float knob_w = (8.0f + 2.0f * hover) * scale;
  const float knob_h = (12.0f + 2.0f * hover) * scale;
  draw->AddRectFilled(ImVec2(knob_x - knob_w * 0.5f, track_y - knob_h * 0.5f),
                      ImVec2(knob_x + knob_w * 0.5f, track_y + knob_h * 0.5f),
                      WithAlpha(kPrimaryTextColor, alpha), 0.0f);
  return active;
}

static void DrawSymmetricX(
    ImDrawList* draw,
    const ImVec2& min,
    float size,
    ImU32 color,
    float scale = 3.0f)
{
  const ImVec2 center(
      min.x + size * 0.5f - 0.5f,
      min.y + size * 0.5f - 0.5f
  );

  const float armLength = 4.0f * scale;
  const float thickness = 1.0f;

  // Disable anti-aliasing to prevent double-blending/halo in the center
  const ImDrawListFlags old_flags = draw->Flags;
  draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;

  // Center to Top-Left
  draw->AddLine(
      center,
      ImVec2(center.x - armLength, center.y - armLength),
      color,
      thickness
  );

  // Center to Bottom-Right
  draw->AddLine(
      center,
      ImVec2(center.x + armLength, center.y + armLength),
      color,
      thickness
  );

  // Center to Top-Right
  draw->AddLine(
      center,
      ImVec2(center.x + armLength, center.y - armLength),
      color,
      thickness
  );

  // Center to Bottom-Left
  draw->AddLine(
      center,
      ImVec2(center.x - armLength, center.y + armLength),
      color,
      thickness
  );

  draw->Flags = old_flags;
}

bool CloseButton(float scale, float alpha) {
  const float size = 16.0f * scale;
  const ImVec2 min = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##close_panel", ImVec2(size, size));
  const float hover =
      Animate(ImGui::GetID("##close_panel"), ImGui::IsItemHovered() ? 1.0f : 0.0f, 24.0f);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImU32 color = Blend(IM_COL32(150, 150, 150, 255), IM_COL32(255, 255, 255, 255), hover);

  DrawSymmetricX(draw, min, size, WithAlpha(color, alpha), scale);
  return ImGui::IsItemClicked();
}

bool MinimizeButton(float scale, float alpha) {
  const float size = 16.0f * scale;
  const ImVec2 min = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##minimize_panel", ImVec2(size, size));
  const float hover =
      Animate(ImGui::GetID("##minimize_panel"), ImGui::IsItemHovered() ? 1.0f : 0.0f, 24.0f);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImU32 color = Blend(IM_COL32(150, 150, 150, 255), IM_COL32(255, 255, 255, 255), hover);

  const ImVec2 center(
      min.x + size * 0.5f - 0.5f,
      min.y + size * 0.5f - 0.5f
  );
  const float arm_length = 4.0f * scale;
  const float thickness = 1.0f;

  const ImDrawListFlags old_flags = draw->Flags;
  draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;

  draw->AddLine(
      ImVec2(center.x - arm_length, center.y),
      ImVec2(center.x + arm_length, center.y),
      WithAlpha(color, alpha),
      thickness
  );

  draw->Flags = old_flags;
  return ImGui::IsItemClicked();
}

}  // namespace

SettingsWindow::~SettingsWindow() { Shutdown(); }

bool SettingsWindow::Initialize(HINSTANCE instance, ToggleCallback toggle_callback,
                                SpeedCallback speed_callback, HealCallback heal_callback,
                                ExitCallback exit_callback) {
  toggle_callback_ = std::move(toggle_callback);
  speed_callback_ = std::move(speed_callback);
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

  NOTIFYICONDATAW tray_icon{};
  tray_icon.cbSize = sizeof(tray_icon);
  tray_icon.hWnd = hwnd_;
  tray_icon.uID = kTrayIconId;
  tray_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  tray_icon.uCallbackMessage = kTrayMessage;
  tray_icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wcscpy_s(tray_icon.szTip, L"Genie Effect");
  Shell_NotifyIconW(NIM_ADD, &tray_icon);
  return true;
}

void SettingsWindow::Shutdown() {
  if (hwnd_ != nullptr) {
    NOTIFYICONDATAW tray_icon{};
    tray_icon.cbSize = sizeof(tray_icon);
    tray_icon.hWnd = hwnd_;
    tray_icon.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &tray_icon);
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

void SettingsWindow::Show(bool show) {
  if (hwnd_ == nullptr) return;
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

void SettingsWindow::UpdateState(bool enabled, float duration_seconds) {
  const bool changed =
      is_enabled_ != enabled || std::abs(duration_seconds_ - duration_seconds) > 0.0001f;
  is_enabled_ = enabled;
  duration_seconds_ = duration_seconds;
  if (changed) ForceRender();
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
                          WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                          width, height, nullptr, nullptr, instance, this);
  if (old_context != nullptr) SetThreadDpiAwarenessContext(old_context);
  if (hwnd_ == nullptr) return false;

  current_dpi_ = GetDpiForWindow(hwnd_);
  ui_scale_ = static_cast<float>(current_dpi_) / USER_DEFAULT_SCREEN_DPI;
  ApplyWindowShape(width, height);
  const DWM_WINDOW_CORNER_PREFERENCE corner_preference = DWMWCP_ROUND;
  DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_preference,
                        sizeof(corner_preference));
  const MARGINS margins{-1};
  DwmExtendFrameIntoClientArea(hwnd_, &margins);
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
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
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

void SettingsWindow::ApplyStyle() {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;
  style.FrameRounding = 0.0f;
  style.GrabRounding = 0.0f;
  style.WindowPadding = ImVec2(0.0f, 0.0f);
  style.ItemSpacing = ImVec2(0.0f, 0.0f);
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;
  style.Colors[ImGuiCol_WindowBg] = Color(0x1E1E1F);
  style.Colors[ImGuiCol_Text] = Color(0xF1F1F1);
  style.Colors[ImGuiCol_TextDisabled] = Color(0x9E9E9E);
}

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
  const std::string regular_font = SystemFontPath(L"segoeui.ttf");
  const std::string semibold_font = SystemFontPath(L"seguisb.ttf");
  const std::string title_font = SystemFontPath(L"segoeuib.ttf");
  const auto add_font = [&config](const std::string& path, float size) -> ImFont* {
    return path.empty() ? nullptr
                        : ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size, &config);
  };
  font_small_ = add_font(regular_font, kSmallFontSize * scale);
  font_body_ = add_font(regular_font, kBodyFontSize * scale);
  font_medium_ = add_font(semibold_font, kBodyFontSize * scale);
  font_title_ = add_font(title_font, kTitleFontSize * scale);
  if (font_body_ == nullptr) font_body_ = io.Fonts->AddFontDefault();
  if (font_small_ == nullptr) font_small_ = font_body_;
  if (font_medium_ == nullptr) font_medium_ = font_body_;
  if (font_title_ == nullptr) font_title_ = font_medium_;
  if (imgui_dx11_ready_) ImGui_ImplDX11_CreateDeviceObjects();
}

void SettingsWindow::ApplyWindowShape(int width, int height) {
  HRGN rectangular_region = CreateRectRgn(0, 0, width + 1, height + 1);
  if (rectangular_region != nullptr && SetWindowRgn(hwnd_, rectangular_region, TRUE) == 0) {
    DeleteObject(rectangular_region);
  }
}

void SettingsWindow::UpdateDpi(UINT dpi) {
  if (dpi == 0 || dpi == current_dpi_) return;
  RebuildFonts(dpi);
  ApplyStyle();
}

void SettingsWindow::Render() {
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
  if (!is_animating && !is_active && !render_requested_) {
    return;
  }
  render_requested_ = false;

  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  RenderContents();
  ImGui::Render();
  constexpr float clear_color[] = {0.055f, 0.059f, 0.071f, 1.0f};
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
  return appearance_active || GetForegroundWindow() == hwnd_ || render_requested_;
}

void SettingsWindow::RenderContents() {
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  const float scale = ui_scale_;
  const auto px = [scale](float value) { return value * scale; };
  const float elapsed = static_cast<float>(GetTickCount64() - shown_at_ms_);
  const float appearance = 1.0f - std::exp(-elapsed / kAppearanceDurationMs);
  const float content_alpha = std::clamp(appearance, 0.0f, 1.0f);
  const float y_offset = px(6.0f) * (1.0f - content_alpha);

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(size);
  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::Begin("GenieEffectRoot", nullptr, flags);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetWindowPos();
  const auto point = [&origin](float x, float y) { return ImVec2(origin.x + x, origin.y + y); };

  // Flat dark grey background (no corners)
  draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), kBackgroundColor, 0.0f);
  // Slate border
  draw->AddRect(origin, ImVec2(origin.x + size.x - 1.0f, origin.y + size.y - 1.0f), kBorderColor,
                0.0f);

  // Title
  draw->AddText(font_medium_, px(14.0f), point(px(18.0f), px(16.0f) + y_offset),
                WithAlpha(kPrimaryTextColor, content_alpha), "Genie Effect");

  // Minimize Button
  ImGui::SetCursorPos(ImVec2(size.x - px(54.0f), px(14.0f) + y_offset));
  if (MinimizeButton(scale, content_alpha)) {
    ShowWindow(hwnd_, SW_MINIMIZE);
  }

  // Close Button
  ImGui::SetCursorPos(ImVec2(size.x - px(34.0f), px(14.0f) + y_offset));
  if (CloseButton(scale, content_alpha)) {
    if (exit_callback_) {
      exit_callback_();
    } else {
      Show(false);
    }
  }

  // Row 1: Animations (y = 52)
  draw->AddText(font_body_, px(13.5f), point(px(18.0f), px(52.0f) + y_offset),
                WithAlpha(kSecondaryTextColor, content_alpha), "Animations");
  ImGui::SetCursorPos(ImVec2(size.x - px(18.0f) - px(36.0f), px(50.0f) + y_offset));
  if (Toggle("##animations_enabled", &is_enabled_, scale, content_alpha) && toggle_callback_) {
    toggle_callback_(is_enabled_);
  }

  // Row 2: Duration (y = 92)
  draw->AddText(font_body_, px(13.5f), point(px(18.0f), px(92.0f) + y_offset),
                WithAlpha(kSecondaryTextColor, content_alpha), "Duration");

  // Value text on the right
  const std::string duration_text = std::format("{:.2f}s", duration_seconds_);
  const ImVec2 duration_size =
      font_medium_->CalcTextSizeA(px(13.0f), FLT_MAX, 0.0f, duration_text.c_str());
  draw->AddText(font_medium_, px(13.0f),
                point(size.x - px(18.0f) - duration_size.x, px(92.0f) + y_offset),
                WithAlpha(kSecondaryTextColor, content_alpha), duration_text.c_str());

  // Slider (Right-aligned next to value text)
  const float slider_w = px(120.0f);
  ImGui::SetCursorPos(
      ImVec2(size.x - px(18.0f) - duration_size.x - slider_w - px(8.0f), px(92.0f) + y_offset));
  float updated_duration = duration_seconds_;
  if (Slider("##duration", &updated_duration, kMinimumAnimationDurationSeconds,
             kMaximumAnimationDurationSeconds, slider_w, scale, content_alpha) &&
      std::abs(updated_duration - duration_seconds_) > 0.0001f) {
    duration_seconds_ = updated_duration;
    if (speed_callback_) speed_callback_(duration_seconds_);
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

  if (message == kTrayMessage && settings != nullptr) {
    if (l_param == WM_LBUTTONUP || l_param == WM_LBUTTONDBLCLK) {
      settings->Show(true);
      return 0;
    }
    if (l_param == WM_RBUTTONUP) {
      HMENU menu = CreatePopupMenu();
      if (menu != nullptr) {
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
        if (selected == kTrayShowSettings) {
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
        if (settings->exit_callback_) {
          settings->exit_callback_();
        } else {
          settings->Show(false);
        }
      }
      return 0;
    case WM_NCHITTEST: {
      POINT pt{static_cast<short>(LOWORD(l_param)), static_cast<short>(HIWORD(l_param))};
      ScreenToClient(hwnd, &pt);
      if (pt.y >= 0 && pt.y < 60.0f * scale && pt.x >= 0 && pt.x < (kWindowWidth - 50.0f) * scale) {
        return HTCAPTION;
      }
      return HTCLIENT;
    }
    default:
      return DefWindowProcW(hwnd, message, w_param, l_param);
  }
}

}  // namespace genie::app
