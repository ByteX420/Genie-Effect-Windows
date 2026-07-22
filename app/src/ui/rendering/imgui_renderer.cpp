#include "pch.hpp"

#include "ui/rendering/imgui_renderer.hpp"

#include <algorithm>
#include <cmath>

#include "app/resource.hpp"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "core/environment.hpp"
#include "imgui.h"
#include "misc/freetype/imgui_freetype.h"
#include "ui/theme/theme.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace minimize::ui::rendering {
namespace {

constexpr DWORD kInitialRecoveryDelayMs = 250;
constexpr DWORD kMaximumRecoveryDelayMs = 4000;
constexpr float kSmallFontSize = 13.0f;
constexpr float kBodyFontSize = 15.0f;
constexpr float kMediumFontSize = 15.0f;
constexpr float kTitleFontSize = 22.0f;

struct EmbeddedResource {
  void* data = nullptr;
  int size = 0;
};

EmbeddedResource LoadEmbeddedResource(int resource_id) {
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(resource_id), MAKEINTRESOURCEW(10));
  if (resource == nullptr) return {};
  HGLOBAL loaded = LoadResource(instance, resource);
  if (loaded == nullptr) return {};
  return {LockResource(loaded), static_cast<int>(SizeofResource(instance, resource))};
}

}  // namespace

ImguiRenderer::~ImguiRenderer() { Shutdown(); }

bool ImguiRenderer::Initialize(HWND window) {
  window_ = window;
  if (window_ == nullptr || !CreateDeviceResources()) return false;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  context_ready_ = true;
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  dpi_ = GetDpiForWindow(window_);
  scale_ = static_cast<float>(dpi_) / USER_DEFAULT_SCREEN_DPI;
  RebuildFonts();
  ApplyStyle();
  if (!ImGui_ImplWin32_Init(window_)) return false;
  win32_ready_ = true;
  if (!ImGui_ImplDX11_Init(device_.Get(), context_.Get())) return false;
  dx11_ready_ = true;
#ifdef _DEBUG
  recovery_test_pending_ = core::EnvironmentFlagEnabled("MINIMIZE_TEST_DEVICE_RECOVERY");
#endif
  return true;
}

void ImguiRenderer::Shutdown() {
  if (dx11_ready_) ImGui_ImplDX11_Shutdown();
  if (win32_ready_) ImGui_ImplWin32_Shutdown();
  if (context_ready_) ImGui::DestroyContext();
  dx11_ready_ = win32_ready_ = context_ready_ = false;
  ReleaseDeviceResources();
  window_ = nullptr;
}

bool ImguiRenderer::BeginFrame() {
#ifdef _DEBUG
  if (recovery_test_pending_) {
    recovery_test_pending_ = false;
    HandleDeviceLost();
  }
#endif
  if (recovery_pending_ && !TryRecoverDeviceResources()) return false;
  if (!ready()) return false;
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  return true;
}

void ImguiRenderer::EndFrame() {
  if (!ready()) return;
  ImGui::Render();
  constexpr float clear_color[] = {0.0f, 0.0f, 0.0f, 0.0f};
  context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);
  context_->ClearRenderTargetView(render_target_view_.Get(), clear_color);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  const HRESULT result = swap_chain_->Present(1, 0);
  if (IsDeviceLostError(result)) HandleDeviceLost();
}

void ImguiRenderer::Resize(UINT width, UINT height) {
  if (swap_chain_ == nullptr || width == 0 || height == 0) return;
  context_->OMSetRenderTargets(0, nullptr, nullptr);
  context_->ClearState();
  render_target_view_.Reset();
  const HRESULT result = swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
  if (IsDeviceLostError(result)) {
    HandleDeviceLost();
  } else if (SUCCEEDED(result)) {
    if (!CreateRenderTarget() && device_ != nullptr && FAILED(device_->GetDeviceRemovedReason())) {
      HandleDeviceLost();
    }
  } else if (device_ != nullptr && FAILED(device_->GetDeviceRemovedReason())) {
    HandleDeviceLost();
  } else {
    CreateRenderTarget();
  }
}

bool ImguiRenderer::HandleWin32Message(HWND window, UINT message, WPARAM w_param,
                                       LPARAM l_param) const {
  return context_ready_ && ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param) != 0;
}

void ImguiRenderer::UpdateDpi(UINT dpi) {
  if (dpi == 0 || dpi == dpi_) return;
  dpi_ = dpi;
  scale_ = static_cast<float>(dpi_) / USER_DEFAULT_SCREEN_DPI;
  RebuildFonts();
  ApplyStyle();
}

bool ImguiRenderer::ready() const {
  return context_ready_ && win32_ready_ && dx11_ready_ && render_target_view_ != nullptr &&
         !recovery_pending_;
}

bool ImguiRenderer::CreateDeviceResources() {
  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferCount = 2;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.OutputWindow = window_;
  desc.SampleDesc.Count = 1;
  desc.Windowed = TRUE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  constexpr D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
  D3D_FEATURE_LEVEL level{};
  const HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                                       0, levels, 1, D3D11_SDK_VERSION, &desc,
                                                       &swap_chain_, &device_, &level, &context_);
  return SUCCEEDED(result) && CreateRenderTarget();
}

