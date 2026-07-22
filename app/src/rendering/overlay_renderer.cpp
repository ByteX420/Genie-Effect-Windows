#include "pch.hpp"

#include "rendering/overlay_renderer.hpp"

#include <array>
#include <cstring>
#include <d3dcompiler.h>

#include "rendering/d3d_device.hpp"

namespace minimize::rendering {
namespace {

constexpr UINT kMaximumMeshVertices = 102;
constexpr UINT kMaximumMeshIndices = 300;

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
float4 Main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0) : SV_TARGET {
  float4 color = source_texture.Sample(linear_sampler, texcoord);
  color.a *= opacity;
  color.rgb *= color.a;
  return color;
}
)";

HRESULT CompileShader(const char* source, const char* entry_point, const char* target,
                      ID3DBlob** byte_code) {
  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
  flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
  return D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry_point, target,
                    flags, 0, byte_code, nullptr);
}

}  // namespace

bool OverlayRenderer::Initialize(D3dDevice* device) {
  Shutdown();
  device_ = device;
  if (device_ == nullptr || !CompileShaders()) return false;
  D3D11_BUFFER_DESC constants{};
  constants.ByteWidth = sizeof(FrameConstants);
  constants.Usage = D3D11_USAGE_DYNAMIC;
  constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(device_->device()->CreateBuffer(&constants, nullptr, &constant_buffer_))) return false;

  D3D11_SAMPLER_DESC sampler{};
  sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sampler.MaxLOD = D3D11_FLOAT32_MAX;
  if (FAILED(device_->device()->CreateSamplerState(&sampler, &sampler_state_))) return false;

  D3D11_BLEND_DESC blend{};
  blend.RenderTarget[0].BlendEnable = TRUE;
  blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  if (FAILED(device_->device()->CreateBlendState(&blend, &blend_state_))) return false;

  D3D11_RASTERIZER_DESC rasterizer{};
  rasterizer.FillMode = D3D11_FILL_SOLID;
  rasterizer.CullMode = D3D11_CULL_NONE;
  rasterizer.DepthClipEnable = TRUE;
  if (FAILED(device_->device()->CreateRasterizerState(&rasterizer, &rasterizer_state_))) {
    return false;
  }
  D3D11_BUFFER_DESC vertices{};
  vertices.ByteWidth = kMaximumMeshVertices * sizeof(animation::MeshVertex);
  vertices.Usage = D3D11_USAGE_DYNAMIC;
  vertices.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertices.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(device_->device()->CreateBuffer(&vertices, nullptr, &vertex_buffer_))) return false;
  D3D11_BUFFER_DESC indices{};
  indices.ByteWidth = kMaximumMeshIndices * sizeof(std::uint16_t);
  indices.Usage = D3D11_USAGE_DYNAMIC;
  indices.BindFlags = D3D11_BIND_INDEX_BUFFER;
  indices.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  return SUCCEEDED(device_->device()->CreateBuffer(&indices, nullptr, &index_buffer_));
}

void OverlayRenderer::Shutdown() {
  rasterizer_state_.Reset();
  blend_state_.Reset();
  sampler_state_.Reset();
  constant_buffer_.Reset();
  index_buffer_.Reset();
  vertex_buffer_.Reset();
  input_layout_.Reset();
  pixel_shader_.Reset();
  vertex_shader_.Reset();
  index_count_ = 0;
  device_lost_ = false;
  device_ = nullptr;
}

bool OverlayRenderer::CompileShaders() {
  Microsoft::WRL::ComPtr<ID3DBlob> vertex_blob;
  if (FAILED(CompileShader(kVertexShaderSource, "Main", "vs_5_0", &vertex_blob))) return false;
  if (FAILED(device_->device()->CreateVertexShader(vertex_blob->GetBufferPointer(),
                                                   vertex_blob->GetBufferSize(), nullptr,
                                                   &vertex_shader_))) {
    return false;
  }
  constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 2> kElements = {
      D3D11_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
                               D3D11_INPUT_PER_VERTEX_DATA, 0},
      D3D11_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
                               D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  if (FAILED(device_->device()->CreateInputLayout(
          kElements.data(), static_cast<UINT>(kElements.size()), vertex_blob->GetBufferPointer(),
          vertex_blob->GetBufferSize(), &input_layout_))) {
    return false;
  }
  Microsoft::WRL::ComPtr<ID3DBlob> pixel_blob;
  if (FAILED(CompileShader(kPixelShaderSource, "Main", "ps_5_0", &pixel_blob))) return false;
  return SUCCEEDED(device_->device()->CreatePixelShader(
      pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), nullptr, &pixel_shader_));
}

