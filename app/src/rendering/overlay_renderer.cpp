#include "pch.hpp"

#include "rendering/overlay_renderer.hpp"

#include <array>
#include <cstring>
#include <d3dcompiler.h>
#include <vector>

#include "rendering/d3d_device.hpp"

namespace minimize::rendering {
namespace {

constexpr UINT kGridSegments = 64;
constexpr UINT kGridVertexCount = (kGridSegments + 1) * (kGridSegments + 1);
constexpr UINT kGridIndexCount = kGridSegments * kGridSegments * 6;

struct PixelConstants {
  float opacity = 1.0f;
  float padding[3]{};
};

constexpr char kVertexShaderSource[] = R"(
cbuffer GenieConstants : register(b0) {
  float4 source;
  float4 target;
  float progress;
  float strength;
  uint edge;
  uint style;
};

struct VertexInput {
  float2 texcoord : TEXCOORD0;
};
struct PixelInput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};

static const uint EDGE_TOP = 0;
static const uint EDGE_BOTTOM = 1;
static const uint EDGE_LEFT = 2;
static const uint EDGE_RIGHT = 3;
static const uint STYLE_CLASSIC = 0;
static const uint STYLE_CURVY = 1;
static const float PI = 3.14159265358979323846f;

float safe_divisor(float value) {
  if (abs(value) < 0.000001f) return value < 0.0f ? -0.000001f : 0.000001f;
  return value;
}

float quadratic_ease_in_out(float value) {
  float p = saturate(value);
  if (p < 0.5f) return 2.0f * p * p;
  return (-2.0f * p * p) + (4.0f * p) - 1.0f;
}

float sine_ease_in_out(float value) {
  return 0.5f - 0.5f * cos(saturate(value) * PI);
}

float curve_position(float position, float curve_start, float curve_end,
                     float value_start, float value_end) {
  if (position < curve_start) return value_start;
  if (position < curve_end) {
    float p = quadratic_ease_in_out(
        (position - curve_start) / safe_divisor(curve_end - curve_start));
    return lerp(value_start, value_end, p);
  }
  return value_end;
}

float2 linear_position(float2 uv) {
  float2 source_point = float2(lerp(source.x, source.z, uv.x),
                               lerp(source.y, source.w, uv.y));
  float2 target_point = float2(lerp(target.x, target.z, uv.x),
                               lerp(target.y, target.w, uv.y));
  return lerp(source_point, target_point, saturate(progress));
}

float2 classic_position(float2 uv) {
  float slide_progress = saturate(progress / 0.5f);
  float translate_progress = saturate((progress - 0.4f) / 0.6f);

  if (edge == EDGE_BOTTOM) {
    float vertical_distance = target.y - source.y;
    float translation = translate_progress * vertical_distance;
    float top_edge = min(source.w + translation, target.w);
    float bottom_edge = source.y + translation;
    float y = lerp(bottom_edge, top_edge, uv.y);
    float left_end = source.x + slide_progress * (target.x - source.x);
    float right_end = source.z + slide_progress * (target.z - source.z);
    return float2(
        lerp(curve_position(y, source.y, target.y, source.x, left_end),
             curve_position(y, source.y, target.y, source.z, right_end), uv.x),
        y);
  }

  if (edge == EDGE_TOP) {
    float vertical_distance = target.w - source.w;
    float translation = translate_progress * vertical_distance;
    float top_edge = source.w + translation;
    float bottom_edge = max(source.y + translation, target.y);
    float y = lerp(bottom_edge, top_edge, uv.y);
    float left_start = source.x + slide_progress * (target.x - source.x);
    float right_start = source.z + slide_progress * (target.z - source.z);
    return float2(
        lerp(curve_position(y, target.w, source.w, left_start, source.x),
             curve_position(y, target.w, source.w, right_start, source.z), uv.x),
        y);
  }

  if (edge == EDGE_LEFT) {
    float horizontal_distance = target.x - source.x;
    float translation = translate_progress * horizontal_distance;
    float left_edge = source.x + translation;
    float right_edge = min(source.z + translation, target.z);
    float x = lerp(left_edge, right_edge, uv.x);
    float top_end = source.w + slide_progress * (target.w - source.w);
    float bottom_end = source.y + slide_progress * (target.y - source.y);
    float y_bottom = curve_position(x, source.x, target.x, source.y, bottom_end);
    float y_top = curve_position(x, source.x, target.x, source.w, top_end);
    return float2(x, lerp(y_bottom, y_top, uv.y));
  }

  float horizontal_distance = target.z - source.z;
  float translation = translate_progress * horizontal_distance;
  float left_edge = max(source.x + translation, target.x);
  float right_edge = source.z + translation;
  float x = lerp(left_edge, right_edge, uv.x);
  float top_start = source.w + slide_progress * (target.w - source.w);
  float bottom_start = source.y + slide_progress * (target.y - source.y);
  float y_bottom = curve_position(x, target.z, source.z, bottom_start, source.y);
  float y_top = curve_position(x, target.z, source.z, top_start, source.w);
  return float2(x, lerp(y_bottom, y_top, uv.y));
}

float2 curvy_position(float2 uv) {
  bool horizontal = edge == EDGE_TOP || edge == EDGE_BOTTOM;
  float moving_extent = horizontal ? source.w - source.y : source.z - source.x;
  float distance_to_target;
  if (edge == EDGE_TOP) distance_to_target = source.w - target.w;
  else if (edge == EDGE_BOTTOM) distance_to_target = target.y - source.y;
  else if (edge == EDGE_LEFT) distance_to_target = source.z - target.z;
  else distance_to_target = target.x - source.x;
  distance_to_target = max(distance_to_target, 0.000001f);

  float shape_factor = clamp(max(0.20f, moving_extent / distance_to_target), 0.20f, 1.0f);
  float stretch_weight = 0.70f * shape_factor;
  float stretch_end = stretch_weight / (1.0f + stretch_weight);
  float stretch_progress = 0.0f;
  float squash_progress = 0.0f;
  if (progress < stretch_end) {
    stretch_progress = shape_factor * saturate(progress / safe_divisor(stretch_end));
  } else {
    squash_progress = saturate((progress - stretch_end) / safe_divisor(1.0f - stretch_end));
    stretch_progress = min(shape_factor + squash_progress, 1.0f);
  }

  float2 source_point = float2(lerp(source.x, source.z, uv.x),
                               lerp(source.y, source.w, uv.y));
  float2 target_point = float2(lerp(target.x, target.z, uv.x),
                               lerp(target.y, target.w, uv.y));
  if (horizontal) {
    float local = source_point.y - source.y;
    float offset = edge == EDGE_BOTTOM
                       ? local + distance_to_target * squash_progress
                       : local - distance_to_target * squash_progress;
    float shape_position = edge == EDGE_BOTTOM
                               ? offset / distance_to_target
                               : ((source.w - source.y) - offset) / distance_to_target;
    float scale = stretch_progress * sine_ease_in_out(shape_position);
    return float2(lerp(source_point.x, target_point.x, scale), source.y + offset);
  }

  float local = source_point.x - source.x;
  float offset = edge == EDGE_RIGHT
                     ? local + distance_to_target * squash_progress
                     : local - distance_to_target * squash_progress;
  float shape_position = edge == EDGE_RIGHT
                             ? offset / distance_to_target
                             : ((source.z - source.x) - offset) / distance_to_target;
  float scale = stretch_progress * sine_ease_in_out(shape_position);
  return float2(source.x + offset, lerp(source_point.y, target_point.y, scale));
}

PixelInput Main(VertexInput input) {
  PixelInput output;
  float2 base_position = linear_position(input.texcoord);
  float2 deformed = base_position;
  if (style == STYLE_CLASSIC) {
    deformed = lerp(base_position, classic_position(input.texcoord), saturate(strength));
  } else if (style == STYLE_CURVY) {
    deformed = curvy_position(input.texcoord);
  }
  output.position = float4(deformed.x * 2.0f - 1.0f, 1.0f - deformed.y * 2.0f, 0.0f, 1.0f);
  output.texcoord = input.texcoord;
  return output;
}
)";

