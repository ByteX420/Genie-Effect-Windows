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
  void RefreshFrames(UINT timeout_ms = 0);
  void ClearHistory() {
    for (auto& output : outputs_) {
      output.frame_history.clear();
    }
  }
  [[nodiscard]] bool device_lost() const { return device_lost_; }
  void ClearDeviceLost() { device_lost_ = false; }

private:
  static constexpr size_t kHistorySize = 4;

  struct OutputCapture {
    RECT desktop_coordinates{};
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> frame_history;
    size_t current_frame_index = 0;
    DXGI_FORMAT latest_frame_format = DXGI_FORMAT_UNKNOWN;
  };

  [[nodiscard]] bool InitializeOutputs();
  [[nodiscard]] OutputCapture* FindOutputForRect(const RECT& screen_rect);
  bool TryAcquireLatestFrame(OutputCapture* output, UINT timeout_ms);
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
