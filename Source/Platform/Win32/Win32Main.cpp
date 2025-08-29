#include <windows.h>
#include <Shlwapi.h>

#include <ThirdParty/imgui/imgui.h>
#include <ThirdParty/imgui/imgui_impl_win32.h>
#include <ThirdParty/imgui/imgui_impl_dx12.h>

#include <Platform/PlatformContext.h>
#include <CoreDefs.h>
#include <MathLib.h>
#include <Platform/DX12/DX12.h>
#include <Shaders/Shared.h>
#include <Geometry.h>
#include <Rendering.h>
#include <Platform/Input.h>
#include <Platform/Platform.h>
#include <Utility.h>

#include <Log.h>

#if ZV_COMPILER_CL
#pragma warning(disable: 4100)  // unreferenced parameter
// #pragma warning(disable: 4505)  // unreferenced function with internal linkage
#pragma warning(disable: 4201)  // nonstandard extension used: nameless struct/union
#endif

static constexpr Vector3 g_forward = {0.0f, 0.0f, 1.0f};
static constexpr Vector3 g_up = {0.0f, 1.0f, 0.0f};
static constexpr Vector3 g_right = {1.0f, 0.0f, 0.0f};

static Camera* g_camera = nullptr;

// TODO: WE ARE LEFT HANDED AND CW ORDER!!!

inline s32 round_f32_to_s32(f32 value)
{
    s32 result = (s32)roundf(value);
    return result;
}

