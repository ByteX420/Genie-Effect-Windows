#include "pch.hpp"

#include "rendering/overlay_window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <d3dcompiler.h>
#include <dwmapi.h>
#include <iostream>
#include <sstream>

#include "common/debug_log.hpp"
#include "platform/window_util.hpp"

namespace genie::rendering {
namespace {

constexpr wchar_t kOverlayWindowClassName[] = L"GenieEffectOverlayWindow";
constexpr wchar_t kAllowMinimizeProperty[] = L"GenieAllowMinimize";
constexpr UINT kMaxMeshVertices = 102;
constexpr UINT kMaxMeshIndices = 300;
constexpr LONG kOverlayPadding = 2;

struct FrameConstants {
  float viewport_size[2]{};
  float opacity = 1.0f;
  float padding = 0.0f;
};

constexpr char kVertexShaderSource[] = R"(
cbuffer FrameConstants : register(b0) {
  float2 viewport_size;
  float opacity;
  float padding;
};

struct VertexInput {
  float2 position : POSITION;
  float2 texcoord : TEXCOORD0;
};

struct PixelInput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};

PixelInput Main(VertexInput input) {
  PixelInput output;
  float2 normalized;
  normalized.x = (input.position.x / viewport_size.x) * 2.0f - 1.0f;
  normalized.y = 1.0f - (input.position.y / viewport_size.y) * 2.0f;
  output.position = float4(normalized, 0.0f, 1.0f);
  output.texcoord = input.texcoord;
  return output;
}
)";

constexpr char kPixelShaderSource[] = R"(
cbuffer FrameConstants : register(b0) {
  float2 viewport_size;
  float opacity;
  float padding;
};

Texture2D source_texture : register(t0);
SamplerState linear_sampler : register(s0);

float4 Main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0)
    : SV_TARGET {
  float4 color = source_texture.Sample(linear_sampler, texcoord);
  color.a *= opacity;
  color.rgb *= color.a;
  return color;
}
)";

int RectWidth(const RECT& rect) { return static_cast<int>(rect.right - rect.left); }

int RectHeight(const RECT& rect) { return static_cast<int>(rect.bottom - rect.top); }

genie::animation::RectF RectToRectF(const RECT& rect) {
  return genie::animation::RectF{
      .left = static_cast<float>(rect.left),
      .top = static_cast<float>(rect.top),
      .right = static_cast<float>(rect.right),
      .bottom = static_cast<float>(rect.bottom),
  };
}

RECT AnimationSurfaceRect(const genie::animation::RectF& source,
                          const genie::animation::RectF& target, const RECT& virtual_screen) {
  RECT requested{
      .left = static_cast<LONG>(std::floor(std::min(source.left, target.left))) - kOverlayPadding,
      .top = static_cast<LONG>(std::floor(std::min(source.top, target.top))) - kOverlayPadding,
      .right = static_cast<LONG>(std::ceil(std::max(source.right, target.right))) + kOverlayPadding,
      .bottom =
          static_cast<LONG>(std::ceil(std::max(source.bottom, target.bottom))) + kOverlayPadding,
  };
  RECT clipped{};
  if (!IntersectRect(&clipped, &requested, &virtual_screen) || RectWidth(clipped) <= 0 ||
      RectHeight(clipped) <= 0) {
    return RECT{};
  }
  return clipped;
}

std::wstring RectFTraceString(const genie::animation::RectF& rect) {
  std::wstringstream ss;
  ss << L"(" << rect.left << L"," << rect.top << L"," << rect.right << L"," << rect.bottom << L")";
  return ss.str();
}

HRESULT CompileShader(const char* source, const char* entry_point, const char* target,
                      ID3DBlob** byte_code) {
  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
  flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
  Microsoft::WRL::ComPtr<ID3DBlob> errors;
  const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry_point,
                                target, flags, 0, byte_code, &errors);
  if (FAILED(hr) && errors != nullptr) {
    std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
  }
  return hr;
}

