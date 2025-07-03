#include <windows.h>

#include <Platform/PlatformContext.h>
#include <CoreTypes.h>
#include <MathUtility.h>
#include <Platform/DX12/DX12.h>
#include <Shaders/Shared.h>
#include <LoadImage.h>

#if ZV_COMPILER_CL
#pragma warning(disable: 4100)  // unreferenced parameter
#pragma warning(disable: 4505)  // unreferenced function with internal linkage
#pragma warning(disable: 4201)  // nonstandard extension used: nameless struct/union
#endif

static UniquePtr<DX12State> g_dx12_state = nullptr;

static MeshVertex g_vertices[24] = {
  // Front face
  { {-1, -1,  1}, {0.0f, 1.0f} },
  { {-1,  1,  1}, {0.0f, 0.0f} },
  { { 1,  1,  1}, {1.0f, 0.0f} },
  { { 1, -1,  1}, {1.0f, 1.0f} },
  // Back face
  { { 1, -1, -1}, {0.0f, 1.0f} },
  { { 1,  1, -1}, {0.0f, 0.0f} },
  { {-1,  1, -1}, {1.0f, 0.0f} },
  { {-1, -1, -1}, {1.0f, 1.0f} },
  // Left face
  { {-1, -1, -1}, {0.0f, 1.0f} },
  { {-1,  1, -1}, {0.0f, 0.0f} },
  { {-1,  1,  1}, {1.0f, 0.0f} },
  { {-1, -1,  1}, {1.0f, 1.0f} },
  // Right face
  { { 1, -1,  1}, {0.0f, 1.0f} },
  { { 1,  1,  1}, {0.0f, 0.0f} },
  { { 1,  1, -1}, {1.0f, 0.0f} },
  { { 1, -1, -1}, {1.0f, 1.0f} },
  // Top face
  { {-1,  1,  1}, {0.0f, 1.0f} },
  { {-1,  1, -1}, {0.0f, 0.0f} },
  { { 1,  1, -1}, {1.0f, 0.0f} },
  { { 1,  1,  1}, {1.0f, 1.0f} },
  // Bottom face
  { {-1, -1, -1}, {0.0f, 1.0f} },
  { {-1, -1,  1}, {0.0f, 0.0f} },
  { { 1, -1,  1}, {1.0f, 0.0f} },
  { { 1, -1, -1}, {1.0f, 1.0f} },
};

static u16 g_indicies[36] =
{
  0,  1,  2,  0,  2,  3,    // Front
  4,  5,  6,  4,  6,  7,    // Back
  8,  9, 10,  8, 10, 11,    // Left
  12, 13, 14, 12, 14, 15,   // Right
  16, 17, 18, 16, 18, 19,   // Top
  20, 21, 22, 20, 22, 23    // Bottom
};

#include <Platform/Win32/Win32Window.cpp>