constexpr char kPixelShaderSource[] = R"(
cbuffer PixelConstants : register(b1) {
  float opacity;
  float3 padding;
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
  if (device_ == nullptr || !CompileShaders() || !CreateStaticGrid()) return false;

  D3D11_BUFFER_DESC constants{};
  constants.ByteWidth = sizeof(animation::GenieConstants);
  constants.Usage = D3D11_USAGE_DYNAMIC;
  constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(device_->device()->CreateBuffer(&constants, nullptr, &genie_constant_buffer_))) {
    return false;
  }
  constants.ByteWidth = sizeof(PixelConstants);
  if (FAILED(device_->device()->CreateBuffer(&constants, nullptr, &pixel_constant_buffer_))) {
    return false;
  }

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
  return SUCCEEDED(device_->device()->CreateRasterizerState(&rasterizer, &rasterizer_state_));
}

void OverlayRenderer::Shutdown() {
  rasterizer_state_.Reset();
  blend_state_.Reset();
  sampler_state_.Reset();
  pixel_constant_buffer_.Reset();
  genie_constant_buffer_.Reset();
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
  constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 1> kElements = {
      D3D11_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
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

bool OverlayRenderer::CreateStaticGrid() {
  std::vector<animation::GridVertex> vertices;
  vertices.reserve(kGridVertexCount);
  for (UINT row = 0; row <= kGridSegments; ++row) {
    for (UINT column = 0; column <= kGridSegments; ++column) {
      vertices.push_back(animation::GridVertex{
          .u = static_cast<float>(column) / static_cast<float>(kGridSegments),
          .v = static_cast<float>(row) / static_cast<float>(kGridSegments),
      });
    }
  }

  std::vector<std::uint16_t> indices;
  indices.reserve(kGridIndexCount);
  for (UINT row = 0; row < kGridSegments; ++row) {
    for (UINT column = 0; column < kGridSegments; ++column) {
      const auto lower_left =
          static_cast<std::uint16_t>(row * (kGridSegments + 1) + column);
      const auto lower_right = static_cast<std::uint16_t>(lower_left + 1);
      const auto upper_left =
          static_cast<std::uint16_t>((row + 1) * (kGridSegments + 1) + column);
      const auto upper_right = static_cast<std::uint16_t>(upper_left + 1);
      indices.insert(indices.end(),
                     {lower_left, upper_left, lower_right, lower_right, upper_left, upper_right});
    }
  }

  D3D11_BUFFER_DESC vertex_desc{};
  vertex_desc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(animation::GridVertex));
  vertex_desc.Usage = D3D11_USAGE_IMMUTABLE;
  vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vertex_data{.pSysMem = vertices.data()};
  if (FAILED(device_->device()->CreateBuffer(&vertex_desc, &vertex_data, &vertex_buffer_))) {
    return false;
  }

  D3D11_BUFFER_DESC index_desc{};
  index_desc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));
  index_desc.Usage = D3D11_USAGE_IMMUTABLE;
  index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA index_data{.pSysMem = indices.data()};
  if (FAILED(device_->device()->CreateBuffer(&index_desc, &index_data, &index_buffer_))) {
    return false;
  }
  index_count_ = static_cast<UINT>(indices.size());
  return true;
}

