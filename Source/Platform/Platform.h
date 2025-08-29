#pragma once

#include <CoreDefs.h>
#include <Platform/PlatformContext.h>
#include <Platform/Jobs.h>
#include <Utility.h>

#if ZV_OS_WINDOWS
#include <Platform/Win32/Win32Platform.h>
#endif

class Renderer;

namespace Platform
{
    struct CreationInfo
    {
#if ZV_OS_WINDOWS
        HINSTANCE m_instance;
        const wchar_t* m_window_title;
        u32 m_width = 1280;
        u32 m_height = 720;
        u32 m_thread_count = 1;
#endif
    };

    void initialize(const CreationInfo& creation_info);
    void shutdown();

    Renderer* get_renderer();

    void add_job(JobPriority priority, JobQueueCallback* callback, void* data);
    void complete_all_jobs(JobPriority priority);

    bool window_resize(u32 width, u32 height);
    void window_toggle_fullscreen();
#if ZV_OS_WINDOWS
    HWND window_get_handle();
#endif
    // TODO: Handle Renderer update here?
    void window_set_client_size(u32 width, u32 height);
    u32 window_get_client_width();
    u32 window_get_client_height();

    void app_set_running(bool running);
    void app_set_paused(bool paused);
    bool app_is_running();
    bool app_is_paused();
    // TODO: Cleanup?
    s64 app_get_perf_count_frequency();
}
