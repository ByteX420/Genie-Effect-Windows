#include "pch.hpp"

#include "rendering/d3d_device.hpp"

#include <array>
#include <iostream>

namespace genie::rendering {
namespace {

HRESULT CreateHardwareDevice(UINT flags, ID3D11Device** device, ID3D11DeviceContext** context) {
  constexpr std::array<D3D_FEATURE_LEVEL, 2> kFeatureLevels = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
  };
  D3D_FEATURE_LEVEL selected_feature_level = D3D_FEATURE_LEVEL_11_0;
  return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, kFeatureLevels.data(),
                           static_cast<UINT>(kFeatureLevels.size()), D3D11_SDK_VERSION, device,
                           &selected_feature_level, context);
}

}  // namespace

std::unique_ptr<D3dDevice> D3dDevice::Create() {
  auto result = std::unique_ptr<D3dDevice>(new D3dDevice());

  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  HRESULT hr =
      CreateHardwareDevice(flags, result->device_.GetAddressOf(), result->context_.GetAddressOf());
#if defined(_DEBUG)
  if (FAILED(hr)) {
    flags &= ~D3D11_CREATE_DEVICE_DEBUG;
    hr = CreateHardwareDevice(flags, result->device_.ReleaseAndGetAddressOf(),
                              result->context_.ReleaseAndGetAddressOf());
  }
#endif
  if (FAILED(hr)) {
    std::wcerr << L"D3D11CreateDevice failed: 0x" << std::hex << hr << L"\n";
    return nullptr;
  }

  hr = result->device_.As(&result->dxgi_device_);
  if (FAILED(hr)) {
    std::wcerr << L"Querying IDXGIDevice failed: 0x" << std::hex << hr << L"\n";
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  hr = result->dxgi_device_->GetAdapter(&adapter);
  if (FAILED(hr)) {
    std::wcerr << L"IDXGIDevice::GetAdapter failed: 0x" << std::hex << hr << L"\n";
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
  hr = adapter->GetParent(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    std::wcerr << L"IDXGIAdapter::GetParent failed: 0x" << std::hex << hr << L"\n";
    return nullptr;
  }

  result->factory_ = factory;
  return result;
}

bool D3dDevice::IsDeviceLostError(HRESULT hr) {
  return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
         hr == DXGI_ERROR_DEVICE_HUNG || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

HRESULT D3dDevice::DeviceRemovedReason() const {
  return device_ != nullptr ? device_->GetDeviceRemovedReason() : DXGI_ERROR_DEVICE_REMOVED;
}

bool D3dDevice::IsDeviceLost(HRESULT operation_result) const {
  return IsDeviceLostError(operation_result) || FAILED(DeviceRemovedReason());
}

}  // namespace genie::rendering