int CALLBACK WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow)
{
  // Set the working directory to the path of the executable.
  WCHAR path[MAX_PATH];
  HMODULE hModule = GetModuleHandleW(NULL);
  if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
  {
    PathRemoveFileSpecW(path);
    SetCurrentDirectoryW(path);
  }

  ZV::Log::initialize();

  // TODO: Remove DirectX Math library
  // Check for DirectX Math library support.
  if (!DirectX::XMVerifyCPUSupport())
  {
    MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
    return false;
  }

  Assets::initialize();
  const u32 thread_count = win32_get_cpu_core_count() - 1;
  Platform::initialize(Platform::CreationInfo{ hInstance, L"ZEngine", 1280, 720, thread_count });
  
  ZV::Input::initialize();

  ZV::Input::set_mouse_sensitivity(0.005f);
  ZV::Input::set_camera_speed(6.0f);

  // auto geometry = create_triangle();
  // auto geometry = create_quad();
  auto geometry = create_plane(2.0f, 2.0f, 8, 8);
  // auto geometry = create_box();
  auto geometry2 = create_sphere(1.0f, 32, 64);
  // auto geometry2 = create_icosphere(1.0f, 5);
  // auto geometry = create_cylinder();
  // auto geometry = create_capsule();

  Vector3 camera_position{};
  Quaternion camera_rotation{};

  Renderer* renderer = Platform::get_renderer();

  // Setup Renderer
  {
    g_camera = renderer->create_camera();
    renderer->set_active_camera(g_camera);

    g_camera->get_transform(camera_position, camera_rotation);

    Vector3 hemispheric_light_color = {0.025f, 0.025f, 0.025f};
    renderer->set_hemispheric_light_color(hemispheric_light_color);

    GlobalLight* global_light = renderer->create_global_light();
    global_light->direction = {0.0f, -1.0f, 0.0f, 1.0f};
    global_light->intensity = 3.0f;
    global_light->light_color = {1.0f, 1.0f, 1.0f};
  
    // PunctualLight* point_light = renderer->create_punctual_light();
    // point_light->position = {-0.5f, 0.5f, -0.5f};
    // point_light->intensity = 2.0f;
    // point_light->light_color = {1.0f, 1.0f, 1.0f};
    // point_light->set_inv_sqr_att_radius(1.5f);
    // point_light->type = PunctualLightType::Point;
  
    // PunctualLight* spot_light = renderer->create_punctual_light();
    // spot_light->position = {0.5f, 0.5f, 0.5f};
    // spot_light->intensity = 2.0f;
    // spot_light->light_color = {1.0f, 1.0f, 1.0f};
    // spot_light->set_inv_sqr_att_radius(1.5f);
    // spot_light->type = PunctualLightType::Spot;
    // spot_light->direction = {0.0f, -1.0f, 0.0f};
    // spot_light->set_angle_scale_and_offset(30.0f * ZV_DEG_TO_RAD, 45.0f * ZV_DEG_TO_RAD);

    Vector4 clear_color = {0.025f, 0.025f, 0.025f, 1.0f};
    renderer->set_clear_color(clear_color);

    DebugPrimitive* debug_primitive_grid = renderer->create_debug_primitive(geometry);
    debug_primitive_grid->m_material_info.m_albedo_texture_id = AssetId("grid_albedo");
    debug_primitive_grid->m_material_info.m_normal_texture_id = AssetId("grid_normal");
    debug_primitive_grid->m_material_info.m_metallic_roughness_texture_id = AssetId("grid_roughness");
    debug_primitive_grid->m_material_info.m_overlay_texture_id = AssetId("grid_overlay");
    debug_primitive_grid->m_material_info.m_channel_packing = ChannelPacking::Roughness;
    debug_primitive_grid->m_material_info.m_albedo_color = {1.0f, 0.533f, 0.153f};
    debug_primitive_grid->m_world_matrix = Matrix::CreateTranslation(0.0f, 0.0f, -2.5f);
    renderer->push_debug_primitive(debug_primitive_grid);

    DebugPrimitive* debug_primitive_grid2 = renderer->create_debug_primitive(geometry);
    debug_primitive_grid2->m_material_info.m_albedo_texture_id = AssetId("grid_albedo");
    debug_primitive_grid2->m_material_info.m_normal_texture_id = AssetId("grid_normal");
    debug_primitive_grid2->m_material_info.m_metallic_roughness_texture_id = AssetId("grid_roughness");
    debug_primitive_grid2->m_material_info.m_overlay_texture_id = AssetId("grid_overlay");
    debug_primitive_grid2->m_material_info.m_channel_packing = ChannelPacking::Roughness;
    debug_primitive_grid2->m_material_info.m_albedo_color = { 0.145f, 0.631f, 1.0f };
    debug_primitive_grid2->m_world_matrix = Matrix::CreateTranslation(0.0f, 0.0f, -5.0f);
    renderer->push_debug_primitive(debug_primitive_grid2);

    DebugPrimitive* debug_primitive_mat_probe_brick_wall_low = renderer->create_debug_primitive(geometry2);
    debug_primitive_mat_probe_brick_wall_low->m_material_info.m_albedo_texture_id = AssetId("brick_wall_low_albedo");
    debug_primitive_mat_probe_brick_wall_low->m_material_info.m_normal_texture_id = AssetId("brick_wall_low_normal");
    debug_primitive_mat_probe_brick_wall_low->m_world_matrix = Matrix::CreateTranslation(-2.5f, 0.0f, -2.5f);
    renderer->push_debug_primitive(debug_primitive_mat_probe_brick_wall_low);

    DebugPrimitive* debug_primitive_mat_probe_brick_wall = renderer->create_debug_primitive(geometry2);
    debug_primitive_mat_probe_brick_wall->m_material_info.m_albedo_texture_id = AssetId("brick_wall_albedo");
    debug_primitive_mat_probe_brick_wall->m_material_info.m_normal_texture_id = AssetId("brick_wall_normal");
    debug_primitive_mat_probe_brick_wall->m_material_info.m_metallic_roughness_texture_id = AssetId("brick_wall_roughness");
    debug_primitive_mat_probe_brick_wall->m_material_info.m_ao_texture_id = AssetId("brick_wall_ao");
    debug_primitive_mat_probe_brick_wall->m_material_info.m_specular_texture_id = AssetId("brick_wall_specular");
    debug_primitive_mat_probe_brick_wall->m_material_info.m_channel_packing = ChannelPacking::Roughness;
    debug_primitive_mat_probe_brick_wall->m_world_matrix = Matrix::CreateTranslation(2.5f, 0.0f, -2.5f);
    renderer->push_debug_primitive(debug_primitive_mat_probe_brick_wall);

    DebugPrimitive* debug_primitive_mat_probe_metal_sheet = renderer->create_debug_primitive(geometry2);
    debug_primitive_mat_probe_metal_sheet->m_material_info.m_albedo_texture_id = AssetId("metal_sheet_albedo");
    debug_primitive_mat_probe_metal_sheet->m_material_info.m_normal_texture_id = AssetId("metal_sheet_normal");
    debug_primitive_mat_probe_metal_sheet->m_material_info.m_metallic_roughness_texture_id = AssetId("metal_sheet_metalRoughness");
    debug_primitive_mat_probe_metal_sheet->m_material_info.m_specular_texture_id = AssetId("metal_sheet_specular");
    debug_primitive_mat_probe_metal_sheet->m_material_info.m_ao_texture_id = AssetId("metal_sheet_ao");
    debug_primitive_mat_probe_metal_sheet->m_material_info.m_channel_packing = ChannelPacking::Roughness | ChannelPacking::Metalness;
    debug_primitive_mat_probe_metal_sheet->m_world_matrix = Matrix::CreateTranslation(-2.5f, 0.0f, 0.0f);
    renderer->push_debug_primitive(debug_primitive_mat_probe_metal_sheet);

    DebugPrimitive* debug_primitive_mat_probe_planks = renderer->create_debug_primitive(geometry2);
    debug_primitive_mat_probe_planks->m_material_info.m_albedo_texture_id = AssetId("planks_albedo");
    debug_primitive_mat_probe_planks->m_material_info.m_normal_texture_id = AssetId("planks_normal");
    debug_primitive_mat_probe_planks->m_material_info.m_metallic_roughness_texture_id = AssetId("planks_roughness");
    debug_primitive_mat_probe_planks->m_material_info.m_specular_texture_id = AssetId("planks_specular");
    debug_primitive_mat_probe_planks->m_material_info.m_ao_texture_id = AssetId("planks_ao");
    debug_primitive_mat_probe_planks->m_material_info.m_channel_packing = ChannelPacking::Roughness;
    debug_primitive_mat_probe_planks->m_world_matrix = Matrix::CreateTranslation(2.5f, 0.0f, 0.0f);
    renderer->push_debug_primitive(debug_primitive_mat_probe_planks);

    DebugPrimitive* debug_primitive_mat_probe_tiles = renderer->create_debug_primitive(geometry2);
    debug_primitive_mat_probe_tiles->m_material_info.m_albedo_texture_id = AssetId("tiles_albedo");
    debug_primitive_mat_probe_tiles->m_material_info.m_normal_texture_id = AssetId("tiles_normal");
    debug_primitive_mat_probe_tiles->m_material_info.m_metallic_roughness_texture_id = AssetId("tiles_roughness");
    debug_primitive_mat_probe_tiles->m_material_info.m_specular_texture_id = AssetId("tiles_specular");
    debug_primitive_mat_probe_tiles->m_material_info.m_ao_texture_id = AssetId("tiles_ao");
    debug_primitive_mat_probe_tiles->m_material_info.m_channel_packing = ChannelPacking::Roughness;
    debug_primitive_mat_probe_tiles->m_world_matrix = Matrix::CreateTranslation(-2.5f, 0.0f, 2.5f);
    renderer->push_debug_primitive(debug_primitive_mat_probe_tiles);

    DebugPrimitive* debug_primitive_mat_probe_wood = renderer->create_debug_primitive(geometry2);
    debug_primitive_mat_probe_wood->m_material_info.m_albedo_texture_id = AssetId("wood_albedo");
    debug_primitive_mat_probe_wood->m_material_info.m_normal_texture_id = AssetId("wood_normal");
    debug_primitive_mat_probe_wood->m_material_info.m_metallic_roughness_texture_id = AssetId("wood_roughness");
    debug_primitive_mat_probe_wood->m_material_info.m_specular_texture_id = AssetId("wood_specular");
    debug_primitive_mat_probe_wood->m_material_info.m_ao_texture_id = AssetId("wood_ao");
    debug_primitive_mat_probe_wood->m_material_info.m_channel_packing = ChannelPacking::Roughness;
    debug_primitive_mat_probe_wood->m_world_matrix = Matrix::CreateTranslation(2.5f, 0.0f, 2.5f);
    renderer->push_debug_primitive(debug_primitive_mat_probe_wood);

    renderer->push_model("DamagedHelmet", Matrix::CreateTranslation(0.0f, 0.5f, 0.0f));

    auto world_matrix_sponza = Matrix::CreateRotationX(ZV_PI) * Matrix::CreateRotationY(ZV_PI / 2.0f) *  Matrix::CreateTranslation(0.0f, -16.0f, 32.0f);
    renderer->push_model("Sponza", world_matrix_sponza);
  }

  LARGE_INTEGER last_counter = win32_get_wall_clock();
  LARGE_INTEGER flip_wall_clock = win32_get_wall_clock();

  Platform::window_toggle_fullscreen();

  HWND window_handle = Platform::window_get_handle();
  
  HDC renderer_dc = GetDC(window_handle);
  s32 monitor_refresh_hz = 60;
  s32 win32_refresh_rate = GetDeviceCaps(renderer_dc, VREFRESH);
  if (win32_refresh_rate > 1)
  {
    monitor_refresh_hz = win32_refresh_rate;
  }
  f32 game_update_hz = (f32)(monitor_refresh_hz);

  Platform::app_set_running(true);

  // ::ShowWindow(window_handle, SW_SHOWMAXIMIZED);
  ::ShowWindow(window_handle, SW_SHOW);

  u32 expected_frames_per_update = 1;
  f32 target_seconds_per_frame = (f32)expected_frames_per_update / (f32)game_update_hz;

  while (Platform::app_is_running() && !ZV::Input::is_quit_requested())
  {
    ZV::Input::update();

    // We need to call this early, so calls to push_model by game code are not overwritten by the renderer
    renderer->process_previous_frame_loads();
    
    // TODO: Update Game
    if (!Platform::app_is_paused())
    {
      static u64 frame_count = 0;
      static f64 total_time = 0.0;
      
      total_time += target_seconds_per_frame; //e.ElapsedTime;
      frame_count++;

      if (total_time > 1.0)
      {
          f64 fps = frame_count / total_time;

          char buffer[512];
          sprintf_s(buffer, "FPS: %f\n", fps);
          OutputDebugStringA(buffer);

          frame_count = 0;
          total_time = 0.0;
      }

      // Camera movement
      {
        bool mouse_move_button_pressed = ZV::Input::is_mouse_button_down(MouseButtonWin32::Right) || ZV::Input::is_mouse_button_down(MouseButtonWin32::Middle);

        // Uncapture mouse
        if (!mouse_move_button_pressed)
        {
          ZV::Input::set_mouse_captured(false);
        }
        // Handle camera movement
        else
        {
          if (!ZV::Input::is_mouse_captured())
          {
            ZV::Input::set_mouse_captured(true);
          }
          else
          {
              const Vector2 mouse_delta = ZV::Input::get_mouse_delta();

              // Camera rotation
              if (ZV::Input::is_mouse_button_down(MouseButtonWin32::Right))
              {
                  // Flip yaw when upside-down
                  static float yaw_sign = 1.0f; // +1 = normal, -1 = flipped
                  // How "tilted" are we? Compare camera-up to world-up.
                  const Vector3 cam_up = Vector3::Transform(g_up, camera_rotation);
                  const float up_dot = cam_up.Dot(g_up); // +1 upright, -1 upside-down

                  // Hysteresis thresholds (avoid rapid flip-flop near the pole)
                  constexpr float hi = 0.25f;  // ~cos(75°)
                  constexpr float lo = -0.25f; // ~cos(105°)
                  if (yaw_sign > 0.0f && up_dot < lo) yaw_sign = -1.0f;
                  else if (yaw_sign < 0.0f && up_dot > hi) yaw_sign = 1.0f;

                  const f32 yaw = (mouse_delta.x * ZV::Input::get_mouse_sensitivity()) * yaw_sign;
                  const f32 pitch = -mouse_delta.y * ZV::Input::get_mouse_sensitivity();

                  const Quaternion yaw_rotation = Quaternion::CreateFromAxisAngle(g_up, yaw);
                  const Quaternion pitch_rotation = Quaternion::CreateFromAxisAngle(g_right, pitch);

                  camera_rotation = pitch_rotation * camera_rotation * yaw_rotation;
                  camera_rotation.Normalize();
              }
              // Camera panning
              else  // Middle mouse button pressed
              {
                  camera_position += Vector3::Transform(g_right, camera_rotation) * mouse_delta.x * ZV::Input::get_camera_speed() * target_seconds_per_frame;
                  camera_position += Vector3::Transform(g_up, camera_rotation) * mouse_delta.y * ZV::Input::get_camera_speed() * target_seconds_per_frame;
              }
          }

          // Camera wasd movement
          Vector2 movement{ 0.0f, 0.0f };
          movement.x += ZV::Input::is_key_down(KeyboardKeyWin32::A) || ZV::Input::is_key_down(KeyboardKeyWin32::ArrowLeft) ? -1.0f : 0.0f;
          movement.x += ZV::Input::is_key_down(KeyboardKeyWin32::D) || ZV::Input::is_key_down(KeyboardKeyWin32::ArrowRight) ? 1.0f : 0.0f;
          movement.y += ZV::Input::is_key_down(KeyboardKeyWin32::W) || ZV::Input::is_key_down(KeyboardKeyWin32::ArrowUp) ? 1.0f : 0.0f;
          movement.y += ZV::Input::is_key_down(KeyboardKeyWin32::S) || ZV::Input::is_key_down(KeyboardKeyWin32::ArrowDown) ? -1.0f : 0.0f;
          movement.Normalize();

          camera_position += Vector3::Transform(g_forward, camera_rotation) * movement.y * ZV::Input::get_camera_speed() * target_seconds_per_frame;
          camera_position += Vector3::Transform(g_right, camera_rotation) * movement.x * ZV::Input::get_camera_speed() * target_seconds_per_frame;
        }
      }
    }

    // TODO: Update Audio
    if (!Platform::app_is_paused())
    {
    }

    // Update imgui
    {
      renderer->begin_frame_imgui();

      ImGui::Begin("Debug Settings");

      ImGui::Text(ZV::format("FPS: {}", 1.0 / target_seconds_per_frame).c_str());
      ImGui::Text(ZV::format("Client size: {}, {}", Platform::window_get_client_width(), Platform::window_get_client_height()).c_str());

      auto vm = g_camera->m_view_matrix;
      auto pm = g_camera->m_projection_matrix;

      ImGui::Text("Camera");
      ImGui::Text(ZV::format("Position: {}, {}, {}", camera_position.x, camera_position.y, camera_position.z).c_str());
      ImGui::Text(ZV::format("Position(M): {}, {}, {}", g_camera->m_world_matrix._41, g_camera->m_world_matrix._42, g_camera->m_world_matrix._43).c_str());
      ImGui::Text("Camera View Matrix");
      ImGui::Text(ZV::format("{}, {}, {}, {}", vm._11, vm._12, vm._13, vm._14).c_str());
      ImGui::Text(ZV::format("{}, {}, {}, {}", vm._21, vm._22, vm._23, vm._24).c_str());
      ImGui::Text(ZV::format("{}, {}, {}, {}", vm._31, vm._32, vm._33, vm._34).c_str());
      ImGui::Text(ZV::format("{}, {}, {}, {}", vm._41, vm._42, vm._43, vm._44).c_str());
      ImGui::Text("Camera Projection Matrix");
      ImGui::Text(ZV::format("{}, {}, {}, {}", pm._11, pm._12, pm._13, pm._14).c_str());
      ImGui::Text(ZV::format("{}, {}, {}, {}", pm._21, pm._22, pm._23, pm._24).c_str());
      ImGui::Text(ZV::format("{}, {}, {}, {}", pm._31, pm._32, pm._33, pm._34).c_str());
      ImGui::Text(ZV::format("{}, {}, {}, {}", pm._41, pm._42, pm._43, pm._44).c_str());

      ImGui::Text("Input");
      ImGui::Text(ZV::format("Mouse Position: {}, {}", ZV::Input::get_mouse_position().x, ZV::Input::get_mouse_position().y).c_str());
      ImGui::Text(ZV::format("Mouse Delta: {}, {}", ZV::Input::get_mouse_delta().x, ZV::Input::get_mouse_delta().y).c_str());
      ImGui::Text(ZV::format("Mouse Right: {}", ZV::Input::is_mouse_button_down(MouseButtonWin32::Right) ? "pressed" : "released").c_str());
      ImGui::Text(ZV::format("Mouse Left: {}", ZV::Input::is_mouse_button_down(MouseButtonWin32::Left) ? "pressed" : "released").c_str());
      ImGui::Text(ZV::format("Mouse Middle: {}", ZV::Input::is_mouse_button_down(MouseButtonWin32::Middle) ? "pressed" : "released").c_str());
      ImGui::Text(ZV::format("Mouse X1: {}", ZV::Input::is_mouse_button_down(MouseButtonWin32::Extended1) ? "pressed" : "released").c_str());
      ImGui::Text(ZV::format("Mouse X2: {}", ZV::Input::is_mouse_button_down(MouseButtonWin32::Extended2) ? "pressed" : "released").c_str());
      ImGui::Text(ZV::format("W: {}", ZV::Input::is_key_down(KeyboardKeyWin32::W) ? "pressed" : "released").c_str());
      ImGui::Text(ZV::format("A: {}", ZV::Input::is_key_down(KeyboardKeyWin32::A) ? "pressed" : "released").c_str());
      ImGui::Text(ZV::format("S: {}", ZV::Input::is_key_down(KeyboardKeyWin32::S) ? "pressed" : "released").c_str());
      ImGui::Text(ZV::format("D: {}", ZV::Input::is_key_down(KeyboardKeyWin32::D) ? "pressed" : "released").c_str());

      ImGui::End();

      renderer->end_frame_imgui();
    }

    // Update renderer data
    {
      g_camera->update_projection_matrix(
        3.14159f / 4.0f, 
        (f32)Platform::window_get_client_width() / (f32)Platform::window_get_client_height()
      );
      g_camera->update_view_matrix(
        camera_position, 
        camera_position + Vector3::Transform(g_forward, camera_rotation), 
        Vector3::Transform(g_up, camera_rotation)
      );
      
      renderer->update_active_camera();
    }

    // Frame display
    {
      // TODO: Handle this within Application
      renderer->set_client_size(Platform::window_get_client_width(), Platform::window_get_client_height());
      renderer->render();
    }

    // End Loop
    flip_wall_clock = win32_get_wall_clock();

    LARGE_INTEGER end_counter = win32_get_wall_clock();
    f32 measured_seconds_per_frame = win32_get_seconds_elapsed(last_counter, end_counter, Platform::app_get_perf_count_frequency());
    f32 exact_target_frames_per_update = measured_seconds_per_frame * (f32)monitor_refresh_hz;
    u32 new_expected_frames_per_update = round_f32_to_s32(exact_target_frames_per_update);
    expected_frames_per_update = new_expected_frames_per_update;

    target_seconds_per_frame = measured_seconds_per_frame;

    // FRAME_MARKER(measured_seconds_per_frame);
    last_counter = end_counter;
  }

  ZV::Input::shutdown();
  Assets::shutdown();
  ZV::Log::shutdown();

  return 0;
}