void AllowCrossIntegrityMessage(HWND window, UINT message, const wchar_t* message_name) {
  (void)message_name;
  if (window == nullptr || message == 0) {
    return;
  }

  CHANGEFILTERSTRUCT filter_status{};
  filter_status.cbSize = sizeof(filter_status);
  if (!ChangeWindowMessageFilterEx(window, message, MSGFLT_ALLOW, &filter_status)) {
    LogDebug(L"Overlay", std::wstring(L"ChangeWindowMessageFilterEx failed for ") + message_name +
                             L" message=" + std::to_wstring(message) + L" error=" +
                             std::to_wstring(GetLastError()));
    return;
  }

  LogDebug(L"Overlay", std::wstring(L"Allowed cross-integrity window message ") + message_name +
                           L" message=" + std::to_wstring(message));
}

}  // namespace

OverlayWindow::~OverlayWindow() { Shutdown(); }

bool OverlayWindow::Initialize(HINSTANCE instance, D3dDevice* d3d_device,
                               MinimizeCallback minimize_callback,
                               RestoreCallback restore_callback) {
  d3d_device_ = d3d_device;
  minimize_callback_ = std::move(minimize_callback);
  restore_callback_ = std::move(restore_callback);
  minimize_attempt_message_ = RegisterWindowMessageW(L"GenieMinimizeAttempt");
  restore_attempt_message_ = RegisterWindowMessageW(L"GenieRestoreAttempt");
  virtual_screen_rect_ = platform::GetVirtualScreenRect();
  overlay_screen_rect_ = RECT{virtual_screen_rect_.left, virtual_screen_rect_.top,
                              virtual_screen_rect_.left + 1, virtual_screen_rect_.top + 1};
  width_ = 1;
  height_ = 1;

  if (!RegisterWindowClass(instance) || !CreateOverlayWindow(instance) ||
      !InitializeComposition() || !CreateRenderTarget() || !CreateRenderResources()) {
    Shutdown();
    return false;
  }

  AllowCrossIntegrityMessage(window_, minimize_attempt_message_, L"GenieMinimizeAttempt");
  AllowCrossIntegrityMessage(window_, restore_attempt_message_, L"GenieRestoreAttempt");

  ClearFrame();
  ShowWindow(window_, SW_SHOWNOACTIVATE);
  return true;
}

void OverlayWindow::Shutdown() {
  animation_state_.active = false;
  mesh_generator_ = genie::animation::GenieMeshGenerator{};
  reusable_mesh_ = genie::animation::GenieMesh{};
  index_count_ = 0;
  if (window_ != nullptr) {
    DestroyWindow(window_);
    window_ = nullptr;
  }
  rasterizer_state_.Reset();
  blend_state_.Reset();
  sampler_state_.Reset();
  constant_buffer_.Reset();
  index_buffer_.Reset();
  vertex_buffer_.Reset();
  input_layout_.Reset();
  pixel_shader_.Reset();
  vertex_shader_.Reset();
  render_target_view_.Reset();
  composition_visual_.Reset();
  composition_target_.Reset();
  composition_device_.Reset();
  swap_chain_.Reset();
}

