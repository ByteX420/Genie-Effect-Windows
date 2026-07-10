#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <functional>
#include <windows.h>
#include <wrl/client.h>

struct ImFont;

namespace genie::app {

class SettingsWindow {
public:
  using ToggleCallback = std::function<void(bool)>;
  using SpeedCallback = std::function<void(float minimize_duration, float restore_duration)>;
  using HealCallback = std::function<void()>;
  using ExitCallback = std::function<void()>;

  SettingsWindow() = default;
  ~SettingsWindow();
  SettingsWindow(const SettingsWindow&) = delete;
  SettingsWindow& operator=(const SettingsWindow&) = delete;

  bool Initialize(HINSTANCE instance, ToggleCallback toggle_callback, SpeedCallback speed_callback,
                  HealCallback heal_callback, ExitCallback exit_callback);
  void Shutdown();
  void Show(bool show);
  void UpdateState(bool enabled, float minimize_duration, float restore_duration);
  void Render();
  void ForceRender();
  [[nodiscard]] HWND hwnd() const { return hwnd_; }
  [[nodiscard]] bool WantsContinuousRendering() const;

private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
  bool CreateRenderWindow(HINSTANCE instance);
  bool CreateDeviceResources();
  bool CreateRenderTarget();
  void ReleaseDeviceResources();
  void HandleDeviceLost();
  bool TryRecoverDeviceResources();
  [[nodiscard]] static bool IsDeviceLostError(HRESULT hr);
  void CleanupRenderTarget();
  void ApplyStyle();
  void RebuildFonts(UINT dpi);
  void ApplyWindowShape(int width, int height);
  void UpdateDpi(UINT dpi);
  void RenderContents();

  HWND hwnd_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view_;
  ToggleCallback toggle_callback_;
  SpeedCallback speed_callback_;
  HealCallback heal_callback_;
  ExitCallback exit_callback_;
  bool is_enabled_ = true;
  float minimize_duration_seconds_ = 0.70f;
  float restore_duration_seconds_ = 0.70f;
  bool imgui_ready_ = false;
  bool imgui_context_ready_ = false;
  bool imgui_win32_ready_ = false;
  bool imgui_dx11_ready_ = false;
  bool device_recovery_pending_ = false;
  ULONGLONG next_device_recovery_ms_ = 0;
  DWORD device_recovery_delay_ms_ = 0;
  bool device_recovery_test_pending_ = false;
  bool render_requested_ = false;
  ULONGLONG shown_at_ms_ = 0;
  UINT current_dpi_ = USER_DEFAULT_SCREEN_DPI;
  float ui_scale_ = 1.0f;
  ImFont* font_small_ = nullptr;
  ImFont* font_body_ = nullptr;
  ImFont* font_medium_ = nullptr;
  ImFont* font_title_ = nullptr;
};

}  // namespace genie::app
