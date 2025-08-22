#ifndef __COMMON_HLSL__
#define __COMMON_HLSL__

#include "Shared.h"

#define PI 3.14159265359f
#define ONE_OVER_PI (1.0f / PI)

// Resources are bound to tables grouped by update frequency, to optimize performance
// Tables are also in order from most frequently to least frequently updated, to optimize performance
#define per_object_space    space0
#define per_material_space  space1
#define per_pass_space      space2
#define per_frame_space     space3

#define texture_diffuse_slot             t0
#define texture_normal_slot              t1
#define texture_metallic_roughness_slot  t2
#define texture_ao_slot                  t3
#define texture_emissive_slot            t4
#define texture_specular_slot            t5
#define texture_overlay_slot             t6

#define PunctualLightType_Point 0u
#define PunctualLightType_Spot  1u

#define MaterialFeature_AlbedoMap    1u
#define MaterialFeature_NormalMap    2u
#define MaterialFeature_RoughnessMap 4u
#define MaterialFeature_SpecularMap  8u
#define MaterialFeature_MetalnessMap 16u
#define MaterialFeature_AOMap        32u
#define MaterialFeature_FlipNormals  64u
#define MaterialFeature_OverlayMap   128u
#define MaterialFeature_EmissiveMap  256u

struct VertexShaderOutput
{
    float4 position_h : SV_POSITION;
    float3 position_w : POSITION;
    float2 uv         : TEXCOORD0;
    float3 normal_w   : NORMAL0;
    float3 tangent_w  : TANGENT0;
};

float3 normal_sample_to_world(float3 normal_sample, float3 normal_w, float3 tangent_w)
{
    float3 normal_t = 2.0f * normal_sample - 1.0f;

    float3 n = normal_w;
    float3 t = normalize(tangent_w - dot(tangent_w, n) * n);
    float3 b = cross(n, t);

    // b = -b;

    float3x3 tbn = float3x3(t, b, n);

    return normalize(mul(normal_t, tbn));
}

// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float3 f_schlick(float3 f0, float3 f90, float l_dot_h)
{
    return f0 + (f90 - f0) * pow(1.0f - l_dot_h, 5.0f);
}

// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float3 f_schlick(float3 f0, float l_dot_h)
{
    return f0 + (1 - f0) * pow(1.0f - l_dot_h, 5.0f);
}


// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float d_ggx(float n_dot_h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float f = (n_dot_h * a2 - n_dot_h) * n_dot_h + 1;
    return a2 / (f * f);
}

// [Burley 2012, "Physically Based Shading at Disney"]
float fr_disney_burley_diffuse(float n_dot_v, float n_dot_l, float l_dot_h, float roughness)
{
    // [Lagarde and de Rousiers 2014, "Moving Frostbite to Physically Based Rendering 3.0"]
    float energy_bias = lerp(0.0f, 0.5f, roughness);
    float energy_factor = lerp(1.0f, 1.0f / 1.51, roughness);
    float f90 = energy_bias + 2.0 * l_dot_h * l_dot_h * roughness;
    float3 f0 = float3(1.0, 1.0, 1.0);
    float light_scatter = f_schlick(f0, f90, n_dot_l).r;
    float view_scatter = f_schlick(f0, f90, n_dot_v).r;

    return light_scatter * view_scatter * energy_factor;
}

// Taken from: https://alextardif.com/PhysicallyBasedRendering.html
float3 compute_indirect(float3 hemispheric_light_color, float3 albedo_color, float metallic, float ao)
{
    return hemispheric_light_color * lerp(albedo_color, float3(0.2f, 0.2f, 0.2f), metallic) * ao;
}