bool OverlayWindow::StartAnimation(CapturedTexture captured_texture,
                                   const genie::animation::RectF& source_screen_rect,
                                   const genie::animation::RectF& target_screen_rect,
                                   genie::animation::GenieEdge edge, float start_progress,
                                   float target_progress) {
  LogTrace(L"Overlay", L"StartAnimation requested source=" + RectFTraceString(source_screen_rect) +
                           L" target=" + RectFTraceString(target_screen_rect) +
                           L" start_progress=" + std::to_wstring(start_progress) +
                           L" target_progress=" + std::to_wstring(target_progress) + L" edge=" +
                           std::to_wstring(static_cast<int>(edge)) + L" has_srv=" +
                           std::to_wstring(captured_texture.shader_resource_view != nullptr));

  if (captured_texture.shader_resource_view == nullptr) {
    LogTrace(L"Overlay", L"StartAnimation failed: missing shader resource view");
    std::wcerr << L"Overlay start failed: captured texture has no shader "
                  L"resource view.\n";
    return false;
  }

  const RECT animation_surface =
      AnimationSurfaceRect(source_screen_rect, target_screen_rect, virtual_screen_rect_);
  if (RectWidth(animation_surface) <= 0 || RectHeight(animation_surface) <= 0) {
    LogTrace(L"Overlay", L"StartAnimation failed: animation surface is outside virtual screen");
    return false;
  }

  HRGN hidden_region = CreateRectRgn(0, 0, 0, 0);
  genie::platform::SetOwnedWindowRegion(window_, hidden_region, false);
  if (!ResizeOverlaySurface(animation_surface)) {
    LogTrace(L"Overlay", L"StartAnimation failed: ResizeOverlaySurface returned false");
    HideOverlay();
    return false;
  }

  animation_state_.active = true;
  animation_state_.captured_texture = std::move(captured_texture);
  animation_state_.source_rect = ToOverlayRect(source_screen_rect);
  animation_state_.target_rect = ToOverlayRect(target_screen_rect);
  animation_state_.edge = edge;
  animation_state_.progress = std::clamp(start_progress, 0.0f, 1.0f);
  animation_state_.target_progress = std::clamp(target_progress, 0.0f, 1.0f);
  animation_state_.clock_started = false;

  if (!Render(animation_state_.progress)) {
    LogTrace(L"Overlay", L"StartAnimation failed: first Render returned false");
    animation_state_.active = false;
    animation_state_.captured_texture = CapturedTexture{};
    HideOverlay();
    std::wcerr << L"Overlay start failed: first frame render failed.\n";
    return false;
  }
  RECT target_rect_win{};
  target_rect_win.left = static_cast<LONG>(target_screen_rect.left);
  target_rect_win.top = static_cast<LONG>(target_screen_rect.top);
  target_rect_win.right = static_cast<LONG>(target_screen_rect.right);
  target_rect_win.bottom = static_cast<LONG>(target_screen_rect.bottom);

  HWND taskbar_hwnd = genie::platform::FindTaskbarWindowForRect(target_rect_win);

  if (taskbar_hwnd != nullptr) {
    SetWindowPos(taskbar_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
  }

  ApplyVisibleOverlayRegion(taskbar_hwnd);
  SetWindowPos(window_, HWND_TOPMOST, overlay_screen_rect_.left, overlay_screen_rect_.top,
               static_cast<int>(width_), static_cast<int>(height_),
               SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);

  DwmFlush();
  HRESULT hr = composition_device_->WaitForCommitCompletion();
  if (FAILED(hr)) {
    LogTrace(L"Overlay", L"StartAnimation failed: WaitForCommitCompletion hr=0x" +
                             std::to_wstring(static_cast<unsigned long>(hr)));
    animation_state_.active = false;
    animation_state_.captured_texture = CapturedTexture{};
    HideOverlay();
    std::wcerr << L"Overlay start failed: first frame commit did not "
                  L"complete: 0x"
               << std::hex << hr << std::dec << L"\n";
    return false;
  }
  LogTrace(L"Overlay", L"StartAnimation first frame visible overlay_source=" +
                           RectFTraceString(animation_state_.source_rect) + L" overlay_target=" +
                           RectFTraceString(animation_state_.target_rect));
  std::wcout << L"Overlay first frame visible: source=(" << source_screen_rect.left << L","
             << source_screen_rect.top << L"," << source_screen_rect.right << L","
             << source_screen_rect.bottom << L") target=(" << target_screen_rect.left << L","
             << target_screen_rect.top << L"," << target_screen_rect.right << L","
             << target_screen_rect.bottom << L").\n";
  return true;
}

void OverlayWindow::StartAnimationClock() {
  if (!animation_state_.active) {
    LogTrace(L"Overlay", L"StartAnimationClock ignored: animation inactive");
    return;
  }
  animation_state_.last_tick_time = std::chrono::steady_clock::now();
  animation_state_.clock_started = true;
  LogTrace(L"Overlay", L"StartAnimationClock progress=" +
                           std::to_wstring(animation_state_.progress) + L" target=" +
                           std::to_wstring(animation_state_.target_progress));
}

void OverlayWindow::ContinueMinimizeAnimation() {
  if (!animation_state_.active) {
    return;
  }
  animation_state_.target_progress = 1.0f;
  animation_state_.last_tick_time = std::chrono::steady_clock::now();
  animation_state_.clock_started = true;
  LogTrace(L"Overlay",
           L"ContinueMinimizeAnimation progress=" + std::to_wstring(animation_state_.progress));
  std::wcout << L"Overlay animation continuing minimize at progress=" << animation_state_.progress
             << L".\n";
}

void OverlayWindow::ReverseAnimation() {
  if (!animation_state_.active) {
    return;
  }
  animation_state_.target_progress = 0.0f;
  animation_state_.last_tick_time = std::chrono::steady_clock::now();
  animation_state_.clock_started = true;
  LogTrace(L"Overlay", L"ReverseAnimation progress=" + std::to_wstring(animation_state_.progress));
  std::wcout << L"Overlay animation reversing at progress=" << animation_state_.progress << L".\n";
}

bool OverlayWindow::Tick() {
  if (!animation_state_.active) {
    return false;
  }
  if (!animation_state_.clock_started) {
    if (IsTraceLoggingEnabled()) {
      LogTrace(L"Overlay", L"Tick waiting_for_clock progress=" +
                               std::to_wstring(animation_state_.progress) + L" target=" +
                               std::to_wstring(animation_state_.target_progress));
    }
    return true;
  }

  const auto now = std::chrono::steady_clock::now();
  const float elapsed_seconds =
      std::chrono::duration<float>(now - animation_state_.last_tick_time).count();
  animation_state_.last_tick_time = now;

  const float step = elapsed_seconds / animation_duration_seconds_;
  [[maybe_unused]] const float previous_progress = animation_state_.progress;
  if (animation_state_.target_progress >= animation_state_.progress) {
    animation_state_.progress =
        std::min(animation_state_.target_progress, animation_state_.progress + step);
  } else {
    animation_state_.progress =
        std::max(animation_state_.target_progress, animation_state_.progress - step);
  }

  if (!Render(animation_state_.progress)) {
    LogTrace(L"Overlay", L"Tick render_failed elapsed_ms=" +
                             std::to_wstring(elapsed_seconds * 1000.0f) + L" previous_progress=" +
                             std::to_wstring(previous_progress) + L" progress=" +
                             std::to_wstring(animation_state_.progress) + L" target=" +
                             std::to_wstring(animation_state_.target_progress));
    animation_state_.active = false;
    animation_state_.captured_texture = CapturedTexture{};
    HideOverlay();
    return false;
  }

  if (IsTraceLoggingEnabled()) {
    LogTrace(L"Overlay", L"Tick rendered elapsed_ms=" + std::to_wstring(elapsed_seconds * 1000.0f) +
                             L" step=" + std::to_wstring(step) + L" previous_progress=" +
                             std::to_wstring(previous_progress) + L" progress=" +
                             std::to_wstring(animation_state_.progress) + L" target=" +
                             std::to_wstring(animation_state_.target_progress));
  }

  if (animation_state_.progress == animation_state_.target_progress) {
    LogTrace(L"Overlay",
             L"Tick reached target progress=" + std::to_wstring(animation_state_.progress));
    animation_state_.active = false;
    if (animation_state_.target_progress != 0.0f) {
      animation_state_.captured_texture = CapturedTexture{};
      HideOverlay();
    }
    return false;
  }

  return true;
}

void OverlayWindow::CancelAnimation() {
  if (animation_state_.active) {
    LogTrace(L"Overlay", L"CancelAnimation progress=" + std::to_wstring(animation_state_.progress) +
                             L" target=" + std::to_wstring(animation_state_.target_progress));
    animation_state_.active = false;
    animation_state_.captured_texture = CapturedTexture{};
    HideOverlay();
  }
}

void OverlayWindow::FinishRestoreAnimation() {
  LogTrace(L"Overlay", L"FinishRestoreAnimation");
  animation_state_.captured_texture = CapturedTexture{};
  HideOverlay();
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND window, UINT message, WPARAM w_param,
                                           LPARAM l_param) {
  OverlayWindow* overlay = nullptr;
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<const CREATESTRUCTW*>(l_param);
    overlay = static_cast<OverlayWindow*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
    overlay->window_ = window;
  } else {
    overlay = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  }

  if (overlay != nullptr) {
    return overlay->HandleMessage(message, w_param, l_param);
  }
  return DefWindowProcW(window, message, w_param, l_param);
}

