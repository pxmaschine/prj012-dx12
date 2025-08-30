#include <Rendering.h>

#include <Geometry.h>

#include <ThirdParty/imgui/imgui.h>
#include <ThirdParty/imgui/imgui_impl_win32.h>
#include <ThirdParty/imgui/imgui_impl_dx12.h>


namespace
{
  inline void read_material_data_from_info(const MaterialInfo& material_info, MaterialData* out_material_data)
  {
    MaterialTextureInfo texture_info{};

    if (material_info.m_albedo_texture_id.is_valid())
    {
      texture_info.m_type = MaterialTextureType::Albedo;
      out_material_data->set_bound_texture(texture_info, material_info.m_albedo_texture_id);
    }
    if (material_info.m_normal_texture_id.is_valid())
    {
      texture_info.m_type = MaterialTextureType::Normal;
      out_material_data->set_bound_texture(texture_info, material_info.m_normal_texture_id);
    }
    if (material_info.m_metallic_roughness_texture_id.is_valid())
    {
      texture_info.m_type = MaterialTextureType::MetallicRoughness;
      texture_info.m_channel_packing = material_info.m_channel_packing;
      out_material_data->set_bound_texture(texture_info, material_info.m_metallic_roughness_texture_id);
    }
    if (material_info.m_ao_texture_id.is_valid())
    {
      texture_info.m_type = MaterialTextureType::AO;
      out_material_data->set_bound_texture(texture_info, material_info.m_ao_texture_id);
    }
    if (material_info.m_emissive_texture_id.is_valid())
    {
      texture_info.m_type = MaterialTextureType::Emissive;
      out_material_data->set_bound_texture(texture_info, material_info.m_emissive_texture_id);
    }
    if (material_info.m_overlay_texture_id.is_valid())
    {
      texture_info.m_type = MaterialTextureType::Overlay;
      out_material_data->set_bound_texture(texture_info, material_info.m_overlay_texture_id);
    }
    if (material_info.m_specular_texture_id.is_valid())
    {
      texture_info.m_type = MaterialTextureType::Specular;
      out_material_data->set_bound_texture(texture_info, material_info.m_specular_texture_id);
    }

    out_material_data->m_constants.albedo_color = material_info.m_albedo_color;
    out_material_data->m_constants.roughness = material_info.m_roughness;
    out_material_data->m_constants.metallic = material_info.m_metalness;
    out_material_data->m_constants.specular = material_info.m_specular;
    out_material_data->m_constants.emissive = material_info.m_emissive;
    out_material_data->m_constants.sampler_mode = material_info.m_sampler_mode;
  }
}


Renderer::Renderer(HWND window_handle, u32 client_width, u32 client_height, bool msaa_enabled, DX12OutputMode output_mode, TonemapType tonemap_type)
  : m_client_width(client_width)
  , m_client_height(client_height)
  , m_msaa_enabled(msaa_enabled)
  , m_tonemap_type(tonemap_type)
{
  m_dx12_state = make_unique_ptr<DX12State>(window_handle, client_width, client_height, false, msaa_enabled, output_mode);

  m_dx12_graphics_ctx = m_dx12_state->create_graphics_context();

  create_default_graphics_pipeline();

  initialize_imgui(window_handle);
}

void Renderer::initialize_imgui(HWND window_handle)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    // (void)io;
  
    DX12Descriptor descriptor = m_dx12_state->get_imgui_descriptor(0);
    DX12Descriptor descriptor2 = m_dx12_state->get_imgui_descriptor(1);
  
    ImGui_ImplWin32_Init(window_handle);
  
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = m_dx12_state->get_device();
    init_info.CommandQueue = m_dx12_state->get_graphics_queue();
    init_info.NumFramesInFlight = k_num_frames_in_flight;
    init_info.RTVFormat = m_dx12_state->get_back_buffer_format();
    init_info.LegacySingleSrvCpuDescriptor = descriptor.m_cpu_handle;
    init_info.LegacySingleSrvGpuDescriptor = descriptor.m_gpu_handle;
    init_info.LegacySingleSrvCpuDescriptor2 = descriptor2.m_cpu_handle;
    init_info.LegacySingleSrvGpuDescriptor2 = descriptor2.m_gpu_handle;
    ImGui_ImplDX12_Init(&init_info);
}