bool ImguiRenderer::CreateRenderTarget() {
  render_target_view_.Reset();
  if (device_ == nullptr || swap_chain_ == nullptr) return false;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
  if (FAILED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return false;
  return SUCCEEDED(
             device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_)) &&
         render_target_view_ != nullptr;
}

void ImguiRenderer::ReleaseDeviceResources() {
  if (context_ != nullptr) {
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    context_->ClearState();
  }
  render_target_view_.Reset();
  swap_chain_.Reset();
  context_.Reset();
  device_.Reset();
}

void ImguiRenderer::HandleDeviceLost() {
  if (dx11_ready_) ImGui_ImplDX11_Shutdown();
  dx11_ready_ = false;
  ReleaseDeviceResources();
  recovery_pending_ = true;
  recovery_delay_ms_ = kInitialRecoveryDelayMs;
  next_recovery_ms_ = GetTickCount64();
  TryRecoverDeviceResources();
}

bool ImguiRenderer::TryRecoverDeviceResources() {
  if (!recovery_pending_) return true;
  const ULONGLONG now = GetTickCount64();
  if (now < next_recovery_ms_) return false;
  if (CreateDeviceResources() && ImGui_ImplDX11_Init(device_.Get(), context_.Get())) {
    dx11_ready_ = true;
    recovery_pending_ = false;
    recovery_delay_ms_ = kInitialRecoveryDelayMs;
    return true;
  }
  ReleaseDeviceResources();
  next_recovery_ms_ = now + recovery_delay_ms_;
  recovery_delay_ms_ = std::min(recovery_delay_ms_ * 2, kMaximumRecoveryDelayMs);
  return false;
}

bool ImguiRenderer::IsDeviceLostError(HRESULT result) {
  return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
         result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

void ImguiRenderer::ApplyStyle() const { ::minimize::ui::theme::ApplyStyle(scale_); }

void ImguiRenderer::RebuildFonts() {
  ImGuiIO& io = ImGui::GetIO();
  if (dx11_ready_) ImGui_ImplDX11_InvalidateDeviceObjects();
  io.Fonts->Clear();
#ifdef IMGUI_ENABLE_FREETYPE
  io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;
#endif
  ImFontConfig config{};
#ifdef IMGUI_ENABLE_FREETYPE
  config.OversampleH = 1;
  config.OversampleV = 1;
#else
  config.OversampleH = 3;
  config.OversampleV = 2;
#endif
  config.PixelSnapH = true;
  config.FontDataOwnedByAtlas = false;
  const auto size = [this](float logical) { return std::max(1.0f, std::round(logical * scale_)); };
  const EmbeddedResource regular = LoadEmbeddedResource(IDR_UI_FONT_REGULAR);
  const EmbeddedResource semibold = LoadEmbeddedResource(IDR_UI_FONT_SEMIBOLD);
  const EmbeddedResource bold = LoadEmbeddedResource(IDR_UI_FONT_BOLD);
  const auto add = [&config](const EmbeddedResource& resource, float font_size) {
    return resource.data == nullptr ? static_cast<ImFont*>(nullptr)
                                    : ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
                                          resource.data, resource.size, font_size, &config);
  };
  small_font_ = add(regular, size(kSmallFontSize));
  body_font_ = add(regular, size(kBodyFontSize));
  medium_font_ = add(semibold, size(kMediumFontSize));
  title_font_ = add(bold, size(kTitleFontSize));
  if (title_font_ == nullptr) title_font_ = add(semibold, size(kTitleFontSize));
  if (body_font_ == nullptr || small_font_ == nullptr || medium_font_ == nullptr ||
      title_font_ == nullptr) {
    ImFontConfig file_config = config;
    file_config.FontDataOwnedByAtlas = true;
    const auto add_file = [&file_config](const char* path, float font_size) {
      return ImGui::GetIO().Fonts->AddFontFromFileTTF(path, font_size, &file_config);
    };
    if (small_font_ == nullptr)
      small_font_ = add_file("assets/fonts/Inter-Regular.ttf", size(kSmallFontSize));
    if (body_font_ == nullptr)
      body_font_ = add_file("assets/fonts/Inter-Regular.ttf", size(kBodyFontSize));
    if (medium_font_ == nullptr)
      medium_font_ = add_file("assets/fonts/Inter-SemiBold.ttf", size(kMediumFontSize));
    if (title_font_ == nullptr)
      title_font_ = add_file("assets/fonts/Inter-Bold.ttf", size(kTitleFontSize));
    if (title_font_ == nullptr)
      title_font_ = add_file("assets/fonts/Inter-SemiBold.ttf", size(kTitleFontSize));
  }
  if (body_font_ == nullptr) body_font_ = io.Fonts->AddFontDefault();
  if (small_font_ == nullptr) small_font_ = body_font_;
  if (medium_font_ == nullptr) medium_font_ = body_font_;
  if (title_font_ == nullptr) title_font_ = medium_font_;
  io.FontDefault = body_font_;
  if (dx11_ready_) ImGui_ImplDX11_CreateDeviceObjects();
}

}  // namespace minimize::ui::rendering
