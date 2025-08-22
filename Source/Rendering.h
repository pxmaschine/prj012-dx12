#pragma once

#include <Asset.h>
#include <Shaders/Shared.h>
#include <Platform/DX12/DX12.h>
#include <CoreDefs.h>
#include <BitFlags.h>

struct MeshGeometryData;

struct DebugPrimitive
{
  SharedPtr<PrimitiveMeshGeometryData> m_geometry = nullptr;
  MaterialInfo m_material_info{};
  Matrix m_world_matrix = {};
};

struct ModelLoadData
{
  AssetId m_id;
  Matrix m_world_matrix;
};

class Camera
{
public:
  Matrix m_view_matrix = {};
  Matrix m_projection_matrix = {};
  Matrix m_world_matrix = {};

public:
  void update_view_matrix(const Vector3& position, const Vector3& look_at, const Vector3& up);
  void update_projection_matrix(f32 field_of_view, f32 aspect_ratio, f32 near_plane = 0.01f, f32 far_plane = 1000.0f);
  void get_transform(Vector3& position, Quaternion& rotation);
};

struct RenderObject
{
  PerObjectConstants m_constants{};
  UniquePtr<DX12BufferResource> m_vertex_buffer = nullptr;
  UniquePtr<DX12BufferResource> m_index_buffer = nullptr;
  StaticArray<UniquePtr<DX12BufferResource>, k_num_frames_in_flight> m_constant_buffers = {};
  u32 m_draw_count = 0;
};

enum class MaterialFeature : u32
{
  None = 0,
  AlbedoMap = 1 << 0,
  NormalMap = 1 << 1,
  RoughnessMap = 1 << 2,
  SpecularMap = 1 << 3,
  MetalnessMap = 1 << 4,
  AOMap = 1 << 5,
  FlipNormalY = 1 << 6,  // For OpenGL textures
  OverlayMap = 1 << 7,
  EmissiveMap = 1 << 8,
};
using MaterialFeatureFlags = BitFlags<MaterialFeature>;

struct MaterialTexture
{
  DX12PipelineResourceBinding m_texture_binding;

  bool is_valid() const { return m_texture_binding.m_resource != nullptr; }
};

struct MaterialData
{
  MaterialFeatureFlags m_feature_flags{MaterialFeature::None};
  PerMaterialConstants m_constants = {};
  StaticArray<MaterialTexture, k_num_material_textures> m_textures = {};
  DynamicArray<RenderObject*> m_render_objects{};
  StaticArray<UniquePtr<DX12BufferResource>, k_num_frames_in_flight> m_constant_buffers = {};

  void set_bound_texture(const MaterialTextureInfo& info, const AssetId& id);
};

class Renderer
{
public:
  Renderer(HWND window_handle, u32 client_width, u32 client_height, bool msaa_enabled = false);
  ~Renderer();

public:
  void push_texture(const AssetId& id);
  // TODO: pop_texture?
  void push_model(const AssetId& id, const Matrix& world_matrix = {});
  // TODO: pop_model?
  void push_debug_primitive(DebugPrimitive* debug_primitive);

  void process_previous_frame_loads();

  // DX12TextureData* create_texture(const char* path, TextureFormat format = TextureFormat::SRGB, u32 request_channels = 4);
  MaterialData* create_material_data();
  RenderObject* create_render_object(MeshGeometryData* geometry, MaterialData* material_data);
  DebugPrimitive* create_debug_primitive(SharedPtr<PrimitiveMeshGeometryData> geometry, const MaterialInfo& material_info = {}, const Matrix& world_matrix = {});
  
  DX12State* get_dx12_state() { return m_dx12_state.get(); }

  Camera* create_camera(
    const Vector3& position = {0.0f, 1.0f, -5.0f}, // TODO: Change default values
    const Vector3& look_at = {0.0f, 0.0f, 0.0f}, 
    const Vector3& up = {0.0f, 1.0f, 0.0f}, 
    f32 field_of_view = ZV_PI / 4.0f, 
    f32 aspect_ratio = 1.0f, 
    f32 near_plane = 0.01f, 
    f32 far_plane = 1000.0f);

  void set_active_camera(Camera* camera);
  void update_active_camera();

  void set_hemispheric_light_color(const Vector3& color) { m_per_frame_constants.hemispheric_light_color = Vector4(color.x, color.y, color.z, 1.0f); }
  
  GlobalLight* create_global_light(const GlobalLight& initial = {});
  PunctualLight* create_punctual_light(const PunctualLight& initial = {});

  void set_client_size(u32 width, u32 height) { m_client_width = width; m_client_height = height; }

  void begin_frame_imgui();
  void end_frame_imgui();

  void set_clear_color(const Vector4& color) { m_clear_color[0] = color.x; m_clear_color[1] = color.y; m_clear_color[2] = color.z; m_clear_color[3] = color.w; }
  void render();

private:
  void initialize_imgui(HWND window_handle);

  void create_default_graphics_pipeline();

  bool check_dependencies(const MaterialInfo& material_info);

  void setup_render_resources(ModelAsset* model_asset, const Matrix& world_matrix);
  void setup_render_resources(TextureAsset* texture_asset);
  void setup_render_resources(DebugPrimitive* debug_primitive);

private:
  UniquePtr<DX12State> m_dx12_state = nullptr;
  UniquePtr<DX12GraphicsCommandContext> m_dx12_graphics_ctx = nullptr;

  DX12PipelineResourceSpace m_per_object_resource_space{};
  DX12PipelineResourceSpace m_per_material_resource_space{};
  DX12PipelineResourceSpace m_per_pass_resource_space{};
  DX12PipelineResourceSpace m_per_frame_resource_space{};

  struct FrameResources
  {
    // UniquePtr<DX12BufferResource> m_per_object_constant_buffer = nullptr;
    // UniquePtr<DX12BufferResource> m_per_material_constant_buffer = nullptr;
    UniquePtr<DX12BufferResource> m_per_pass_constant_buffer = nullptr;
    UniquePtr<DX12BufferResource> m_per_frame_constant_buffer = nullptr;
  };
  StaticArray<FrameResources, k_num_frames_in_flight> m_frame_resources = {};

  UniquePtr<DX12BufferResource> m_per_object_constant_buffer_dummy = nullptr;
  UniquePtr<DX12BufferResource> m_per_material_constant_buffer_dummy = nullptr;

  UniquePtr<DX12PipelineState> m_default_graphics_pipeline = nullptr;

  PerFrameConstants m_per_frame_constants{};
  PerPassConstants m_per_pass_constants{};

  f32 m_clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
  u32 m_client_width = 0;
  u32 m_client_height = 0;
  bool m_msaa_enabled = false;
  DynamicArray<UniquePtr<DX12TextureData>> m_textures{};
  DynamicArray<UniquePtr<RenderObject>> m_render_objects{};
  DynamicArray<UniquePtr<MaterialData>> m_material_data{};

  DynamicArray<UniquePtr<Camera>> m_cameras{};
  Camera* m_active_camera = nullptr;

  DynamicArray<ModelLoadData> m_previous_pending_model_loads{};
  DynamicArray<ModelLoadData> m_pending_model_loads{};

  DynamicArray<AssetId> m_previous_pending_texture_loads{};
  DynamicArray<AssetId> m_pending_texture_loads{};

  DynamicArray<UniquePtr<DebugPrimitive>> m_debug_primitives{};
  DynamicArray<DebugPrimitive*> m_previous_pending_debug_primitive_loads{};
  DynamicArray<DebugPrimitive*> m_pending_debug_primitive_loads{};
};
