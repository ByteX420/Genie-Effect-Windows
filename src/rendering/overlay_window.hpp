#pragma once

#include <chrono>
#include <d3d11_4.h>
#include <dcomp.h>
#include <dxgi1_6.h>
#include <functional>
#include <windows.h>
#include <wrl/client.h>

#include "animation/genie_mesh.hpp"
#include "rendering/d3d_device.hpp"
#include "rendering/desktop_capture.hpp"

namespace genie::rendering {

class OverlayWindow {
public:
  using MinimizeCallback = std::function<bool(HWND)>;
  using RestoreCallback = std::function<bool(HWND)>;

  OverlayWindow() = default;
  ~OverlayWindow();

  OverlayWindow(const OverlayWindow&) = delete;
  OverlayWindow& operator=(const OverlayWindow&) = delete;

  bool Initialize(HINSTANCE instance, D3dDevice* d3d_device, MinimizeCallback minimize_callback,
                  RestoreCallback restore_callback);
  void Shutdown();
  void SetAnimationDuration(float duration_seconds) {
    animation_duration_seconds_ = duration_seconds;
  }

  [[nodiscard]] HWND window() const { return window_; }
  [[nodiscard]] bool active() const { return animation_state_.active; }
  [[nodiscard]] bool clock_started() const { return animation_state_.clock_started; }
  [[nodiscard]] bool device_lost() const { return device_lost_; }

  [[nodiscard]] bool StartAnimation(CapturedTexture captured_texture,
                                    const genie::animation::RectF& source_screen_rect,
                                    const genie::animation::RectF& target_screen_rect,
                                    genie::animation::GenieEdge edge, float start_progress = 0.0f,
                                    float target_progress = 1.0f);
  void StartAnimationClock();
  void ContinueMinimizeAnimation();
  void ReverseAnimation();
  bool Tick();
  void CancelAnimation();
  void FinishRestoreAnimation();
  [[nodiscard]] bool restoring() const {
    return animation_state_.active && animation_state_.target_progress < animation_state_.progress;
  }
  [[nodiscard]] CapturedTexture* mutable_captured_texture() {
    return &animation_state_.captured_texture;
  }

private:
  struct AnimationState {
    bool active = false;
    CapturedTexture captured_texture;
    genie::animation::RectF source_rect;
    genie::animation::RectF target_rect;
    genie::animation::GenieEdge edge = genie::animation::GenieEdge::kBottom;
    std::chrono::steady_clock::time_point last_tick_time;
    float progress = 0.0f;
    float target_progress = 1.0f;
    bool clock_started = false;
  };

  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

  bool RegisterWindowClass(HINSTANCE instance);
  bool CreateOverlayWindow(HINSTANCE instance);
  bool InitializeComposition();
  bool CreateRenderTarget();
  bool ResizeOverlaySurface(const RECT& screen_rect);
  void ApplyVisibleOverlayRegion(HWND taskbar_window);
  bool CreateRenderResources();
  bool CompileShaders();
  bool UploadMesh(const genie::animation::GenieMesh& mesh, bool upload_indices);
  bool UpdateFrameConstants();
  [[nodiscard]] bool Render(float progress);
  void ClearFrame();
  void HideOverlay();
  void MarkDeviceLost(const wchar_t* operation, HRESULT hr);
  [[nodiscard]] genie::animation::RectF ToOverlayRect(
      const genie::animation::RectF& screen_rect) const;

  D3dDevice* d3d_device_ = nullptr;
  HWND window_ = nullptr;
  RECT virtual_screen_rect_{};
  RECT overlay_screen_rect_{};
  UINT width_ = 0;
  UINT height_ = 0;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view_;
  Microsoft::WRL::ComPtr<IDCompositionDevice> composition_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> composition_target_;
  Microsoft::WRL::ComPtr<IDCompositionVisual> composition_visual_;
  Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader_;
  Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout_;
  Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer_;
  Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer_;
  Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer_;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_state_;
  Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state_;
  Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_state_;

  UINT index_count_ = 0;
  AnimationState animation_state_;
  genie::animation::GenieMeshGenerator mesh_generator_;
  genie::animation::GenieMesh reusable_mesh_;
  MinimizeCallback minimize_callback_;
  RestoreCallback restore_callback_;
  float animation_duration_seconds_ = 0.70f;
  UINT minimize_attempt_message_ = 0;
  UINT restore_attempt_message_ = 0;
  bool device_lost_ = false;
};

}  // namespace genie::rendering