int CALLBACK WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow)
{
  win32_create_window(hInstance, L"Hello World Window", g_client_width, g_client_height);

  g_dx12_state = make_unique_ptr<DX12State>(g_window_handle, g_client_width, g_client_height);
  auto dx12_graphics_ctx = g_dx12_state->create_graphics_context();

  s32 image_width = 0;
  s32 image_height = 0;
  s32 image_channels = 0;
  u8* image_data = nullptr;
  load_image_rgba("Assets/Textures/brickwall.jpg", &image_width, &image_height, &image_channels, &image_data);

  RawTextureData texture_data{};
  texture_data.m_width = image_width;
  texture_data.m_height = image_height;
  texture_data.m_num_channels = 4;
  texture_data.m_data = image_data;
  UniquePtr<DX12TextureData> dx12_texture_data = g_dx12_state->create_texture_data(texture_data);
  free_image(image_data);

  auto dx12_upload_ctx = g_dx12_state->get_upload_context_for_current_frame();
  dx12_upload_ctx->record_texture_upload(dx12_texture_data.get());

  // Create vertex buffer
  DX12BufferResource::Desc vb_desc{};
  vb_desc.m_size = sizeof(g_vertices);
  vb_desc.m_stride = sizeof(MeshVertex);
  vb_desc.m_access = DX12ResourceAccess::GpuOnly;
  UniquePtr<DX12BufferResource> vertex_buffer = move_ptr(g_dx12_state->create_buffer_resource(vb_desc));
  dx12_graphics_ctx->add_barrier(vertex_buffer.get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
  dx12_upload_ctx->record_buffer_upload(vertex_buffer.get(), g_vertices, sizeof(g_vertices));

  // Create index buffer
  DX12BufferResource::Desc ib_desc{};
  ib_desc.m_size = sizeof(g_indicies);
  ib_desc.m_stride = sizeof(u16);
  ib_desc.m_access = DX12ResourceAccess::GpuOnly;
  UniquePtr<DX12BufferResource> index_buffer = move_ptr(g_dx12_state->create_buffer_resource(ib_desc));
  dx12_graphics_ctx->add_barrier(index_buffer.get(), D3D12_RESOURCE_STATE_INDEX_BUFFER);
  dx12_upload_ctx->record_buffer_upload(index_buffer.get(), g_indicies, sizeof(g_indicies));

  StaticArray<UniquePtr<DX12BufferResource>, k_num_frames_in_flight> per_object_constant_buffers;

  DX12BufferResource::Desc per_object_cb_desc{};
  per_object_cb_desc.m_size = sizeof(PerObjectConstants);
  per_object_cb_desc.m_access = DX12ResourceAccess::HostWritable;
  per_object_cb_desc.m_view_flags.set(DX12BufferViewFlags::CBV);

  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    per_object_constant_buffers[i] = g_dx12_state->create_buffer_resource(per_object_cb_desc);
  }

  PerObjectConstants object_constants;
  per_object_constant_buffers[0]->copy_data(&object_constants, sizeof(PerObjectConstants));

  DX12PipelineResourceSpace per_object_resource_space;
  per_object_resource_space.set_cbv(per_object_constant_buffers[0].get());
  per_object_resource_space.lock();

  UniquePtr<DX12BufferResource> per_pass_constants_buffer;

  DX12BufferResource::Desc per_pass_cb_desc{};
  per_pass_cb_desc.m_size = sizeof(PerPassConstants);
  per_pass_cb_desc.m_access = DX12ResourceAccess::HostWritable;
  per_pass_cb_desc.m_view_flags.set(DX12BufferViewFlags::CBV);

  per_pass_constants_buffer = g_dx12_state->create_buffer_resource(per_pass_cb_desc);

  f32 field_of_view = 3.14159f / 4.0f;
  f32 aspect_ratio = (f32)g_client_width / (f32)g_client_height;
  Vector3 camera_position = Vector3(-2.0f, 2.0f, 6.0f);  // Right-handed coordinate system

  PerPassConstants pass_constants;
  pass_constants.view_matrix = Matrix::CreateLookAt(camera_position, Vector3(0, 0, 0), Vector3(0, 1, 0));
  pass_constants.projection_matrix = Matrix::CreatePerspectiveFieldOfView(field_of_view, aspect_ratio, 0.01f, 1000.0f);
  pass_constants.camera_position = camera_position;

  per_pass_constants_buffer->copy_data(&pass_constants, sizeof(PerPassConstants));

  DX12PipelineResourceBinding diffuse_texture_binding{};
  diffuse_texture_binding.m_resource = dx12_texture_data->m_texture_resource.get();
  diffuse_texture_binding.m_binding_index = 0;

  DX12PipelineResourceSpace per_pass_resource_space;
  per_pass_resource_space.set_cbv(per_pass_constants_buffer.get());
  per_pass_resource_space.set_srv(diffuse_texture_binding);
  per_pass_resource_space.lock();
  
  DX12PipelineState::Desc pipeline_desc = get_default_pipeline_state_desc();
  pipeline_desc.m_vs_path = L"Shaders/cube_vs.cso";
  pipeline_desc.m_ps_path = L"Shaders/cube_ps.cso";
  pipeline_desc.m_render_target_desc.m_num_render_targets = 1;
  pipeline_desc.m_render_target_desc.m_render_target_formats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  pipeline_desc.m_render_target_desc.m_depth_stencil_format = DXGI_FORMAT_D32_FLOAT;
  pipeline_desc.m_spaces[DX12ResourceSpace::PerObjectSpace] = &per_object_resource_space;
  pipeline_desc.m_spaces[DX12ResourceSpace::PerPassSpace] = &per_pass_resource_space;
  pipeline_desc.m_input_layout.m_elements[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
  pipeline_desc.m_input_layout.m_elements[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
  pipeline_desc.m_input_layout.m_num_elements = 2;
  pipeline_desc.m_depth_stencil_desc.DepthEnable = true;
  pipeline_desc.m_depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

  auto cube_pipeline = g_dx12_state->create_graphics_pipeline(pipeline_desc);

  game_input input[2] = {};
  game_input* new_input = &input[0];
  game_input* old_input = &input[1];

  g_running = true;
  ::ShowWindow(g_window_handle, SW_SHOWMAXIMIZED);
  while (g_running)
  {
    // Process Input
    {
      game_controller_input *old_keyboard_controller = get_controller(old_input, 0);
      game_controller_input *new_keyboard_controller = get_controller(new_input, 0);
      *new_keyboard_controller = {};
      new_keyboard_controller->is_connected = true;
      for(int button_index = 0;
          button_index < ArrayCount(new_keyboard_controller->buttons);
          ++button_index)
      {
        new_keyboard_controller->buttons[button_index].ended_down =
        old_keyboard_controller->buttons[button_index].ended_down;
      }
      win32_process_pending_messages(new_keyboard_controller);
    }

    // Frame Display
    {
      g_dx12_state->begin_frame();

      DX12TextureResource* back_buffer = g_dx12_state->get_current_back_buffer();
      DX12TextureResource* depth_buffer = g_dx12_state->get_depth_stencil_buffer();

      dx12_graphics_ctx->reset();
      dx12_graphics_ctx->add_barrier(back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
      dx12_graphics_ctx->add_barrier(depth_buffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
      dx12_graphics_ctx->flush_barriers();

      f32 clear_color[4] = {0.05f, 0.05f, 0.05f, 1.0f};
      dx12_graphics_ctx->clear_render_target(back_buffer, clear_color);
      dx12_graphics_ctx->clear_depth_stencil_target(depth_buffer);

      static float rotation = 0.0f;
      rotation += 0.0001f;

      object_constants.world_matrix = Matrix::CreateRotationY(rotation);
      per_object_constant_buffers[g_dx12_state->get_frame_id()]->copy_data(&object_constants, sizeof(PerObjectConstants));
      per_object_resource_space.set_cbv(per_object_constant_buffers[g_dx12_state->get_frame_id()].get());

      dx12_graphics_ctx->set_pipeline_state(cube_pipeline.get());
      dx12_graphics_ctx->set_pipeline_resources(DX12ResourceSpace::PerObjectSpace, &per_object_resource_space);
      dx12_graphics_ctx->set_pipeline_resources(DX12ResourceSpace::PerPassSpace, &per_pass_resource_space);
      dx12_graphics_ctx->set_vertex_buffer(vertex_buffer.get());
      dx12_graphics_ctx->set_index_buffer(index_buffer.get());
      dx12_graphics_ctx->set_viewport_and_scissor(g_client_width, g_client_height);
      dx12_graphics_ctx->set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      dx12_graphics_ctx->draw_indexed(_countof(g_indicies));

      dx12_graphics_ctx->add_barrier(back_buffer, D3D12_RESOURCE_STATE_PRESENT);
      dx12_graphics_ctx->flush_barriers();

      g_dx12_state->submit_context(dx12_graphics_ctx.get());

      g_dx12_state->end_frame();
      g_dx12_state->present();
    }

    // End Loop
    {
      game_input *temp = new_input;
      new_input = old_input;
      old_input = temp;
    }
  }

  g_dx12_state->flush_queues();

  return 0;
}