float luminance(float3 rgb)
{
	return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

// Attenuates F90 for very low F0 values
// Source: "An efficient and Physically Plausible Real-Time Shading Model" in ShaderX7 by Schuler
// Also see section "Overbright highlights" in Hoffman's 2010 "Crafting Physically Motivated Shading Models for Game Development" for discussion
// IMPORTANT: Note that when F0 is calculated using metalness, it's value is never less than MIN_DIELECTRICS_F0, and therefore,
// this adjustment has no effect. To be effective, F0 must be authored separately, or calculated in different way. See main text for discussion.
float shadowedF90(float3 F0, float min_dielectrics_f0) {
    // Specifies minimal reflectance for dielectrics (when metalness is zero)
    // Nothing has lower reflectance than 2%, but we use 4% to have consistent results with UE4, Frostbite, et al.
    // Note: only takes effect when USE_REFLECTANCE_PARAMETER is not defined
    // #define MIN_DIELECTRICS_F0 0.04f
	// This scaler value is somewhat arbitrary, Schuler used 60 in his article. In here, we derive it from MIN_DIELECTRICS_F0 so
	// that it takes effect for any reflectance lower than least reflective dielectrics
	//const float t = 60.0f;
	// const float t = (1.0f / 0.04f /*MIN_DIELECTRICS_F0*/);
	const float t = (1.0f / min_dielectrics_f0);
	return min(1.0f, t * luminance(F0));
}

// Smith G2 term (masking-shadowing function) for GGX distribution
// Height correlated version - optimized by substituing G_Lambda for G_Lambda_GGX and dividing by (4 * NdotL * NdotV) to cancel out 
// the terms in specular BRDF denominator
// Source: "Moving Frostbite to Physically Based Rendering" by Lagarde & de Rousiers
// Note that returned value is G2 / (4 * NdotL * NdotV) and therefore includes division by specular BRDF denominator
float g_smith_ggx_height_correlated(float n_dot_v, float n_dot_l, float roughness)
{
    // [Burley 2012, "Physically Based Shading at Disney"]
    float a = pow((0.5 + roughness/2.0f), 2.0f);
    float a2 = a * a;

	float lambda_ggxv = n_dot_l * sqrt(a2 + n_dot_v * (n_dot_v - a2 * n_dot_v));
	float lambda_ggxl = n_dot_v * sqrt(a2 + n_dot_l * (n_dot_l - a2 * n_dot_l));
	return 0.5f / (lambda_ggxv + lambda_ggxl);
}

// [Lagarde and de Rousiers 2014, "Moving Frostbite to Physically Based Rendering 3.0"]
float3 compute_direct(float3 view_direction, float3 light_direction, float3 base_color, float3 surface_normal, float roughness, float metallic, float specular)
{
    // view_direction: from surface to camera
    // light_direction: from surface to light
    // surface normal: world or tangent space

    float3 result;

    // angle between surface normal and light direction (how much the surface faces the light)
	float n_dot_l = dot(surface_normal, light_direction);
    // angle between normal and view direction
	float n_dot_v = dot(surface_normal, view_direction);

    // If the surface is facing away from the light or the view direction, return 0
	// if (n_dot_v <= 0.0f || n_dot_l <= 0.0f) 
    // {
    //     result = float3(0.0f, 0.0f, 0.0f);
    // }
    // else
    // {
    // Clamp NdotX to prevent numerical instability. Assume vectors below the hemisphere will be filtered using 'Vbackfacing' and 'Lbackfacing' flags
    n_dot_l = min(max(1e-5, n_dot_l), 1.0f);
    n_dot_v = min(max(1e-5, n_dot_v), 1.0f);

    // half vector between view and light direction (hypothetical microfacet that reflects L into V)
    float3 h = normalize(view_direction + light_direction);
    // angle between light and half vector (how much the light aligns with the reflecting direction)
    float l_dot_h = saturate(dot(light_direction, h));
    // angle between normal and half vector (how likely it is for a microfacet to be oriented with H)
    float n_dot_h = saturate(dot(surface_normal, h));

    // Evaluate specular BRDF
    const float min_dielectrics_f0 = 0.08f * specular;
    float3 specular_f0 = lerp(float3(min_dielectrics_f0, min_dielectrics_f0, min_dielectrics_f0), base_color, metallic);

    float3 f = f_schlick(specular_f0, shadowedF90(specular_f0, min_dielectrics_f0), l_dot_h);
    float d = d_ggx(n_dot_h, roughness);
    float g = g_smith_ggx_height_correlated(n_dot_v, n_dot_l, roughness);

    float3 f_r = f * g * d * ONE_OVER_PI;

    // Evaluate diffuse BRDF
    float3 diffuse_reflectance = base_color * (1.0f - metallic);

    float3 f_d = diffuse_reflectance * (fr_disney_burley_diffuse(n_dot_v, n_dot_l, l_dot_h, roughness) * ONE_OVER_PI);

    result = (f_d + f_r) * n_dot_l;
    // }

	return result;
}

float3 compute_global_light(GlobalLight light, PerMaterialConstants mat, float3 albedo_color, float3 normal, float3 view_direction, float roughness, float specular, float metalness)
{
    // The light vector aims opposite the direction the light rays travel.
    float3 light_direction = -light.direction.xyz;

    float3 direct_light = compute_direct(view_direction, light_direction, albedo_color, normal, roughness, metalness, specular);

    return light.intensity * light.light_color * direct_light;
}

float smooth_distance_attenuation(float squared_distance, float inv_sqr_att_radius)
{
  float factor = squared_distance * inv_sqr_att_radius;
  float smooth_factor = saturate(1.0f - factor * factor);
  return smooth_factor * smooth_factor;
}

float get_distance_attenuation(float3 unnormalized_light_vector, float inv_sqr_att_radius)
{
  float squared_distance = dot(unnormalized_light_vector , unnormalized_light_vector);
  float attenuation = 1.0 / (max(squared_distance, 0.01 * 0.01));
  attenuation *= smooth_distance_attenuation(squared_distance, inv_sqr_att_radius);
  return attenuation;
}

float get_angle_attenuation(float3 normalized_light_vector, float3 light_direction, float light_angle_scale, float light_angle_offset)
{
  float cd = dot(light_direction, normalized_light_vector);
  float attenuation = saturate(cd * light_angle_scale + light_angle_offset);
  attenuation *= attenuation;
  return attenuation;
}

float3 compute_punctual_light(PunctualLight light, PerMaterialConstants mat, float3 albedo_color, float3 position, float3 normal, float3 view_direction, float roughness, float specular, float metalness)
{
    float3 unnormalized_light_vector = light.position.xyz - position;
    float3 light_direction = normalize(unnormalized_light_vector);
    float attenuation = 1.0;

    attenuation *= get_distance_attenuation(unnormalized_light_vector, light.inv_sqr_att_radius);
    
    if (light.type == PunctualLightType_Spot)
    {
        // TODO: light.direction is not flipped in source
        attenuation *= get_angle_attenuation(light_direction, -light.direction.xyz, light.angle_scale, light.angle_offset);
    }

    float3 direct_light = compute_direct(view_direction, light_direction, albedo_color, normal, roughness, metalness, specular);

    return light.intensity * light.light_color.rgb * direct_light * attenuation;
}

float3 compute_lights(GlobalLight global_light, PunctualLight punctual_lights[k_max_lights], u32 num_punctual_lights, PerMaterialConstants mat,
                      float3 albedo_color,float3 position, float3 normal, float3 view_direction, float roughness, float specular, float metalness)//, float3 shadowFactor)
{
    float3 result = 0.0f;

    result += compute_global_light(global_light, mat, albedo_color, normal, view_direction, roughness, specular, metalness);

    for (uint i = 0; i < num_punctual_lights; ++i)
    {
        result += compute_punctual_light(punctual_lights[i], mat, albedo_color, position, normal, view_direction, roughness, specular, metalness);
    }

    return result;
}


#endif
