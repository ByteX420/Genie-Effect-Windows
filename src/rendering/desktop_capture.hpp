#pragma once

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

#include "animation/geometry.hpp"
#include "rendering/d3d_device.hpp"

namespace genie::rendering {

struct CapturedTexture {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
  genie::animation::SizeF size;
};

class DesktopCapture {
public:
  explicit DesktopCapture(D3dDevice* d3d_device);

  DesktopCapture(const DesktopCapture&) = delete;
  DesktopCapture& operator=(const DesktopCapture&) = delete;

  [[nodiscard]] bool CaptureRegion(const RECT& screen_rect, CapturedTexture* captured_texture);
  [[nodiscard]] bool CaptureWindow(HWND window, const RECT& requested_screen_rect,
                                   CapturedTexture* captured_texture, RECT* captured_screen_rect);
  [[nodiscard]] bool RefreshCapturedTexture(const RECT& screen_rect,
                                            CapturedTexture* captured_texture);
  void ClearHistory() {
    for (auto& output : outputs_) {
      output.latest_frame.Reset();
    }
  }
  [[nodiscard]] bool device_lost() const { return device_lost_; }
  void ClearDeviceLost() { device_lost_ = false; }

private:
  struct OutputCapture {
    RECT desktop_coordinates{};
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> latest_frame;
    DXGI_FORMAT latest_frame_format = DXGI_FORMAT_UNKNOWN;
  };

  enum class AcquireResult {
    kAcquired,
    kNoNewFrame,
    kAccessLost,
    kDeviceLost,
    kFailed,
  };

  [[nodiscard]] bool InitializeOutputs();
  [[nodiscard]] OutputCapture* FindOutputForRect(const RECT& screen_rect);
  [[nodiscard]] OutputCapture* AcquireFrameForRect(const RECT& screen_rect,
                                                   UINT first_frame_timeout_ms);
  AcquireResult TryAcquireLatestFrame(OutputCapture* output, UINT timeout_ms);
  [[nodiscard]] bool CopyRegionFromFrame(OutputCapture* output, const RECT& screen_rect,
                                         CapturedTexture* captured_texture);
  [[nodiscard]] bool CopyRegionIntoTexture(OutputCapture* output, const RECT& screen_rect,
                                           CapturedTexture* captured_texture);
  void ResetOutputs();
  void MarkDeviceLost(const wchar_t* context, HRESULT hr);

  D3dDevice* d3d_device_ = nullptr;
  std::vector<OutputCapture> outputs_;
  bool device_lost_ = false;
};

}  // namespace genie::rendering