bool OverlayRenderer::UploadMesh(const animation::MinimizeMesh& mesh, bool upload_indices) {
  if (mesh.vertices.empty() || mesh.indices.empty() ||
      mesh.vertices.size() > kMaximumMeshVertices || mesh.indices.size() > kMaximumMeshIndices) {
    return false;
  }
  ID3D11DeviceContext* context = device_->context();
  D3D11_MAPPED_SUBRESOURCE mapped{};
  HRESULT result = context->Map(vertex_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  if (FAILED(result)) {
    MarkDeviceLost(result);
    return false;
  }
  std::memcpy(mapped.pData, mesh.vertices.data(),
              mesh.vertices.size() * sizeof(animation::MeshVertex));
  context->Unmap(vertex_buffer_.Get(), 0);
  if (upload_indices) {
    result = context->Map(index_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(result)) {
      MarkDeviceLost(result);
      return false;
    }
    std::memcpy(mapped.pData, mesh.indices.data(), mesh.indices.size() * sizeof(std::uint16_t));
    context->Unmap(index_buffer_.Get(), 0);
  }
  index_count_ = static_cast<UINT>(mesh.indices.size());
  return true;
}

bool OverlayRenderer::UpdateConstants(UINT width, UINT height, float opacity) {
  D3D11_MAPPED_SUBRESOURCE mapped{};
  const HRESULT result =
      device_->context()->Map(constant_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  if (FAILED(result)) {
    MarkDeviceLost(result);
    return false;
  }
  auto* constants = static_cast<FrameConstants*>(mapped.pData);
  constants->viewport_size[0] = static_cast<float>(width);
  constants->viewport_size[1] = static_cast<float>(height);
  constants->opacity = opacity;
  constants->padding = 0.0f;
  device_->context()->Unmap(constant_buffer_.Get(), 0);
  return true;
}

bool OverlayRenderer::Render(const animation::MinimizeMesh& mesh, bool indices_changed,
                             ID3D11ShaderResourceView* texture,
                             ID3D11RenderTargetView* render_target, UINT width, UINT height,
                             float opacity) {
  if (device_lost_ || texture == nullptr || render_target == nullptr ||
      !UpdateConstants(width, height, opacity) || !UploadMesh(mesh, indices_changed)) {
    return false;
  }
  ID3D11DeviceContext* context = device_->context();
  D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height),
                          0.0f, 1.0f};
  constexpr std::array<float, 4> kClear = {0.0f, 0.0f, 0.0f, 0.0f};
  context->OMSetRenderTargets(1, &render_target, nullptr);
  context->ClearRenderTargetView(render_target, kClear.data());
  context->RSSetViewports(1, &viewport);
  context->RSSetState(rasterizer_state_.Get());
  constexpr UINT stride = sizeof(animation::MeshVertex);
  constexpr UINT offset = 0;
  ID3D11Buffer* vertices = vertex_buffer_.Get();
  context->IASetInputLayout(input_layout_.Get());
  context->IASetVertexBuffers(0, 1, &vertices, &stride, &offset);
  context->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
  context->VSSetConstantBuffers(0, 1, constant_buffer_.GetAddressOf());
  context->PSSetShader(pixel_shader_.Get(), nullptr, 0);
  context->PSSetConstantBuffers(0, 1, constant_buffer_.GetAddressOf());
  context->PSSetShaderResources(0, 1, &texture);
  context->PSSetSamplers(0, 1, sampler_state_.GetAddressOf());
  constexpr std::array<float, 4> kBlend = {0.0f, 0.0f, 0.0f, 0.0f};
  context->OMSetBlendState(blend_state_.Get(), kBlend.data(), 0xffffffff);
  context->DrawIndexed(index_count_, 0, 0);
  ID3D11ShaderResourceView* null_resource = nullptr;
  context->PSSetShaderResources(0, 1, &null_resource);
  return true;
}

void OverlayRenderer::MarkDeviceLost(HRESULT result) {
  if (device_ != nullptr && device_->IsDeviceLost(result)) device_lost_ = true;
}

}  // namespace minimize::rendering
