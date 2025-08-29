#include <Platform/Win32/Win32Platform.h>

#include <Platform/Platform.h>
#include <Rendering.h>
#include <Platform/DX12/DX12.h>
#include <Platform/Input.h>

#include <ThirdParty/imgui/imgui.h>
#include <ThirdParty/imgui/imgui_impl_win32.h>
#include <ThirdParty/imgui/imgui_impl_dx12.h>

#if ZV_COMPILER_CL
#pragma warning(disable: 4100)  // unreferenced parameter
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


namespace
{
    void cat_strings(
        size_t source_a_count, char* source_a,
        size_t source_b_count, char* source_b,
        size_t dest_count, char* dest) // internal
    {
        zv_assert_msg(dest_count > source_a_count + source_b_count, "Destination buffer is too small!");
    
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

    int string_length(char* string)
    {
        int count = 0;
        while(*string++)
        {
            ++count;
        }
        return(count);
    }

    LRESULT CALLBACK win32_main_window_callback(
        HWND window,
        UINT message,
        WPARAM w_param,
        LPARAM l_param)  // internal
    {
        LRESULT result = 0;
    
        if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param))
        {
            return result;
        }
    
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
                Platform::app_set_running(false);
                break;
            }
            case WM_DESTROY:
            {
                Platform::app_set_running(false);
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
                ::GetClientRect(window, &client_rect);
        
                int width = client_rect.right - client_rect.left;
                int height = client_rect.bottom - client_rect.top;
        
                Renderer* renderer = Platform::get_renderer();
                DX12State* dx12_state = renderer->get_dx12_state();
        
                // TODO: Move this to Application
                if (Platform::window_resize(width, height) && dx12_state)
                {
                    dx12_state->resize(width, height);
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
            zv_assert_msg(false, "Keyboard input came in through a non-dispatch message!");
            break;
            }
            // case WM_MOUSEMOVE:
            // {
            //     POINT mouse_p;
            //     GetCursorPos(&mouse_p);
            //     ScreenToClient(g_window_handle, &mouse_p);
            //     g_mouse_delta.x = (f32)mouse_p.x;
            //     g_mouse_delta.y = (f32)mouse_p.y;
            //     SetCursorPos((s32)(g_client_width * 0.5f), (s32)(g_client_height * 0.5f));
            //     break;
            // }
            default:
            {
                result = DefWindowProcW(window, message, w_param, l_param);
                break;
            }
        }
    
        return result;
    }

    Win32Window win32_create_window(HINSTANCE instance, const wchar_t* window_title, uint32_t width, uint32_t height)
    {
        Win32Window window{};

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
        zv_assert_msg(atom > 0, "Failed to register window class!");

        int screen_width = ::GetSystemMetrics(SM_CXSCREEN);
        int screen_height = ::GetSystemMetrics(SM_CYSCREEN);

        RECT window_rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        ::AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

        int window_width = window_rect.right - window_rect.left;
        int window_height = window_rect.bottom - window_rect.top;

        // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
        int window_x = ZV::min(0, (screen_width - window_width) / 2);
        int window_y = ZV::max(0, (screen_height - window_height) / 2);

        window.m_window_handle = ::CreateWindowExW(
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

        zv_assert_msg(window.m_window_handle != nullptr, "Failed to create window!");

        // Initialize the global window rect variable.
        ::GetWindowRect(window.m_window_handle, &window.m_window_rect);

        return window;
    }

    void win32_get_executable_file_name(Win32State* state)
    {
        /*DWORD size_of_filename = */GetModuleFileNameA(0, state->m_exe_file_name, sizeof(state->m_exe_file_name));
        state->m_one_past_last_exe_files_name_slash = state->m_exe_file_name;
        for(char *scan = state->m_exe_file_name; *scan; ++scan)
        {
            if(*scan == '\\')
            {
                state->m_one_past_last_exe_files_name_slash = scan + 1;
            }
        }
    }

    void win32_build_executable_path_file_name(Win32State* state, char* file_name, int dest_count, char* dest)
    {
        cat_strings(state->m_one_past_last_exe_files_name_slash - state->m_exe_file_name, state->m_exe_file_name, string_length(file_name), file_name,  dest_count, dest);
    }
}

void win32_create_state(Win32State* state, HINSTANCE instance, const wchar_t* window_title, u32 width, u32 height, u32 thread_count)
{
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    state->m_perf_count_frequency = perf_count_frequency_result.QuadPart;

    win32_get_executable_file_name(state);
    win32_build_executable_path_file_name(state, "data.txt", sizeof(state->m_exe_file_name), state->m_exe_file_name);

    state->m_window = win32_create_window(instance, window_title, width, height);
    state->m_client_width = width;
    state->m_client_height = height;

    if (thread_count > 0)
    {
        win32_create_job_queue(&state->m_high_priority_queue, thread_count);
        win32_create_job_queue(&state->m_low_priority_queue, thread_count);
    }
}

void win32_process_pending_messages(InputState* input_state)
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
                input_state->m_quit_requested = true;
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

                if (was_down != is_down)
                {
                    win32_process_digital_input_message(&input_state->m_keyboard.m_keys[vk_code], is_down);
                    
                    if (vk_code == VK_ESCAPE)
                    {
                      input_state->m_quit_requested = true;
                    }

                    bool alt_key_was_down = (message.lParam & (1 << 29));

                    if (is_down)
                    {
                        if ((vk_code == VK_F4) && alt_key_was_down)
                        {
                            input_state->m_quit_requested = true;
                        }
                        else if ((vk_code == VK_RETURN) && alt_key_was_down)
                        {
                            if (message.hwnd == Platform::window_get_handle())
                            {
                                Platform::window_toggle_fullscreen();
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

void win32_toggle_fullscreen(Win32Window* window)
{
    // This follows Raymond Chen's prescription
    // for fullscreen toggling, see:
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx

    DWORD style = GetWindowLong(window->m_window_handle, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO monitor_info = {sizeof(monitor_info)};
        if(GetWindowPlacement(window->m_window_handle, &window->m_prev_window_position) &&
           GetMonitorInfo(MonitorFromWindow(window->m_window_handle, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
        {
            SetWindowLong(window->m_window_handle, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window->m_window_handle, HWND_TOP,
                         monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                         monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                         monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(window->m_window_handle, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window->m_window_handle, &window->m_prev_window_position);
        SetWindowPos(window->m_window_handle, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

void win32_process_digital_input_message(DigitalInput* new_state, bool is_down)
{
    if (new_state->m_is_down != is_down)
    {
        new_state->m_is_down = is_down;
        ++new_state->m_num_transitions;
    }
}

void win32_set_mouse_captured(bool is_captured)
{
    if (!is_captured)
    {
        ShowCursor(true);
    }
    else
    {
        ShowCursor(false);
        SetCursor(NULL);

        s32 x_client = (s32)(Platform::window_get_client_width() * 0.5f);
        s32 y_client = (s32)(Platform::window_get_client_height() * 0.5f);

        win32_set_cursor_position(x_client, y_client);
    }
}

void win32_set_cursor_position(s32 x_client, s32 y_client)
{
    HWND window_handle = Platform::window_get_handle();

    POINT pos = { x_client, y_client };
    ClientToScreen(window_handle, &pos);
    SetCursorPos(pos.x, pos.y - 1);
}
