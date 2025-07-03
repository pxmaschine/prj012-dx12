#include "Shaders/Common.hlsl"

SamplerState sampler_point_wrap        : register(s0);
SamplerState sampler_point_clamp       : register(s1);
SamplerState sampler_linear_wrap       : register(s2);
SamplerState sampler_linear_clamp      : register(s3);
SamplerState sampler_anisotropic_wrap  : register(s4);
SamplerState sampler_anisotropic_clamp : register(s5);

Texture2D texture_diffuse : register(t0, per_pass_space);

struct PixelShaderInput
{
    float2 uv       : TEXCOORD0;
};

float4 main( PixelShaderInput IN ) : SV_Target
{
    float4 diffuse_color = texture_diffuse.Sample(sampler_anisotropic_clamp, IN.uv);
    return diffuse_color;
}
