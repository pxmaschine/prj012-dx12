#pragma once

#include <CoreDefs.h>
#include <MathLib.h>
#include <Shaders/Shared.h>


struct MeshGeometryData
{
  DynamicArray<MeshVertex> m_vertices{};
  DynamicArray<u16> m_indices{};

  size_t vertices_size() const { return m_vertices.size() * sizeof(MeshVertex); }
  size_t indices_size() const { return m_indices.size() * sizeof(u16); }
};

struct PrimitiveMeshGeometryData : public MeshGeometryData
{
  enum class Type : u8
  {
    Triangle  = 1 << 0,
    Quad      = 1 << 1,
    Plane     = 1 << 2,
    Box       = 1 << 3,
    Sphere    = 1 << 4,
    Icosphere = 1 << 5,
    Cylinder  = 1 << 6,
    Capsule   = 1 << 7,
  };
  Type m_type;
};

SharedPtr<PrimitiveMeshGeometryData> create_triangle(
    Vector3 p1 = {-0.5f, -0.5f, 0.0f}, 
    Vector3 p2 = {0.0f, 0.5f, 0.0f}, 
    Vector3 p3 = {0.5f, -0.5f, 0.0f});
SharedPtr<PrimitiveMeshGeometryData> create_quad(
    f32 width = 1.0f, 
    f32 height = 1.0f);
SharedPtr<PrimitiveMeshGeometryData> create_plane(
    f32 width = 1.0f, 
    f32 height = 1.0f, 
    u32 width_segments = 1, 
    u32 height_segments = 1);
SharedPtr<PrimitiveMeshGeometryData> create_box(
    f32 width = 1.0f, 
    f32 height = 1.0f, 
    f32 depth = 1.0f, 
    u32 num_subdivisions = 0);
SharedPtr<PrimitiveMeshGeometryData> create_sphere(
    f32 radius = 1.0f, 
    u32 width_segments = 16, 
    u32 height_segments = 8);
SharedPtr<PrimitiveMeshGeometryData> create_icosphere(
    f32 radius = 1.0f, 
    u32 num_subdivisions = 4);
SharedPtr<PrimitiveMeshGeometryData> create_cylinder(
    f32 bottom_radius = 0.5f, 
    f32 top_radius = 0.5f, 
    f32 height = 1.0f, 
    u32 radial_segments = 16, 
    u32 height_segments = 1);
SharedPtr<PrimitiveMeshGeometryData> create_capsule(
    f32 radius = 0.5f, 
    f32 height = 1.0f, 
    u32 radial_segments = 16, 
    u32 height_segments = 1, 
    u32 cap_segments = 16);
