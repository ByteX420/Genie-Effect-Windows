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

Texture2D source_texture : register(t0);
SamplerState linear_sampler : register(s0);

PixelInput VertexMain(VertexInput input) {
  PixelInput output;
  float2 normalized;
  normalized.x = (input.position.x / viewport_size.x) * 2.0f - 1.0f;
  normalized.y = 1.0f - (input.position.y / viewport_size.y) * 2.0f;
  output.position = float4(normalized, 0.0f, 1.0f);
  output.texcoord = input.texcoord;
  return output;
}

float4 PixelMain(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0)
    : SV_TARGET {
  float4 color = source_texture.Sample(linear_sampler, texcoord);
  color.a = opacity;
  color.rgb *= opacity;
  return color;
}
