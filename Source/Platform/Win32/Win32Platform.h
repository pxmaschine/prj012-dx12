#pragma once

#include <Log.h>
#include <Windows.h>
#include <Platform/Jobs.h>

#include <CoreDefs.h>

struct DigitalInput;
struct InputState;

struct Win32Window
{
    HWND m_window_handle;
    RECT m_window_rect;
    WINDOWPLACEMENT m_prev_window_position = {sizeof(m_prev_window_position)};
};

struct Win32State
{
    s64 m_perf_count_frequency;  // global

    char m_exe_file_name[MAX_PATH];              // win32_state
    char* m_one_past_last_exe_files_name_slash;  // win32_state
    
    bool m_running = false;  // global
    bool m_pause = false;  // global
    
    Win32Window m_window;
     
    u32 m_client_width = 1280;
    u32 m_client_height = 720;

    JobQueue m_high_priority_queue;
    JobQueue m_low_priority_queue;
};

void win32_create_state(Win32State* state, HINSTANCE instance, const wchar_t* window_title, u32 width, u32 height, u32 thread_count);
LARGE_INTEGER win32_get_wall_clock(void);
f32 win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end, s64 perf_count_frequency);
void win32_process_pending_messages(InputState* input_state);
void win32_toggle_fullscreen(Win32Window* window);
void win32_set_mouse_captured(bool is_captured);
void win32_set_cursor_position(s32 x_client, s32 y_client);

void win32_process_digital_input_message(DigitalInput* new_state, bool is_down);

inline LARGE_INTEGER win32_get_wall_clock(void)
{    
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return(result);
}

inline f32 win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end, s64 perf_count_frequency)
{
    f32 result = ((f32)(end.QuadPart - start.QuadPart) /
                  (f32)perf_count_frequency);
    return(result);
}

inline u32 win32_get_cpu_core_count(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (u32)info.dwNumberOfProcessors;
}