Renderer::~Renderer()
{
  m_dx12_state->flush_queues();

  for (auto& render_texture : m_textures)
  {
    m_dx12_state->destroy_texture_resource(move_ptr(render_texture->m_texture->m_texture_resource));
  }

  m_dx12_state->destroy_buffer_resource(move_ptr(m_per_object_constant_buffer_dummy));
  m_dx12_state->destroy_buffer_resource(move_ptr(m_per_material_constant_buffer_dummy));

  for (auto& render_object : m_render_objects)
  {
    m_dx12_state->destroy_buffer_resource(move_ptr(render_object->m_vertex_buffer));
    m_dx12_state->destroy_buffer_resource(move_ptr(render_object->m_index_buffer));

    for (auto& constant_buffer : render_object->m_constant_buffers)
    {
      m_dx12_state->destroy_buffer_resource(move_ptr(constant_buffer));
    }
  }

  for (auto& material_data : m_material_data)
  {
    for (auto& constant_buffer : material_data->m_constant_buffers)
    {
      m_dx12_state->destroy_buffer_resource(move_ptr(constant_buffer));
    }
  }

  for (auto& frame_resource : m_frame_resources)
  {
    m_dx12_state->destroy_buffer_resource(move_ptr(frame_resource.m_per_pass_constant_buffer));
    m_dx12_state->destroy_buffer_resource(move_ptr(frame_resource.m_per_frame_constant_buffer));
  }

  if (m_default_graphics_pipeline)
  {
    m_dx12_state->destroy_pipeline_state(move_ptr(m_default_graphics_pipeline));
  }

  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  if (m_dx12_graphics_ctx)
  {
    m_dx12_state->destroy_context(move_ptr(m_dx12_graphics_ctx));
  }

  m_dx12_state = nullptr;
}

void Renderer::push_texture(const AssetId& id)
{
  TextureAsset* texture_asset = Assets::get_texture_asset(id);

  if (!texture_asset)
  {
    Assets::load_texture_asset_async(id);

    m_pending_texture_loads.emplace(id);
  }
  else
  {
    setup_render_resources(texture_asset);
  }
}

void Renderer::push_model(const AssetId& id, const Matrix& world_matrix)
{
  ModelAsset* model_asset = Assets::get_model_asset(id);

  if (!model_asset)
  {
    Assets::load_model_asset_async(id);

    m_pending_model_loads.emplace(id, ModelLoadData{ id, world_matrix });
  }
  else
  {
    setup_render_resources(model_asset, world_matrix);
  }
}

void Renderer::process_previous_frame_loads()
{
  const HashSet<AssetId>&& temp1 = move_ptr(m_previous_pending_texture_loads);
  m_previous_pending_texture_loads = move_ptr(m_pending_texture_loads);
  m_pending_texture_loads = move_ptr(temp1);

  m_pending_texture_loads.clear();

  for (auto& id : m_previous_pending_texture_loads)
  {
    TextureAsset* texture_asset = Assets::get_texture_asset(id);
    if (texture_asset)
    {
      setup_render_resources(texture_asset);
    }
    else
    {
     m_pending_texture_loads.emplace(id);
    }
  }

  const HashMap<AssetId, ModelLoadData>&& temp2 = move_ptr(m_previous_pending_model_loads);
  m_previous_pending_model_loads = move_ptr(m_pending_model_loads);
  m_pending_model_loads = move_ptr(temp2);

  m_pending_model_loads.clear();
  
  for (auto& pair : m_previous_pending_model_loads)
  {
    ModelAsset* model_asset = Assets::get_model_asset(pair.first);
    if (model_asset)
    {
      setup_render_resources(model_asset, pair.second.m_world_matrix);
    }
    else
    {
      m_pending_model_loads.emplace(pair);
    }
  }

  const DynamicArray<DebugPrimitive*>&& temp3 = move_ptr(m_previous_pending_debug_primitive_loads);
  m_previous_pending_debug_primitive_loads = move_ptr(m_pending_debug_primitive_loads);
  m_pending_debug_primitive_loads = move_ptr(temp3);

  m_pending_debug_primitive_loads.clear();

  for (auto& debug_primitive : m_previous_pending_debug_primitive_loads)
  {
    setup_render_resources(debug_primitive);
  }
}

