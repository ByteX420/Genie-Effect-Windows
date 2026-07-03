#pragma once

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <memory>
#include <wrl/client.h>

namespace genie::rendering {

class D3dDevice {
public:
  static std::unique_ptr<D3dDevice> Create();

  D3dDevice(const D3dDevice&) = delete;
  D3dDevice& operator=(const D3dDevice&) = delete;

  [[nodiscard]] ID3D11Device* device() const { return device_.Get(); }
  [[nodiscard]] ID3D11DeviceContext* context() const { return context_.Get(); }
  [[nodiscard]] IDXGIDevice* dxgi_device() const { return dxgi_device_.Get(); }
  [[nodiscard]] IDXGIFactory2* factory() const { return factory_.Get(); }

private:
  D3dDevice() = default;

  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device_;
  Microsoft::WRL::ComPtr<IDXGIFactory2> factory_;
};

}  // namespace genie::rendering
