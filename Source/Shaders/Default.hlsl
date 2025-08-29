#include "Shaders/Common.hlsl"

SamplerState sampler_point_wrap        : register(s0);
SamplerState sampler_point_clamp       : register(s1);
SamplerState sampler_linear_wrap       : register(s2);
SamplerState sampler_linear_clamp      : register(s3);
SamplerState sampler_anisotropic_wrap  : register(s4);
SamplerState sampler_anisotropic_clamp : register(s5);

Texture2D texture_diffuse   : register(texture_diffuse_slot, per_material_space);
Texture2D texture_normal    : register(texture_normal_slot, per_material_space);
Texture2D texture_metallic_roughness : register(texture_metallic_roughness_slot, per_material_space);
Texture2D texture_specular  : register(texture_specular_slot, per_material_space);
Texture2D texture_ao        : register(texture_ao_slot, per_material_space);
Texture2D texture_emissive  : register(texture_emissive_slot, per_material_space);
Texture2D texture_overlay   : register(texture_overlay_slot, per_material_space);

ConstantBuffer<PerObjectConstants> per_object_cb : register(b0, per_object_space);
ConstantBuffer<PerMaterialConstants> per_material_cb : register(b0, per_material_space);
ConstantBuffer<PerPassConstants> per_pass_cb : register(b0, per_pass_space);
ConstantBuffer<PerFrameConstants> per_frame_cb : register(b0, per_frame_space);


VertexShaderOutput VS(MeshVertex IN)
{
    VertexShaderOutput OUT;

    float4 position_world = mul(per_object_cb.world_matrix, float4(IN.position, 1.0f));
    OUT.position_w = position_world.xyz;

    OUT.normal_w = normalize(mul((float3x3)per_object_cb.world_matrix, IN.normal));
    OUT.tangent_w = float4(normalize(mul((float3x3)per_object_cb.world_matrix, IN.tangent.xyz)), IN.tangent.w);

    OUT.position_h = mul(per_pass_cb.view_matrix, position_world);
    OUT.position_h = mul(per_pass_cb.projection_matrix, OUT.position_h);

    OUT.uv = IN.uv;

    return OUT;
}

// TODO: Use sampler descriptor heap!!!
float4 sample_texture(Texture2D tex, uint sampler_mode, float2 uv)
{
    if (sampler_mode == 0)
    {
        return tex.Sample(sampler_anisotropic_clamp, uv);
    }
    else
    {
        return tex.Sample(sampler_anisotropic_wrap, uv);
    }
}

float4 PS(VertexShaderOutput IN) : SV_Target
{
    float4 final_albedo_color = float4(per_material_cb.albedo_color, 1.0f);
    if (per_material_cb.feature_flags & MaterialFeature_AlbedoMap)
    {
        final_albedo_color = final_albedo_color * sample_texture(texture_diffuse, per_material_cb.sampler_mode, IN.uv);
        
        clip(final_albedo_color.a < 0.1f ? -1:1);
    }

    if (per_material_cb.feature_flags & MaterialFeature_OverlayMap)
    {
        float4 overlay_color = sample_texture(texture_overlay, per_material_cb.sampler_mode, IN.uv);
        final_albedo_color = lerp(final_albedo_color, overlay_color, overlay_color.a);
    }

    float3 normal_world = IN.normal_w;
    if (per_material_cb.feature_flags & MaterialFeature_NormalMap)
    {
        float4 normal_sample = sample_texture(texture_normal, per_material_cb.sampler_mode, IN.uv);
        if (per_material_cb.feature_flags & MaterialFeature_FlipNormals)
        {
            normal_sample.g = 1.0f - normal_sample.g;
        }
        normal_world = normal_sample_to_world(normal_sample.rgb, IN.normal_w, IN.tangent_w);
    }

    float metalness = per_material_cb.metallic;
    float roughness = per_material_cb.roughness;
    bool has_metallic_map = per_material_cb.feature_flags & MaterialFeature_MetalnessMap;
    bool has_roughness_map = per_material_cb.feature_flags & MaterialFeature_RoughnessMap;

    if (has_metallic_map || has_roughness_map)
    {
        float4 metallic_roughness_sample = sample_texture(texture_metallic_roughness, per_material_cb.sampler_mode, IN.uv);
        if (has_roughness_map)
        {
            roughness = metallic_roughness_sample.g;
            // roughness = lerp(0.015f, 1.0f, roughness);
        }
        if (has_metallic_map)
        {
            metalness = metallic_roughness_sample.b;
        }
    }

    float specular = per_material_cb.specular;
    if (per_material_cb.feature_flags & MaterialFeature_SpecularMap)
    {
        specular = sample_texture(texture_specular, per_material_cb.sampler_mode, IN.uv).r;
    }

    float ao = 1.0f;
    if (per_material_cb.feature_flags & MaterialFeature_AOMap)
    {
        ao = sample_texture(texture_ao, per_material_cb.sampler_mode, IN.uv).r;
    }

    // TODO
    float4 emissive_color = float4(per_material_cb.emissive, per_material_cb.emissive, per_material_cb.emissive, 1.0f);
    if (per_material_cb.feature_flags & MaterialFeature_EmissiveMap)
    {
        emissive_color = sample_texture(texture_emissive, per_material_cb.sampler_mode, IN.uv);
    }

    // Indirect lighting
    float3 indirect_light = compute_indirect(per_frame_cb.hemispheric_light_color.rgb, final_albedo_color.rgb, metalness, ao);

    // Direct lighting
    float3 view_direction = normalize(per_pass_cb.camera_position.xyz - IN.position_w);
    float3 direct_light = compute_lights(
        per_frame_cb.global_light, 
        per_frame_cb.punctual_lights, 
        per_frame_cb.num_punctual_lights, 
        per_material_cb, 
        final_albedo_color.rgb,
        IN.position_w, 
        normal_world, 
        view_direction,
        roughness,
        specular,
        metalness);

    float4 out_color = float4(indirect_light + direct_light, final_albedo_color.a) + emissive_color;

    // out_color = float4(metalness, metalness, metalness, 1.0f);
    // out_color = float4(roughness, roughness, roughness, 1.0f);
    // out_color = float4(ao, ao, ao, 1.0f);

    // out_color = float4(direct_light.rgb, 1.0f);

    return out_color;
}