void Renderer::setup_render_resources(ModelAsset* model_asset, const Matrix& world_matrix)
{
  for (auto& submesh : model_asset->m_submeshes)
  {
    if (!check_dependencies(submesh.m_material_info))
    {
      m_pending_model_loads.emplace(model_asset->m_id, ModelLoadData{ model_asset->m_id, world_matrix });
      return;
    }
  }

  for (auto& submesh : model_asset->m_submeshes)
  {
    MaterialData* material_data = create_material_data();
    read_material_data_from_info(submesh.m_material_info, material_data);

    RenderObject* render_object = create_render_object(&submesh.m_data, material_data);
    if (submesh.m_parent == SubmeshHandle::Invalid)
    {
      render_object->m_constants.world_matrix = submesh.m_world_transform * world_matrix;
    }
    else
    {
      render_object->m_constants.world_matrix = submesh.m_world_transform;
    }
  }
}

void Renderer::setup_render_resources(TextureAsset* texture_asset)
{
  UniquePtr<RenderTexture> render_texture = make_unique_ptr<RenderTexture>();
  render_texture->m_texture = move_ptr(m_dx12_state->create_texture_data(texture_asset));
#if ZV_DEBUG
  render_texture->m_asset_name = texture_asset->m_id.name();
#endif
  m_textures.emplace_back(move_ptr(render_texture));
  DX12TextureData* dx12_texture_data = m_textures.back()->m_texture.get();

  DX12UploadCommandContext* dx12_upload_ctx = m_dx12_state->get_upload_context_for_current_frame();
  dx12_upload_ctx->record_texture_upload(dx12_texture_data);

  texture_asset->m_texture_data = dx12_texture_data;
}

void Renderer::setup_render_resources(DebugPrimitive* debug_primitive)
{
  if (!check_dependencies(debug_primitive->m_material_info))
  {
    m_pending_debug_primitive_loads.emplace_back(debug_primitive);
    return;
  }

  MaterialData* material_data = create_material_data();
  read_material_data_from_info(debug_primitive->m_material_info, material_data);

  RenderObject* render_object = create_render_object(debug_primitive->m_geometry.get(), material_data);
  render_object->m_constants.world_matrix = debug_primitive->m_world_matrix;
}

bool Renderer::check_dependencies(const MaterialInfo& material_info)
{
  bool all_dependencies_ready = true;

  auto check_texture = [&](const AssetId& id)
  {
    // If the texture id is invalid, it means that the texture is not needed for this material
    if (!id.is_valid())
    {
      return true;
    }

    TextureAsset* texture_asset = Assets::get_texture_asset(id);
    if (!texture_asset)
    {
      push_texture(id);
      return false;
    }
    
    return /*texture_asset->m_state == AssetState::Loaded && */texture_asset->is_ready();
  };

  all_dependencies_ready &= check_texture(material_info.m_albedo_texture_id);
  all_dependencies_ready &= check_texture(material_info.m_normal_texture_id);
  all_dependencies_ready &= check_texture(material_info.m_metallic_roughness_texture_id);
  all_dependencies_ready &= check_texture(material_info.m_ao_texture_id);
  all_dependencies_ready &= check_texture(material_info.m_emissive_texture_id);
  all_dependencies_ready &= check_texture(material_info.m_overlay_texture_id);
  all_dependencies_ready &= check_texture(material_info.m_specular_texture_id);

  return all_dependencies_ready;
}

DebugPrimitive* Renderer::create_debug_primitive(SharedPtr<PrimitiveMeshGeometryData> geometry, const MaterialInfo& material_info, const Matrix& world_matrix)
{
  m_debug_primitives.emplace_back(make_unique_ptr<DebugPrimitive>());
  DebugPrimitive* debug_primitive = m_debug_primitives.back().get();

  debug_primitive->m_geometry = geometry;
  debug_primitive->m_material_info = material_info;
  debug_primitive->m_world_matrix = world_matrix;

  return debug_primitive;
}

void Renderer::push_debug_primitive(DebugPrimitive* debug_primitive)
{
  setup_render_resources(debug_primitive);
}

MaterialData* Renderer::create_material_data()
{
  m_material_data.emplace_back(make_unique_ptr<MaterialData>());

  MaterialData* material_data = m_material_data.back().get();

  DX12BufferResource::Desc per_material_cb_desc{};
  per_material_cb_desc.m_size = sizeof(PerMaterialConstants);
  per_material_cb_desc.m_access = DX12ResourceAccess::HostWritable;
  per_material_cb_desc.m_view_flags.set(DX12BufferViewFlags::CBV);

  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    material_data->m_constant_buffers[i] = m_dx12_state->create_buffer_resource(per_material_cb_desc);
  }
  material_data->m_constant_buffers[0]->copy_data(&material_data->m_constants, sizeof(PerMaterialConstants));

  return material_data;
}