LRESULT OverlayWindow::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
  if (message == minimize_attempt_message_ && minimize_attempt_message_ != 0) {
    HWND minimize_window = reinterpret_cast<HWND>(w_param);
    if (minimize_callback_ && minimize_callback_(minimize_window)) {
      return 1;
    }
    if (minimize_window != nullptr && IsWindow(minimize_window)) {
      SetPropW(minimize_window, kAllowMinimizeProperty, reinterpret_cast<HANDLE>(1));
      ShowWindow(minimize_window, SW_MINIMIZE);
      RemovePropW(minimize_window, kAllowMinimizeProperty);
      LogTrace(L"Overlay", L"Minimize callback failed; allowed native minimize fallback");
    }
    return 0;
  }

  if (message == restore_attempt_message_ && restore_attempt_message_ != 0) {
    HWND restore_window = reinterpret_cast<HWND>(w_param);
    if (restore_callback_ && restore_callback_(restore_window)) {
      return 1;
    }
    return 0;
  }

  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_NCHITTEST:
      return HTTRANSPARENT;
    case WM_DESTROY:
      window_ = nullptr;
      return 0;
    default:
      return DefWindowProcW(window_, message, w_param, l_param);
  }
}

bool OverlayWindow::RegisterWindowClass(HINSTANCE instance) {
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.style = CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = &OverlayWindow::WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.lpszClassName = kOverlayWindowClassName;

  if (RegisterClassExW(&window_class) == 0) {
    const DWORD error = GetLastError();
    return error == ERROR_CLASS_ALREADY_EXISTS;
  }
  return true;
}