bool OverlayRenderer::UpdateConstants(const animation::GenieConstants& genie_constants,
                                      float opacity) {
  ID3D11DeviceContext* context = device_->context();
  D3D11_MAPPED_SUBRESOURCE mapped{};
  HRESULT result =
      context->Map(genie_constant_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  if (FAILED(result)) {
    MarkDeviceLost(result);
    return false;
  }
  std::memcpy(mapped.pData, &genie_constants, sizeof(genie_constants));
  context->Unmap(genie_constant_buffer_.Get(), 0);

  result = context->Map(pixel_constant_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  if (FAILED(result)) {
    MarkDeviceLost(result);
    return false;
  }
  const PixelConstants pixel_constants{.opacity = opacity};
  std::memcpy(mapped.pData, &pixel_constants, sizeof(pixel_constants));
  context->Unmap(pixel_constant_buffer_.Get(), 0);
  return true;
}

bool OverlayRenderer::Render(const animation::GenieConstants& genie_constants,
                             ID3D11ShaderResourceView* texture,
                             ID3D11RenderTargetView* render_target, UINT width, UINT height,
                             float opacity) {
  if (device_lost_ || texture == nullptr || render_target == nullptr ||
      !UpdateConstants(genie_constants, opacity)) {
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
  constexpr UINT stride = sizeof(animation::GridVertex);
  constexpr UINT offset = 0;
  ID3D11Buffer* vertices = vertex_buffer_.Get();
  context->IASetInputLayout(input_layout_.Get());
  context->IASetVertexBuffers(0, 1, &vertices, &stride, &offset);
  context->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
  ID3D11Buffer* genie_constants_buffer = genie_constant_buffer_.Get();
  context->VSSetConstantBuffers(0, 1, &genie_constants_buffer);
  context->PSSetShader(pixel_shader_.Get(), nullptr, 0);
  ID3D11Buffer* pixel_constants_buffer = pixel_constant_buffer_.Get();
  context->PSSetConstantBuffers(1, 1, &pixel_constants_buffer);
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
