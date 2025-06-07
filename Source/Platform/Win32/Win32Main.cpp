#include <windows.h>

#include <MathUtility.h>
#include <PrimitiveTypes.h>
#include <Platform/PlatformContext.h>

#if ZV_COMPILER_CL
#pragma warning(disable: 4100)  // unreferenced parameter
#pragma warning(disable: 4505)  // unreferenced function with internal linkage
#pragma warning(disable: 4201)  // nonstandard extension used: nameless struct/union
#endif

#include <Platform/DX12/DX12.cpp>
#include <Platform/Win32/Win32Window.cpp>

int CALLBACK WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow)
{
  LARGE_INTEGER perf_count_frequency_result;
  QueryPerformanceFrequency(&perf_count_frequency_result);
  g_perf_count_frequency = perf_count_frequency_result.QuadPart;

  win32_create_window(hInstance, L"Hello World Window", g_client_width, g_client_height);

  bool use_warp = false;
  dx12_init(g_window_handle, g_client_width, g_client_height, use_warp);
  if (!dx12_cube_example_init(g_client_width, g_client_height))
  {
    // TODO: Error handling
    return 1;
  }

  game_input input[2] = {};
  game_input* new_input = &input[0];
  game_input* old_input = &input[1];

  LARGE_INTEGER last_counter = win32_get_wall_clock();
  LARGE_INTEGER flip_wall_clock = win32_get_wall_clock();

  // win32_toggle_fullscreen(g_window_handle);

  HDC renderer_dc = GetDC(g_window_handle);
  s32 monitor_refresh_hz = 60;
  s32 win32_refresh_rate = GetDeviceCaps(renderer_dc, VREFRESH);
  if (win32_refresh_rate > 1)
  {
    monitor_refresh_hz = win32_refresh_rate;
  }
  f32 game_update_hz = (f32)(monitor_refresh_hz);


  g_running = true;

  ::ShowWindow(g_window_handle, SW_SHOWMAXIMIZED);
  //::ShowWindow(g_window_handle, SW_SHOW);

  u32 expected_frames_per_update = 1;
  f32 target_seconds_per_frame = (f32)expected_frames_per_update / (f32)game_update_hz;

  while (g_running)
  {
    // Process Input

    // TODO: We can't zero everything because the up/down state will
    // be wrong!!!
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
    
    if (!g_pause)
    {
      // Get Mouse Position
      {
          POINT mouse_p;
          GetCursorPos(&mouse_p);
          ScreenToClient(g_window_handle, &mouse_p);
          new_input->mouse_x = (f32)mouse_p.x;
          new_input->mouse_y = (f32)((g_client_height - 1) - mouse_p.y);  // TODO: backbuffer height (1080)
          new_input->mouse_z = 0; // TODO: Support mousewheel?

          new_input->shift_down = (GetKeyState(VK_SHIFT) & (1 << 15));
          new_input->alt_down = (GetKeyState(VK_MENU) & (1 << 15));
          new_input->control_down = (GetKeyState(VK_CONTROL) & (1 << 15));
      }

      // Get Keyboard State
      {
        DWORD win_button_id[PlatformMouseButton_Count] =
        {
            VK_LBUTTON,
            VK_MBUTTON,
            VK_RBUTTON,
            VK_XBUTTON1,
            VK_XBUTTON2,
        };
        for(u32 button_index = 0;
            button_index < PlatformMouseButton_Count;
            ++button_index)
        {
            new_input->mouse_buttons[button_index] = old_input->mouse_buttons[button_index];
            new_input->mouse_buttons[button_index].half_transition_count = 0;
            win32_process_keyboard_message(&new_input->mouse_buttons[button_index],
                GetKeyState(win_button_id[button_index]) & (1 << 15));
        }
      }

      // Get Controller State
      {
        // TODO
      }
    }

    // TODO: Update Game
    if (!g_pause)
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

      // Update the model matrix.
      float angle = static_cast<float>(total_time * 90.0);
      const DirectX::XMVECTOR rotational_axis = DirectX::XMVectorSet(0, 1, 1, 0);
      g_model_matrix = DirectX::XMMatrixRotationAxis(rotational_axis, DirectX::XMConvertToRadians(angle));

      // Update the view matrix.
      const DirectX::XMVECTOR eye_position = DirectX::XMVectorSet(0, 0, -10, 1);
      const DirectX::XMVECTOR focus_point = DirectX::XMVectorSet(0, 0, 0, 1);
      const DirectX::XMVECTOR up_direction = DirectX::XMVectorSet(0, 1, 0, 0);
      g_view_matrix = DirectX::XMMatrixLookAtLH(eye_position, focus_point, up_direction);

      // Update the projection matrix.
      float aspect_ratio = g_client_width / static_cast<float>(g_client_height);
      g_projection_matrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(g_fov), aspect_ratio, 0.1f, 100.0f);
    }

    // TODO: Update Audio
    if (!g_pause)
    {
    }

    // Frame Display
    dx12_render_frame();

    // End Loop
    flip_wall_clock = win32_get_wall_clock();

    game_input *temp = new_input;
    new_input = old_input;
    old_input = temp;
    // TODO: Should I clear these here?

    LARGE_INTEGER end_counter = win32_get_wall_clock();
    f32 measured_seconds_per_frame = win32_get_seconds_elapsed(last_counter, end_counter);
    f32 exact_target_frames_per_update = measured_seconds_per_frame * (f32)monitor_refresh_hz;
    u32 new_expected_frames_per_update = round_f32_to_s32(exact_target_frames_per_update);
    expected_frames_per_update = new_expected_frames_per_update;

    target_seconds_per_frame = measured_seconds_per_frame;

    // FRAME_MARKER(measured_seconds_per_frame);
    last_counter = end_counter;
  }

  dx12_shutdown();
  
  return 0;
}


#if 0
///////////////////////////////
// Start: DirectX12 Tutorial //
///////////////////////////////

#include <shellapi.h> // For CommandLineToArgvW

// Window callback function.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

//  Argument     |   Description
// --------------------------------------------------------------------------------------------
// -w, --width   | Specify the width (in pixels) of the render window.
// -h, --height  | Specify the height (in pixels) of the render window.
// -warp, --warp | Use the Windows Advanced Rasterization Platform (WARP) for device creation.
void ParseCommandLineArguments()
{
  int argc;
  wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

  for (size_t i = 0; i < argc; ++i)
  {
    if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
    {
      g_ClientWidth = ::wcstol(argv[++i], nullptr, 10);
    }

    if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
    {
      g_ClientHeight = ::wcstol(argv[++i], nullptr, 10);
    }

    if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
    {
      g_UseWarp = true;
    }
  }

  // Free memory allocated by CommandLineToArgvW
  ::LocalFree(argv);
}

// #include <Shlwapi.h> // PathRemoveFileSpecW (deprecated)

#include <dxgidebug.h>  // ReportLiveObjects

void ReportLiveObjects()
{
    IDXGIDebug1* dxgiDebug;
    DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
    dxgiDebug->Release();
}

// From main
/*
{
  // Set the working directory to the path of the executable.
  WCHAR path[MAX_PATH];
  HMODULE hModule = GetModuleHandleW(NULL);
  if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
  {
    PathRemoveFileSpecW(path);
    SetCurrentDirectoryW(path);
  }

  // ...

  atexit(&ReportLiveObjects);

  return 0;
}
*/
/////////////////////////////
// End: DirectX12 Tutorial //
/////////////////////////////
#endif