bool OverlayWindow::CreateOverlayWindow(HINSTANCE instance) {
  constexpr DWORD kExStyle =
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED;
  window_ =
      CreateWindowExW(kExStyle, kOverlayWindowClassName, L"Genie Effect Overlay", WS_POPUP,
                      overlay_screen_rect_.left, overlay_screen_rect_.top, static_cast<int>(width_),
                      static_cast<int>(height_), nullptr, nullptr, instance, this);
  return window_ != nullptr;
}

bool OverlayWindow::InitializeComposition() {
  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
  swap_chain_desc.Width = width_;
  swap_chain_desc.Height = height_;
  swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  swap_chain_desc.Stereo = FALSE;
  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.BufferCount = 2;
  swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

  HRESULT hr = d3d_device_->factory()->CreateSwapChainForComposition(
      d3d_device_->device(), &swap_chain_desc, nullptr, &swap_chain_);
  if (FAILED(hr)) {
    std::wcerr << L"CreateSwapChainForComposition failed: 0x" << std::hex << hr << L"\n";
    return false;
  }

  hr = DCompositionCreateDevice(d3d_device_->dxgi_device(), IID_PPV_ARGS(&composition_device_));
  if (FAILED(hr)) {
    std::wcerr << L"DCompositionCreateDevice failed: 0x" << std::hex << hr << L"\n";
    return false;
  }

  hr = composition_device_->CreateTargetForHwnd(window_, TRUE, &composition_target_);
  if (FAILED(hr)) {
    std::wcerr << L"CreateTargetForHwnd failed: 0x" << std::hex << hr << L"\n";
    return false;
  }

  hr = composition_device_->CreateVisual(&composition_visual_);
  if (FAILED(hr)) {
    return false;
  }
  hr = composition_visual_->SetContent(swap_chain_.Get());
  if (FAILED(hr)) {
    return false;
  }
  hr = composition_target_->SetRoot(composition_visual_.Get());
  if (FAILED(hr)) {
    return false;
  }
  hr = composition_device_->Commit();
  return SUCCEEDED(hr);
}

bool OverlayWindow::CreateRenderTarget() {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
  HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
  if (FAILED(hr)) {
    return false;
  }

  hr = d3d_device_->device()->CreateRenderTargetView(back_buffer.Get(), nullptr,
                                                     &render_target_view_);
  if (FAILED(hr)) {
    std::wcerr << L"CreateRenderTargetView failed: 0x" << std::hex << hr << L"\n";
    return false;
  }
  return true;
}

