/////////////
// Globals //
/////////////

static s64 g_perf_count_frequency;  // global

static char g_exe_file_name[MAX_PATH];              // win32_state
static char* g_one_past_last_exe_files_name_slash;  // win32_state

static bool g_running = false;  // global
static bool g_pause = false;  // global

static HWND g_window_handle;
static RECT g_window_rect;
static WINDOWPLACEMENT g_prev_window_position = {sizeof(g_prev_window_position)};
 
static u32 g_client_width = 1280;  // TODO: What is this?
static u32 g_client_height = 720;  // TODO: What is this?


/////////////
/// Win32 ///
/////////////

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))


inline s32 round_f32_to_s32(f32 value)
{
    s32 result = (s32)roundf(value);
    return result;
}

static void cat_strings(
    size_t source_a_count, char* source_a,
    size_t source_b_count, char* source_b,
    size_t dest_count, char* dest) // internal
{
    // TODO: Dest bounds checking!

    for(int index = 0;
        index < source_a_count;
        ++index)
    {
        *dest++ = *source_a++;
    }

    for(int Index = 0;
        Index < source_b_count;
        ++Index)
    {
        *dest++ = *source_b++;
    }

    *dest++ = 0;
}

static int string_length(char* string) // internal
{
    int count = 0;
    while(*string++)
    {
        ++count;
    }
    return(count);
}

typedef struct game_button_state
{
    s32 half_transition_count;
    bool ended_down;
} game_button_state;

typedef struct game_controller_input
{
    bool is_connected;
    bool is_analog;    
    s32 stick_average_x;
    s32 stick_average_y;
    
    union
    {
        game_button_state buttons[12];
        struct
        {
            game_button_state move_up;
            game_button_state move_down;
            game_button_state move_left;
            game_button_state move_right;
            
            game_button_state action_up;
            game_button_state action_down;
            game_button_state action_left;
            game_button_state action_right;
            
            game_button_state left_shoulder;
            game_button_state right_shoulder;

            game_button_state back;
            game_button_state start;
        };
    };
} game_controller_input;

enum game_input_mouse_button
{
    PlatformMouseButton_Left,
    PlatformMouseButton_Middle,
    PlatformMouseButton_Right,
    PlatformMouseButton_Extended0,
    PlatformMouseButton_Extended1,

    PlatformMouseButton_Count,
};

typedef struct game_input
{
    f32 dt_for_frame;

    game_controller_input controllers[5];

    // Signals back to the platform layer
    bool quit_requested;

    // NOTE: For debugging only
    game_button_state mouse_buttons[PlatformMouseButton_Count];
    f32 mouse_x, mouse_y, mouse_z;
    bool shift_down, alt_down, control_down;
} game_input;

inline game_controller_input* get_controller(game_input* input, int unsigned controller_index)
{
  // TODO: Assert
  // Assert(ControllerIndex < ArrayCount(Input->Controllers));
  
  game_controller_input *result = &input->controllers[controller_index];
  return result;
}

bool win32_resize(u32 width, u32 height)
{
  if (g_client_width != width || g_client_height != height)
  {
    // Don't allow 0 size swap chain back buffers.
    g_client_width = ZV::max(1u, width );
    g_client_height = ZV::max( 1u, height);

    return true;
  }

  return false;
}

