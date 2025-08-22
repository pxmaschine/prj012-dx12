#ifndef __SHARED_HLSL__
#define __SHARED_HLSL__

#ifndef __cplusplus
//This is a subset of HLSL types mapped to corresponding SimpleMath types
#define u32               uint
#define s32               int
#define f32               float
#define Vector2           float2
#define Vector3           float3
#define Vector4           float4
#define Matrix            float4x4
#define PunctualLightType uint
#define SamplerAddressMode uint

struct VertexPosColor
{
    Vector3 position : POSITION;
    Vector3 color    : COLOR;
};

struct MeshVertex
{
    Vector3 position : POSITION;
    Vector2 uv       : TEXCOORD;
    Vector3 normal   : NORMAL;
    Vector3 tangent  : TANGENT;
};

#else

#include <CoreDefs.h>
#include <MathLib.h>

struct VertexPosColor
{
  Vector3 position;
  Vector3 color;
};

struct MeshVertex
{
    Vector3 position;
    Vector2 uv;
    Vector3 normal;
    Vector3 tangent;
};

enum class PunctualLightType : u32
{
    Point = 0,
    Spot = 1,
};

enum class SamplerAddressMode : u32
{
    Clamp = 0,
    Wrap = 1,
};

#endif

#define k_max_lights 32

// A light that emits light globally in a single direction (e.g. sun)
struct GlobalLight
{
    Vector4 direction;
    Vector3 light_color;
    f32 intensity;
};

// TODO: Remove
#ifdef __cplusplus
#pragma pack(push)
#pragma pack(16) 
#endif
struct PunctualLight
{
    Vector3 position;
    f32 intensity;
    Vector3 light_color;
    f32 inv_sqr_att_radius;
    Vector3 direction; // Spot light only
    f32 angle_scale;   // Spot light only; 1.0 f / max (0.001f, ( inner_cone_angle - outer_cone_angle ));
    f32 angle_offset;  // Spot light only; -outer_cone_angle * angle_scale;
    PunctualLightType type;
    u32 pad0;
    u32 pad1;

#ifdef __cplusplus
    void set_inv_sqr_att_radius(f32 att_radius)
    {
        inv_sqr_att_radius = 1.0f / (att_radius * att_radius);
    }

    void set_angle_scale_and_offset(f32 inner_cone_angle, f32 outer_cone_angle)
    {
        angle_scale = 1.0f / ZV::max(0.001f, (ZV::cos(inner_cone_angle) - ZV::cos(outer_cone_angle)));
        angle_offset = -ZV::cos(outer_cone_angle) * angle_scale;
    }
#endif
};
#ifdef __cplusplus
#pragma pack(pop)
#endif

struct PerObjectConstants
{
    Matrix world_matrix;
};

struct PerMaterialConstants
{
    Vector3 albedo_color;
    f32 metallic;
    f32 roughness;
    f32 specular;
    f32 emissive;
    u32 feature_flags;
    SamplerAddressMode sampler_mode;

#ifdef __cplusplus
    PerMaterialConstants()
    {
        albedo_color = Vector3(1.0f, 1.0f, 1.0f);
        metallic = 0.0f;
        roughness = 0.5f;
        specular = 0.5f;
        emissive = 0.0f;
        feature_flags = 0;
        sampler_mode = SamplerAddressMode::Clamp;
    }
#endif
};

// TODO: Remove
#ifdef __cplusplus
#pragma pack(push)
#pragma pack(16) 
#endif
struct PerPassConstants
{
    Matrix view_matrix;
    Matrix projection_matrix;
    Vector4 camera_position;
};
#ifdef __cplusplus
#pragma pack(pop)
#endif

struct PerFrameConstants
{
    Vector4 hemispheric_light_color;

    GlobalLight global_light;
    PunctualLight punctual_lights[k_max_lights];
    u32 num_punctual_lights;

#ifdef __cplusplus
    PerFrameConstants()
    {
        hemispheric_light_color = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        global_light.direction = Vector4(0.0f, -1.0f, 0.0f, 1.0f);
        global_light.intensity = 0.0f;
        global_light.light_color = Vector3(1.0f, 1.0f, 1.0f);
        num_punctual_lights = 0;
    }
#endif
};

#endif