bool OverlayWindow::ResizeOverlaySurface(const RECT& screen_rect) {
  const UINT new_width = static_cast<UINT>(RectWidth(screen_rect));
  const UINT new_height = static_cast<UINT>(RectHeight(screen_rect));
  if (new_width == 0 || new_height == 0 || swap_chain_ == nullptr || window_ == nullptr) {
    return false;
  }

  if (new_width != width_ || new_height != height_) {
    ID3D11DeviceContext* context = d3d_device_->context();
    context->OMSetRenderTargets(0, nullptr, nullptr);
    render_target_view_.Reset();
    const HRESULT hr = swap_chain_->ResizeBuffers(0, new_width, new_height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
      std::wcerr << L"ResizeBuffers for animation surface failed: 0x" << std::hex << hr << std::dec
                 << L"\n";
      return false;
    }
    width_ = new_width;
    height_ = new_height;
    if (!CreateRenderTarget()) {
      return false;
    }
    if (!UpdateFrameConstants()) {
      return false;
    }
  }

  overlay_screen_rect_ = screen_rect;
  return SetWindowPos(window_, HWND_TOPMOST, screen_rect.left, screen_rect.top,
                      static_cast<int>(new_width), static_cast<int>(new_height),
                      SWP_NOACTIVATE | SWP_NOOWNERZORDER) != FALSE;
}

void OverlayWindow::ApplyVisibleOverlayRegion(HWND taskbar_window) {
  HRGN visible_region = CreateRectRgn(0, 0, static_cast<int>(width_), static_cast<int>(height_));
  if (visible_region == nullptr) {
    return;
  }

  RECT taskbar_rect{};
  RECT overlap{};
  if (taskbar_window != nullptr && GetWindowRect(taskbar_window, &taskbar_rect) &&
      IntersectRect(&overlap, &overlay_screen_rect_, &taskbar_rect)) {
    OffsetRect(&overlap, -overlay_screen_rect_.left, -overlay_screen_rect_.top);
    HRGN taskbar_region = CreateRectRgn(overlap.left, overlap.top, overlap.right, overlap.bottom);
    if (taskbar_region != nullptr) {
      if (CombineRgn(visible_region, visible_region, taskbar_region, RGN_DIFF) == ERROR) {
        DeleteObject(taskbar_region);
        DeleteObject(visible_region);
        return;
      }
      DeleteObject(taskbar_region);
    }
  }

  genie::platform::SetOwnedWindowRegion(window_, visible_region, true);
}

bool OverlayWindow::CreateRenderResources() {
  if (!CompileShaders()) {
    return false;
  }

  D3D11_BUFFER_DESC constant_buffer_desc{};
  constant_buffer_desc.ByteWidth = sizeof(FrameConstants);
  constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  HRESULT hr =
      d3d_device_->device()->CreateBuffer(&constant_buffer_desc, nullptr, &constant_buffer_);
  if (FAILED(hr)) {
    return false;
  }

  D3D11_SAMPLER_DESC sampler_desc{};
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sampler_desc.MinLOD = 0.0f;
  sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
  hr = d3d_device_->device()->CreateSamplerState(&sampler_desc, &sampler_state_);
  if (FAILED(hr)) {
    return false;
  }

  D3D11_BLEND_DESC blend_desc{};
  blend_desc.RenderTarget[0].BlendEnable = TRUE;
  blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  hr = d3d_device_->device()->CreateBlendState(&blend_desc, &blend_state_);
  if (FAILED(hr)) {
    return false;
  }

  D3D11_RASTERIZER_DESC rasterizer_desc{};
  rasterizer_desc.FillMode = D3D11_FILL_SOLID;
  rasterizer_desc.CullMode = D3D11_CULL_NONE;
  rasterizer_desc.DepthClipEnable = TRUE;
  hr = d3d_device_->device()->CreateRasterizerState(&rasterizer_desc, &rasterizer_state_);
  if (FAILED(hr)) {
    return false;
  }

  D3D11_BUFFER_DESC vertex_buffer_desc{};
  vertex_buffer_desc.ByteWidth = kMaxMeshVertices * sizeof(genie::animation::MeshVertex);
  vertex_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertex_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  hr = d3d_device_->device()->CreateBuffer(&vertex_buffer_desc, nullptr, &vertex_buffer_);
  if (FAILED(hr)) {
    return false;
  }

  D3D11_BUFFER_DESC index_buffer_desc{};
  index_buffer_desc.ByteWidth = kMaxMeshIndices * sizeof(std::uint16_t);
  index_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  index_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  hr = d3d_device_->device()->CreateBuffer(&index_buffer_desc, nullptr, &index_buffer_);
  return SUCCEEDED(hr);
}