RenderObject* Renderer::create_render_object(MeshGeometryData* geometry, MaterialData* material_data)
{
  m_render_objects.emplace_back(make_unique_ptr<RenderObject>());

  RenderObject* render_object = m_render_objects.back().get();
  render_object->m_draw_count = static_cast<u32>(geometry->m_indices.size());
  material_data->m_render_objects.emplace_back(render_object);

  DX12UploadCommandContext* dx12_upload_ctx = m_dx12_state->get_upload_context_for_current_frame();

  // Create vertex buffer
  DX12BufferResource::Desc vb_desc{};
  vb_desc.m_size = static_cast<u32>(geometry->vertices_size());
  vb_desc.m_stride = sizeof(MeshVertex);
  vb_desc.m_access = DX12ResourceAccess::GpuOnly;
  vb_desc.m_buffer_type = DX12BufferResource::Desc::BufferType::VertexBuffer;
  render_object->m_vertex_buffer = move_ptr(m_dx12_state->create_buffer_resource(vb_desc));
  dx12_upload_ctx->record_buffer_upload(render_object->m_vertex_buffer.get(), geometry->m_vertices.data(), static_cast<u32>(geometry->vertices_size()));

  // Create index buffer
  DX12BufferResource::Desc ib_desc{};
  ib_desc.m_size = static_cast<u32>(geometry->indices_size());
  ib_desc.m_stride = sizeof(u16);
  ib_desc.m_access = DX12ResourceAccess::GpuOnly;
  ib_desc.m_buffer_type = DX12BufferResource::Desc::BufferType::IndexBuffer;
  render_object->m_index_buffer = move_ptr(m_dx12_state->create_buffer_resource(ib_desc));
  dx12_upload_ctx->record_buffer_upload(render_object->m_index_buffer.get(), geometry->m_indices.data(), static_cast<u32>(geometry->indices_size()));

  // Create per object constant buffer

  DX12BufferResource::Desc per_object_cb_desc{};
  per_object_cb_desc.m_size = sizeof(PerObjectConstants);
  per_object_cb_desc.m_access = DX12ResourceAccess::HostWritable;
  per_object_cb_desc.m_view_flags.set(DX12BufferViewFlags::CBV);
  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    render_object->m_constant_buffers[i] = m_dx12_state->create_buffer_resource(per_object_cb_desc);
  }
  render_object->m_constant_buffers[0]->copy_data(&render_object->m_constants, sizeof(PerObjectConstants));

  return render_object;
}

