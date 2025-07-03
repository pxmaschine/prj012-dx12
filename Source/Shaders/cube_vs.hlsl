#include "Shaders/Common.hlsl"

ConstantBuffer<PerObjectConstants> PerObjectCB : register(b0, per_object_space);
ConstantBuffer<PerPassConstants> PerPassCB : register(b0, per_pass_space);

struct VertexShaderOutput
{
    float2 uv       : TEXCOORD0;
    float4 position : SV_Position;
};

VertexShaderOutput main(MeshVertex IN)
{
    VertexShaderOutput OUT;

    // matrix mvp = mul(PerObjectCB.world_matrix, PerPassCB.view_matrix);
    // mvp = mul(mvp, PerPassCB.projection_matrix);
    OUT.position = mul(PerObjectCB.world_matrix, float4(IN.position, 1.0f));
    OUT.position = mul(PerPassCB.view_matrix, OUT.position);
    OUT.position = mul(PerPassCB.projection_matrix, OUT.position);
    OUT.uv = IN.uv;

    return OUT;
}