bool OverlayWindow::CompileShaders() {
  Microsoft::WRL::ComPtr<ID3DBlob> vertex_shader_blob;
  HRESULT hr = CompileShader(kVertexShaderSource, "Main", "vs_5_0", &vertex_shader_blob);
  if (FAILED(hr)) {
    return false;
  }

  hr = d3d_device_->device()->CreateVertexShader(vertex_shader_blob->GetBufferPointer(),
                                                 vertex_shader_blob->GetBufferSize(), nullptr,
                                                 &vertex_shader_);
  if (FAILED(hr)) {
    return false;
  }

  constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 2> kInputElements = {
      D3D11_INPUT_ELEMENT_DESC{
          .SemanticName = "POSITION",
          .SemanticIndex = 0,
          .Format = DXGI_FORMAT_R32G32_FLOAT,
          .InputSlot = 0,
          .AlignedByteOffset = 0,
          .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
          .InstanceDataStepRate = 0,
      },
      D3D11_INPUT_ELEMENT_DESC{
          .SemanticName = "TEXCOORD",
          .SemanticIndex = 0,
          .Format = DXGI_FORMAT_R32G32_FLOAT,
          .InputSlot = 0,
          .AlignedByteOffset = 8,
          .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
          .InstanceDataStepRate = 0,
      },
  };
  hr = d3d_device_->device()->CreateInputLayout(
      kInputElements.data(), static_cast<UINT>(kInputElements.size()),
      vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &input_layout_);
  if (FAILED(hr)) {
    return false;
  }

  Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_blob;
  hr = CompileShader(kPixelShaderSource, "Main", "ps_5_0", &pixel_shader_blob);
  if (FAILED(hr)) {
    return false;
  }

  hr = d3d_device_->device()->CreatePixelShader(pixel_shader_blob->GetBufferPointer(),
                                                pixel_shader_blob->GetBufferSize(), nullptr,
                                                &pixel_shader_);
  return SUCCEEDED(hr);
}