static LRESULT CALLBACK win32_main_window_callback(
  HWND window,
  UINT message,
  WPARAM w_param,
  LPARAM l_param)  // internal
{
    LRESULT result = 0;

    switch(message)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            (void)BeginPaint(window, &ps);
            EndPaint(window, &ps);
            break;
        }
        case WM_CLOSE:
        {
            g_running = false;
            break;
        }
        case WM_DESTROY:
        {
            g_running = false;
            break;
        }
        case WM_ACTIVATEAPP:
        {
          // if(WParam == TRUE)
          // {
          //     SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
          // }
          // else
          // {
          //     SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
          // }
          break;
        }
        case WM_SIZE:
        {
          RECT client_rect = {};
          ::GetClientRect(g_window_handle, &client_rect);

          int width = client_rect.right - client_rect.left;
          int height = client_rect.bottom - client_rect.top;

          if (win32_resize(width, height))
          {
              dx12_resize(width, height);
          }
          break;
        }
        // case WM_SETCURSOR:
        // {
        //     // if(DEBUGGlobalShowCursor)
        //     // {
        //     //     result = DefWindowProcA(window, message, w_param, l_param);
        //     // }
        //     // else
        //     // {
        //       SetCursor(0);
        //     // }
        //   break;
        // } 
        // TODO: Test this
        // The default window procedure will play a system notification sound 
        // when pressing the Alt+Enter keyboard combination if this message is 
        // not handled.
        // case WM_SYSCHAR:
        // {
        //   break;
        // }
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
          // TODO: assert
          // Assert(!"Keyboard input came in through a non-dispatch message!");
          break;
        }
        default:
        {
            result = DefWindowProcW(window, message, w_param, l_param);
            break;
        }
    }

    return result;
}

void win32_create_window(
  HINSTANCE instance,
  const wchar_t* window_title, 
  uint32_t width, 
  uint32_t height)
{
  // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
  // Using this awareness context allows the client area of the window 
  // to achieve 100% scaling while still allowing non-client window content to 
  // be rendered in a DPI sensitive fashion.
  SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  // Register a window class for creating our render window with.
  const wchar_t* window_class_name = L"DX12WindowClass";

  WNDCLASSEXW window_class = {};

  window_class.cbSize = sizeof(WNDCLASSEX);
  window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  window_class.lpfnWndProc = &win32_main_window_callback;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = instance;
  window_class.hIcon = ::LoadIcon(instance, NULL);
  window_class.hCursor = ::LoadCursor(NULL, IDC_ARROW);
  window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  window_class.lpszMenuName = NULL;
  window_class.lpszClassName = window_class_name;
  window_class.hIconSm = ::LoadIcon(instance, NULL);

  static ATOM atom = ::RegisterClassExW(&window_class);
  // TODO: assert
  // assert(atom > 0);

  int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
  int screen_height = ::GetSystemMetrics(SM_CYSCREEN);

  RECT window_rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
  ::AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

  int window_width = window_rect.right - window_rect.left;
  int window_height = window_rect.bottom - window_rect.top;

  // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
  int window_x = ZV::min(0, (screenWidth - window_width) / 2);
  int window_y = ZV::max(0, (screen_height - window_height) / 2);

  g_window_handle = ::CreateWindowExW(
    NULL,
    window_class_name,
    window_title,
    WS_OVERLAPPEDWINDOW,
    window_x,
    window_y,
    window_width,
    window_height,
    NULL,
    NULL,
    instance,
    nullptr
  );

  // TODO
  // assert(hWnd && "Failed to create window");

  // Initialize the global window rect variable.
  ::GetWindowRect(g_window_handle, &g_window_rect);
}

