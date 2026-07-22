#pragma once

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

#include "animation/geometry.hpp"
#include "rendering/d3d_device.hpp"
#include "rendering/desktop_duplication_session.hpp"

namespace minimize::rendering {

struct CapturedTexture {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
  minimize::animation::SizeF size;
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
  void ClearHistory() { duplication_session_.ClearHistory(); }
  [[nodiscard]] bool device_lost() const {
    return device_lost_ || duplication_session_.device_lost();
  }
  void ClearDeviceLost() {
    device_lost_ = false;
    duplication_session_.ClearDeviceLost();
  }

private:
  using OutputCapture = DesktopDuplicationSession::OutputCapture;
  [[nodiscard]] bool CopyRegionFromFrame(OutputCapture* output, const RECT& screen_rect,
                                         CapturedTexture* captured_texture);
  [[nodiscard]] bool CopyRegionIntoTexture(OutputCapture* output, const RECT& screen_rect,
                                           CapturedTexture* captured_texture);
  void MarkDeviceLost(const wchar_t* context, HRESULT hr);

  D3dDevice* d3d_device_ = nullptr;
  DesktopDuplicationSession duplication_session_;
  bool device_lost_ = false;
};

}  // namespace minimize::rendering