bool OverlayWindow::UploadMesh(const genie::animation::GenieMesh& mesh, bool upload_indices) {
  if (mesh.vertices.empty() || mesh.indices.empty() || mesh.vertices.size() > kMaxMeshVertices ||
      mesh.indices.size() > kMaxMeshIndices) {
    return false;
  }

  ID3D11DeviceContext* context = d3d_device_->context();
  D3D11_MAPPED_SUBRESOURCE mapped_resource{};
  HRESULT hr = context->Map(vertex_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
  if (FAILED(hr)) {
    return false;
  }
  std::memcpy(mapped_resource.pData, mesh.vertices.data(),
              mesh.vertices.size() * sizeof(genie::animation::MeshVertex));
  context->Unmap(vertex_buffer_.Get(), 0);

  if (upload_indices) {
    hr = context->Map(index_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
    if (FAILED(hr)) {
      return false;
    }
    std::memcpy(mapped_resource.pData, mesh.indices.data(),
                mesh.indices.size() * sizeof(std::uint16_t));
    context->Unmap(index_buffer_.Get(), 0);
  }
  index_count_ = static_cast<UINT>(mesh.indices.size());
  return true;
}

bool OverlayWindow::UpdateFrameConstants() {
  if (constant_buffer_ == nullptr) {
    return false;
  }
  D3D11_MAPPED_SUBRESOURCE mapped_resource{};
  ID3D11DeviceContext* context = d3d_device_->context();
  const HRESULT hr =
      context->Map(constant_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
  if (FAILED(hr)) {
    return false;
  }
  auto* constants = static_cast<FrameConstants*>(mapped_resource.pData);
  constants->viewport_size[0] = static_cast<float>(width_);
  constants->viewport_size[1] = static_cast<float>(height_);
  constants->opacity = 1.0f;
  constants->padding = 0.0f;
  context->Unmap(constant_buffer_.Get(), 0);
  return true;
}

bool OverlayWindow::Render(float progress) {
  if (!animation_state_.active || render_target_view_ == nullptr) {
    return false;
  }

  const bool indices_changed = mesh_generator_.GenerateInto(
      animation_state_.source_rect, animation_state_.target_rect, animation_state_.edge,
      genie::animation::GenieDirection::kMinimize, progress, static_cast<float>(height_),
      &reusable_mesh_);
  if (!UploadMesh(reusable_mesh_, indices_changed)) {
    std::wcerr << L"Overlay render failed: mesh upload failed. vertices="
               << reusable_mesh_.vertices.size() << L" indices=" << reusable_mesh_.indices.size()
               << L"\n";
    return false;
  }

  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(width_);
  viewport.Height = static_cast<float>(height_);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  constexpr std::array<float, 4> kClearColor = {0.0f, 0.0f, 0.0f, 0.0f};
  ID3D11DeviceContext* context = d3d_device_->context();
  context->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);
  context->ClearRenderTargetView(render_target_view_.Get(), kClearColor.data());
  context->RSSetViewports(1, &viewport);
  context->RSSetState(rasterizer_state_.Get());

  constexpr UINT stride = sizeof(genie::animation::MeshVertex);
  constexpr UINT offset = 0;
  ID3D11Buffer* vertex_buffer = vertex_buffer_.Get();
  context->IASetInputLayout(input_layout_.Get());
  context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
  context->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
  context->VSSetConstantBuffers(0, 1, constant_buffer_.GetAddressOf());
  context->PSSetShader(pixel_shader_.Get(), nullptr, 0);
  context->PSSetConstantBuffers(0, 1, constant_buffer_.GetAddressOf());
  context->PSSetShaderResources(
      0, 1, animation_state_.captured_texture.shader_resource_view.GetAddressOf());
  context->PSSetSamplers(0, 1, sampler_state_.GetAddressOf());

  constexpr std::array<float, 4> kBlendFactor = {0.0f, 0.0f, 0.0f, 0.0f};
  context->OMSetBlendState(blend_state_.Get(), kBlendFactor.data(), 0xffffffff);
  context->DrawIndexed(index_count_, 0, 0);

  ID3D11ShaderResourceView* null_resource = nullptr;
  context->PSSetShaderResources(0, 1, &null_resource);
  HRESULT hr = swap_chain_->Present(0, 0);
  if (FAILED(hr)) {
    std::wcerr << L"Overlay render failed: Present failed: 0x" << std::hex << hr << std::dec
               << L"\n";
    return false;
  }
  hr = composition_device_->Commit();
  if (FAILED(hr)) {
    std::wcerr << L"Overlay render failed: DComposition Commit failed: 0x" << std::hex << hr
               << std::dec << L"\n";
    return false;
  }
  return true;
}

void OverlayWindow::ClearFrame() {
  if (render_target_view_ == nullptr || swap_chain_ == nullptr) {
    return;
  }
  constexpr std::array<float, 4> kClearColor = {0.0f, 0.0f, 0.0f, 0.0f};
  ID3D11DeviceContext* context = d3d_device_->context();
  context->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);
  context->ClearRenderTargetView(render_target_view_.Get(), kClearColor.data());
  swap_chain_->Present(0, 0);
  composition_device_->Commit();
}

void OverlayWindow::HideOverlay() {
  if (window_ != nullptr) {
    ClearFrame();
    HRGN hidden_region = CreateRectRgn(0, 0, 0, 0);
    genie::platform::SetOwnedWindowRegion(window_, hidden_region, true);
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
  }
}

genie::animation::RectF OverlayWindow::ToOverlayRect(
    const genie::animation::RectF& screen_rect) const {
  const genie::animation::RectF overlay_screen = RectToRectF(overlay_screen_rect_);
  return genie::animation::RectF{
      .left = screen_rect.left - overlay_screen.left,
      .top = screen_rect.top - overlay_screen.top,
      .right = screen_rect.right - overlay_screen.left,
      .bottom = screen_rect.bottom - overlay_screen.top,
  };
}

}  // namespace genie::rendering