void Renderer::create_default_graphics_pipeline()
{
  Assets::load_texture_asset("dummy");

  push_texture("dummy");
  
  TextureAsset* dummy_asset = Assets::get_texture_asset("dummy");

  zv_assert_msg(dummy_asset != nullptr, "Dummy texture not found");
  zv_assert_msg(dummy_asset->is_ready(), "Dummy texture not ready");

  DX12TextureData* dummy_texture = dummy_asset->m_texture_data;

  // TODO: BUG
  // DX12UploadCommandContext* dx12_upload_ctx = m_dx12_state->get_upload_context_for_current_frame();
  // dx12_upload_ctx->record_texture_upload(dummy_texture);

  // Prepare frame resources

  DX12BufferResource::Desc per_object_cb_desc{};
  per_object_cb_desc.m_size = sizeof(PerObjectConstants);
  per_object_cb_desc.m_access = DX12ResourceAccess::HostWritable;
  per_object_cb_desc.m_view_flags.set(DX12BufferViewFlags::CBV);

  DX12BufferResource::Desc per_material_cb_desc{};
  per_material_cb_desc.m_size = sizeof(PerMaterialConstants);
  per_material_cb_desc.m_access = DX12ResourceAccess::HostWritable;
  per_material_cb_desc.m_view_flags.set(DX12BufferViewFlags::CBV);

  DX12BufferResource::Desc per_pass_cb_desc{};
  per_pass_cb_desc.m_size = sizeof(PerPassConstants);
  per_pass_cb_desc.m_access = DX12ResourceAccess::HostWritable;
  per_pass_cb_desc.m_view_flags.set(DX12BufferViewFlags::CBV);

  DX12BufferResource::Desc per_frame_cb_desc{};
  per_frame_cb_desc.m_size = sizeof(PerFrameConstants);
  per_frame_cb_desc.m_access = DX12ResourceAccess::HostWritable;
  per_frame_cb_desc.m_view_flags.set(DX12BufferViewFlags::CBV);

  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    m_frame_resources[i].m_per_pass_constant_buffer = m_dx12_state->create_buffer_resource(per_pass_cb_desc);
    m_frame_resources[i].m_per_frame_constant_buffer = m_dx12_state->create_buffer_resource(per_frame_cb_desc);
  }

  // Initialize per object constant buffer

  m_per_object_constant_buffer_dummy = m_dx12_state->create_buffer_resource(per_object_cb_desc);

  PerObjectConstants object_constants{};
  m_per_object_constant_buffer_dummy->copy_data(&object_constants, sizeof(PerObjectConstants));

  m_per_object_resource_space.set_cbv(m_per_object_constant_buffer_dummy.get());
  m_per_object_resource_space.lock();

  // Initialize per material constant buffer

  m_per_material_constant_buffer_dummy = m_dx12_state->create_buffer_resource(per_material_cb_desc);

  PerMaterialConstants material_constants{};
  m_per_material_constant_buffer_dummy->copy_data(&material_constants, sizeof(PerMaterialConstants));

  DX12PipelineResourceBinding diffuse_texture_binding{};
  diffuse_texture_binding.m_resource = dummy_texture->m_texture_resource.get();
  diffuse_texture_binding.m_binding_index = 0;

  DX12PipelineResourceBinding normal_texture_binding{};
  normal_texture_binding.m_resource = dummy_texture->m_texture_resource.get();
  normal_texture_binding.m_binding_index = 1;

  DX12PipelineResourceBinding metallic_roughness_texture_binding{};
  metallic_roughness_texture_binding.m_resource = dummy_texture->m_texture_resource.get();
  metallic_roughness_texture_binding.m_binding_index = 2;

  DX12PipelineResourceBinding ao_texture_binding{};
  ao_texture_binding.m_resource = dummy_texture->m_texture_resource.get();
  ao_texture_binding.m_binding_index = 3;

  DX12PipelineResourceBinding emissive_texture_binding{};
  emissive_texture_binding.m_resource = dummy_texture->m_texture_resource.get();
  emissive_texture_binding.m_binding_index = 4;

  DX12PipelineResourceBinding specular_texture_binding{};
  specular_texture_binding.m_resource = dummy_texture->m_texture_resource.get();
  specular_texture_binding.m_binding_index = 5;

  DX12PipelineResourceBinding overlay_texture_binding{};
  overlay_texture_binding.m_resource = dummy_texture->m_texture_resource.get();
  overlay_texture_binding.m_binding_index = 6;

  m_per_material_resource_space.set_cbv(m_per_material_constant_buffer_dummy.get());
  m_per_material_resource_space.set_srv(diffuse_texture_binding);
  m_per_material_resource_space.set_srv(normal_texture_binding);
  m_per_material_resource_space.set_srv(metallic_roughness_texture_binding);
  m_per_material_resource_space.set_srv(ao_texture_binding);
  m_per_material_resource_space.set_srv(emissive_texture_binding);
  m_per_material_resource_space.set_srv(specular_texture_binding);
  m_per_material_resource_space.set_srv(overlay_texture_binding);
  m_per_material_resource_space.lock();

  // Initialize per pass constant buffer

  m_frame_resources[0].m_per_pass_constant_buffer->copy_data(&m_per_pass_constants, sizeof(PerPassConstants));

  m_per_pass_resource_space.set_cbv(m_frame_resources[0].m_per_pass_constant_buffer.get());
  m_per_pass_resource_space.lock();

  // Initialize per frame constant buffer

  m_per_frame_constants.output_mode = static_cast<OutputMode>(m_dx12_state->get_output_mode());
  m_per_frame_constants.reference_white_nits = m_dx12_state->get_reference_white_nits();
  m_per_frame_constants.exposure = 1.0f;
  m_per_frame_constants.tonemap_type = m_tonemap_type;
  m_frame_resources[0].m_per_frame_constant_buffer->copy_data(&m_per_frame_constants, sizeof(PerFrameConstants));

  m_per_frame_resource_space.set_cbv(m_frame_resources[0].m_per_frame_constant_buffer.get());
  m_per_frame_resource_space.lock();

  // Create pipeline state object

  DX12PipelineState::Desc pipeline_desc = get_default_pipeline_state_desc();
  pipeline_desc.m_vs_path = L"Shaders/Default_vs.cso";
  pipeline_desc.m_ps_path = L"Shaders/Default_ps.cso";
  pipeline_desc.m_render_target_desc.m_num_render_targets = 1;
  pipeline_desc.m_render_target_desc.m_render_target_formats[0] = m_dx12_state->get_back_buffer_format();
  pipeline_desc.m_render_target_desc.m_depth_stencil_format = DXGI_FORMAT_D32_FLOAT;
  pipeline_desc.m_spaces[DX12ResourceSpace::PerObjectSpace] = &m_per_object_resource_space;
  pipeline_desc.m_spaces[DX12ResourceSpace::PerMaterialSpace] = &m_per_material_resource_space;
  pipeline_desc.m_spaces[DX12ResourceSpace::PerPassSpace] = &m_per_pass_resource_space;
  pipeline_desc.m_spaces[DX12ResourceSpace::PerFrameSpace] = &m_per_frame_resource_space;
  pipeline_desc.m_input_layout.m_elements[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
  pipeline_desc.m_input_layout.m_elements[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
  pipeline_desc.m_input_layout.m_elements[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
  pipeline_desc.m_input_layout.m_elements[3] = { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
  pipeline_desc.m_input_layout.m_num_elements = 4;
  pipeline_desc.m_depth_stencil_desc.DepthEnable = true;
  pipeline_desc.m_depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  pipeline_desc.m_sample_desc.Count = m_msaa_enabled ? 4 : 1;
  pipeline_desc.m_sample_desc.Quality = m_msaa_enabled ? m_dx12_state->get_msaa_quality_level() - 1 : 0;

  m_default_graphics_pipeline = m_dx12_state->create_graphics_pipeline(pipeline_desc);
}

Camera* Renderer::create_camera(
  const Vector3& position, 
  const Vector3& look_at, 
  const Vector3& up, 
  f32 field_of_view, 
  f32 aspect_ratio, 
  f32 near_plane, 
  f32 far_plane)
{
  // TODO: Validate position, look_at, up, field_of_view?, aspect_ratio?, near_plane?, far_plane?

  m_cameras.emplace_back(make_unique_ptr<Camera>());

  Camera* camera = m_cameras.back().get();
  camera->update_view_matrix(position, look_at, up);
  camera->update_projection_matrix(field_of_view, aspect_ratio, near_plane, far_plane);

  return camera;
}

void Renderer::set_active_camera(Camera* camera)
{
  m_active_camera = camera;

  update_active_camera();
}

void Renderer::update_active_camera()
{
  m_per_pass_constants.view_matrix = m_active_camera->m_view_matrix;
  m_per_pass_constants.projection_matrix = m_active_camera->m_projection_matrix;

  m_per_pass_constants.camera_position.x = m_active_camera->m_world_matrix._41;
  m_per_pass_constants.camera_position.y = m_active_camera->m_world_matrix._42;
  m_per_pass_constants.camera_position.z = m_active_camera->m_world_matrix._43;
  m_per_pass_constants.camera_position.w = 1.0f;
}

GlobalLight* Renderer::create_global_light(const GlobalLight& initial)
{
  m_per_frame_constants.global_light = initial;
  return &m_per_frame_constants.global_light;
}

PunctualLight* Renderer::create_punctual_light(const PunctualLight& initial)
{
  if (m_per_frame_constants.num_punctual_lights >= k_max_lights)
  {
    zv_error("Max number of punctual lights reached.");
    return nullptr;
  }

  PunctualLight* new_light = &m_per_frame_constants.punctual_lights[m_per_frame_constants.num_punctual_lights++];
  *new_light = initial;
  return new_light;
}

void Renderer::begin_frame_imgui()
{
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}

void Renderer::end_frame_imgui()
{
  ImGui::Render();
}

void Renderer::render()
{
  m_dx12_state->begin_frame();

  DX12TextureResource* render_target = m_msaa_enabled ? m_dx12_state->get_msaa_render_target() : m_dx12_state->get_current_back_buffer();
  DX12TextureResource* depth_buffer = m_dx12_state->get_depth_stencil_buffer();

  m_dx12_graphics_ctx->reset();
  m_dx12_graphics_ctx->add_barrier(render_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
  m_dx12_graphics_ctx->add_barrier(depth_buffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
  m_dx12_graphics_ctx->flush_barriers();

  m_dx12_graphics_ctx->clear_render_target(render_target, m_clear_color);
  m_dx12_graphics_ctx->clear_depth_stencil_target(depth_buffer);

  DX12PipelineInfo pipeline_info{};
  pipeline_info.m_pipeline = m_default_graphics_pipeline.get();
  pipeline_info.m_render_targets.emplace_back(render_target);
  pipeline_info.m_depth_stencil_target = depth_buffer;
  m_dx12_graphics_ctx->set_pipeline(pipeline_info);

  FrameResources* frame_resources = &m_frame_resources[m_dx12_state->get_frame_id()];

  frame_resources->m_per_frame_constant_buffer->copy_data(&m_per_frame_constants, sizeof(PerFrameConstants));
  m_per_frame_resource_space.set_cbv(frame_resources->m_per_frame_constant_buffer.get());

  m_dx12_graphics_ctx->set_pipeline_resources(DX12ResourceSpace::PerFrameSpace, &m_per_frame_resource_space);

  frame_resources->m_per_pass_constant_buffer->copy_data(&m_per_pass_constants, sizeof(PerPassConstants));
  m_per_pass_resource_space.set_cbv(frame_resources->m_per_pass_constant_buffer.get());

  m_dx12_graphics_ctx->set_pipeline_resources(DX12ResourceSpace::PerPassSpace, &m_per_pass_resource_space);

  for (auto& material_data : m_material_data)
  {
    material_data->m_constant_buffers[m_dx12_state->get_frame_id()]->copy_data(&material_data->m_constants, sizeof(PerMaterialConstants));
    m_per_material_resource_space.set_cbv(material_data->m_constant_buffers[m_dx12_state->get_frame_id()].get());

    MaterialTexture& albedo_texture = material_data->m_textures[static_cast<u32>(MaterialTextureType::Albedo)];
    MaterialTexture& normal_texture = material_data->m_textures[static_cast<u32>(MaterialTextureType::Normal)];
    MaterialTexture& metallic_roughness_texture = material_data->m_textures[static_cast<u32>(MaterialTextureType::MetallicRoughness)];
    MaterialTexture& specular_texture = material_data->m_textures[static_cast<u32>(MaterialTextureType::Specular)];
    MaterialTexture& ao_texture = material_data->m_textures[static_cast<u32>(MaterialTextureType::AO)];
    MaterialTexture& overlay_texture = material_data->m_textures[static_cast<u32>(MaterialTextureType::Overlay)];
    MaterialTexture& emissive_texture = material_data->m_textures[static_cast<u32>(MaterialTextureType::Emissive)];

    if (albedo_texture.is_valid())
    {
      m_per_material_resource_space.set_srv(albedo_texture.m_texture_binding);
    }
    if (normal_texture.is_valid())
    {
      m_per_material_resource_space.set_srv(normal_texture.m_texture_binding);
    }
    if (metallic_roughness_texture.is_valid())
    {
      m_per_material_resource_space.set_srv(metallic_roughness_texture.m_texture_binding);
    }
    if (specular_texture.is_valid())
    {
      m_per_material_resource_space.set_srv(specular_texture.m_texture_binding);
    }
    if (ao_texture.is_valid())
    {
      m_per_material_resource_space.set_srv(ao_texture.m_texture_binding);
    }
    if (emissive_texture.is_valid())
    {
      m_per_material_resource_space.set_srv(emissive_texture.m_texture_binding);
    }
    if (overlay_texture.is_valid())
    {
      m_per_material_resource_space.set_srv(overlay_texture.m_texture_binding);
    }

    m_dx12_graphics_ctx->set_pipeline_resources(DX12ResourceSpace::PerMaterialSpace, &m_per_material_resource_space);

    for (auto& render_object : material_data->m_render_objects)
    {
      render_object->m_constant_buffers[m_dx12_state->get_frame_id()]->copy_data(&render_object->m_constants, sizeof(PerObjectConstants));
      m_per_object_resource_space.set_cbv(render_object->m_constant_buffers[m_dx12_state->get_frame_id()].get());

      m_dx12_graphics_ctx->set_pipeline_resources(DX12ResourceSpace::PerObjectSpace, &m_per_object_resource_space);
      m_dx12_graphics_ctx->set_vertex_buffer(render_object->m_vertex_buffer.get());
      m_dx12_graphics_ctx->set_index_buffer(render_object->m_index_buffer.get());
      m_dx12_graphics_ctx->set_viewport_and_scissor(m_client_width, m_client_height);
      m_dx12_graphics_ctx->set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      m_dx12_graphics_ctx->draw_indexed(render_object->m_draw_count);
    }
  }

  if (m_msaa_enabled)
  {
    DX12TextureResource* msaa_rt = render_target;
    DX12TextureResource* current_back_buffer = m_dx12_state->get_current_back_buffer();

    m_dx12_graphics_ctx->add_barrier(msaa_rt, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    m_dx12_graphics_ctx->add_barrier(current_back_buffer, D3D12_RESOURCE_STATE_RESOLVE_DEST);

    m_dx12_graphics_ctx->flush_barriers();

    m_dx12_graphics_ctx->resolve_msaa_render_target(msaa_rt, current_back_buffer);

    render_target = current_back_buffer;

    m_dx12_graphics_ctx->add_barrier(render_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_dx12_graphics_ctx->flush_barriers();
  }

  DX12PipelineInfo imgui_pipeline_info{};
  imgui_pipeline_info.m_pipeline = nullptr;
  imgui_pipeline_info.m_render_targets.emplace_back(render_target);
  imgui_pipeline_info.m_depth_stencil_target = nullptr;
  m_dx12_graphics_ctx->set_pipeline(imgui_pipeline_info);

  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_dx12_graphics_ctx->get_command_list());

  m_dx12_graphics_ctx->add_barrier(render_target, D3D12_RESOURCE_STATE_PRESENT);
  m_dx12_graphics_ctx->flush_barriers();

  m_dx12_state->submit_context(m_dx12_graphics_ctx.get());

  m_dx12_state->end_frame();
  m_dx12_state->present();
}

void Camera::get_transform(Vector3& position, Quaternion& rotation)
{
  // TODO: We may have to do this anyway, in case someone modifies view_matrix directly!!
  // m_world_matrix = m_view_matrix.Invert();

  Vector3 scale{};
  if (!m_world_matrix.Decompose(scale, rotation, position))
  {
    zv_warning("Failed to decompose view matrix.");
  }
}

void Camera::update_view_matrix(const Vector3& position, const Vector3& look_at, const Vector3& up)
{
  m_view_matrix = DirectX::XMMatrixLookAtLH(position, look_at, up);
  m_world_matrix = m_view_matrix.Invert();
}

void Camera::update_projection_matrix(f32 field_of_view, f32 aspect_ratio, f32 near_plane, f32 far_plane)
{
  m_projection_matrix = DirectX::XMMatrixPerspectiveFovLH(field_of_view, aspect_ratio, near_plane, far_plane);
}

void MaterialData::set_bound_texture(const MaterialTextureInfo& info, const AssetId& id)
{
  TextureAsset* texture_asset = Assets::get_texture_asset(id);

  zv_assert_msg(texture_asset != nullptr, "Texture asset not found: {}", id.name().c_str());
  zv_assert_msg(texture_asset->is_ready(), "Texture asset not ready: {}", id.name().c_str());

  m_textures[static_cast<u32>(info.m_type)].m_texture_binding.m_resource = texture_asset->m_texture_data->m_texture_resource.get();
  m_textures[static_cast<u32>(info.m_type)].m_texture_binding.m_binding_index = static_cast<u32>(info.m_type);

  switch (info.m_type)
  {
  case MaterialTextureType::Albedo:
    m_feature_flags.set(MaterialFeature::AlbedoMap);
    break;
  case MaterialTextureType::Normal:
    m_feature_flags.set(MaterialFeature::NormalMap);
    break;
  case MaterialTextureType::MetallicRoughness:
  {
    zv_assert_msg(info.m_channel_packing != ChannelPacking::None, "Channel packing info is required for metallic roughness texture!");
    if (info.m_channel_packing.is_set(ChannelPacking::Roughness))
    {
      m_feature_flags.set(MaterialFeature::RoughnessMap);
    }
    if (info.m_channel_packing.is_set(ChannelPacking::Metalness))
    {
      m_feature_flags.set(MaterialFeature::MetalnessMap);
    }
    break;
  }
  case MaterialTextureType::Specular:
    m_feature_flags.set(MaterialFeature::SpecularMap);
    break;
  case MaterialTextureType::AO:
    m_feature_flags.set(MaterialFeature::AOMap);
    break;
  case MaterialTextureType::Overlay:
    m_feature_flags.set(MaterialFeature::OverlayMap);
    break;
  case MaterialTextureType::Emissive:
    m_feature_flags.set(MaterialFeature::EmissiveMap);
    break;
  default:
    break;
  }

  m_constants.feature_flags = m_feature_flags.value();
}