static void win32_toggle_fullscreen(HWND window)  // internal
{
    // This follows Raymond Chen's prescription
    // for fullscreen toggling, see:
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx

    DWORD style = GetWindowLong(window, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO monitor_info = {sizeof(monitor_info)};
        if(GetWindowPlacement(window, &g_prev_window_position) &&
           GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
        {
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, HWND_TOP,
                         monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                         monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                         monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &g_prev_window_position);
        SetWindowPos(window, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

static void win32_process_keyboard_message(game_button_state* new_state, bool is_down)  // internal
{
    if(new_state->ended_down != is_down)
    {
        new_state->ended_down = is_down;
        ++new_state->half_transition_count;
    }
}

static void win32_process_pending_messages(/*win32_state *State, */game_controller_input* keyboard_controller)  // internal
{
    MSG message;

    for(;;)
    {
        BOOL got_message = FALSE;
        
        {
            // TIMED_BLOCK("PeekMessage");
            got_message = PeekMessage(&message, 0, 0, 0, PM_REMOVE);
        }
        
        if(!got_message)
        {
            break;
        }
        
        switch(message.message)
        {
            case WM_QUIT:
            {
                g_running = false;
                break;
            }
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                u32 vk_code = (u32)message.wParam;

                // Since we are comparing WasDown to is_down,
                // we MUST use == and != to convert these bit tests to actual
                // 0 or 1 values.
                bool was_down = ((message.lParam & (1 << 30)) != 0);
                bool is_down = ((message.lParam & (1 << 31)) == 0);
                if(was_down != is_down)
                {
                    if(vk_code == 'W')
                    {
                        win32_process_keyboard_message(&keyboard_controller->move_up, is_down);
                    }
                    else if(vk_code == 'A')
                    {
                        win32_process_keyboard_message(&keyboard_controller->move_left, is_down);
                    }
                    else if(vk_code == 'S')
                    {
                        win32_process_keyboard_message(&keyboard_controller->move_down, is_down);
                    }
                    else if(vk_code == 'D')
                    {
                        win32_process_keyboard_message(&keyboard_controller->move_right, is_down);
                    }
                    else if(vk_code == 'Q')
                    {
                        win32_process_keyboard_message(&keyboard_controller->left_shoulder, is_down);
                    }
                    else if(vk_code == 'E')
                    {
                        win32_process_keyboard_message(&keyboard_controller->right_shoulder, is_down);
                    }
                    else if(vk_code == VK_UP)
                    {
                        win32_process_keyboard_message(&keyboard_controller->action_up, is_down);
                    }
                    else if(vk_code == VK_LEFT)
                    {
                        win32_process_keyboard_message(&keyboard_controller->action_left, is_down);
                    }
                    else if(vk_code == VK_DOWN)
                    {
                        win32_process_keyboard_message(&keyboard_controller->action_down, is_down);
                    }
                    else if(vk_code == VK_RIGHT)
                    {
                        win32_process_keyboard_message(&keyboard_controller->action_right, is_down);
                    }
                    else if(vk_code == VK_BACK)
                    {
                        win32_process_keyboard_message(&keyboard_controller->back, is_down);
                    }
                    else if(vk_code == VK_SPACE)
                    {
                        win32_process_keyboard_message(&keyboard_controller->start, is_down);
                    }
                    else if (vk_code == VK_ESCAPE)
                    {
                      g_running = false;
                    }
                    if(is_down)
                    {
                        s32 alt_key_was_down = (message.lParam & (1 << 29));
                        if((vk_code == VK_F4) && alt_key_was_down)
                        {
                            g_running = false;
                        }
                        if((vk_code == VK_RETURN) && alt_key_was_down)
                        {
                            if(message.hwnd)
                            {
                                win32_toggle_fullscreen(message.hwnd);
                            }
                        }
                    }
                }

                break;
            }
            default:
            {
                TranslateMessage(&message);
                DispatchMessageA(&message);
                break;
            }
        }
    }
}

inline LARGE_INTEGER win32_get_wall_clock(void)
{    
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return(result);
}

inline f32 win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    f32 result = ((f32)(start.QuadPart - end.QuadPart) /
                     (f32)g_perf_count_frequency);
    return(result);
}

static void win32_get_executable_file_name(void)  // internal
{
    /*DWORD size_of_filename = */GetModuleFileNameA(0, g_exe_file_name, sizeof(g_exe_file_name));
    g_one_past_last_exe_files_name_slash = g_exe_file_name;
    for(char *scan = g_exe_file_name; *scan; ++scan)
    {
        if(*scan == '\\')
        {
            g_one_past_last_exe_files_name_slash = scan + 1;
        }
    }
}

static void win32_build_executable_path_file_name(char* file_name, int dest_count, char* dest)  // internal
{
    cat_strings(g_one_past_last_exe_files_name_slash - g_exe_file_name, g_exe_file_name, string_length(file_name), file_name,  dest_count, dest);
}
