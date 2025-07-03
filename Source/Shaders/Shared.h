#ifndef __SHARED_HLSL__
#define __SHARED_HLSL__

#ifndef __cplusplus
//This is a subset of HLSL types mapped to corresponding SimpleMath types
#define u32      uint
#define s32      int
#define f32      float
#define Vector2  float2
#define Vector3  float3
#define Vector4  float4
#define Matrix   float4x4

struct VertexPosColor
{
    Vector3 position : POSITION;
    Vector3 color    : COLOR;
};

struct MeshVertex
{
    Vector3 position : POSITION;
    Vector2 uv       : TEXCOORD;
    // Vector3 normal   : NORMAL;
};

#else

#include <CoreTypes.h>
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
    // Vector3 normal;
};

#endif

struct PerObjectConstants
{
    Matrix world_matrix;
};

struct PerMaterialConstants
{
};

struct PerPassConstants
{
    Matrix view_matrix;
    Matrix projection_matrix;
    Vector3 camera_position;
};

struct PerFrameConstants
{
};

#endif