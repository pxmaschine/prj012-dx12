#include "Shaders/Common.hlsl"

SamplerState sampler_point_wrap        : register(s0);
SamplerState sampler_point_clamp       : register(s1);
SamplerState sampler_linear_wrap       : register(s2);
SamplerState sampler_linear_clamp      : register(s3);
SamplerState sampler_anisotropic_wrap  : register(s4);
SamplerState sampler_anisotropic_clamp : register(s5);

Texture2D texture_diffuse : register(t0, per_pass_space);

ConstantBuffer<PerObjectConstants> per_object_cb : register(b0, per_object_space);
ConstantBuffer<PerPassConstants> per_pass_cb : register(b0, per_pass_space);


VertexShaderOutput VS(MeshVertex IN)
{
    VertexShaderOutput OUT;

    // matrix mvp = mul(PerObjectCB.world_matrix, PerPassCB.view_matrix);
    // mvp = mul(mvp, PerPassCB.projection_matrix);
    // OUT.position = mul(PerObjectCB.world_matrix, float4(IN.position, 1.0f));
    // OUT.position = mul(PerPassCB.view_matrix, OUT.position);
    // OUT.position = mul(PerPassCB.projection_matrix, OUT.position);
    // OUT.normal = IN.normal;
    // OUT.uv = IN.uv;

    float4 position_world = mul(per_object_cb.world_matrix, float4(IN.position, 1.0f));
    OUT.position_w = position_world.xyz;

    OUT.position_h = mul(per_pass_cb.view_matrix, position_world);
    OUT.position_h = mul(per_pass_cb.projection_matrix, OUT.position_h);
    OUT.normal_w = IN.normal;
    OUT.tangent_w = IN.tangent;
    OUT.uv = IN.uv;

    return OUT;
}

float4 PS(VertexShaderOutput IN) : SV_Target
{
    float4 diffuse_color = texture_diffuse.Sample(sampler_anisotropic_clamp, IN.uv);
    return diffuse_color;
}
