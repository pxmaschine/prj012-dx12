#include <Platform/Platform.h>

#include <Rendering.h>

namespace { struct PlatformApplication; }
static UniquePtr<PlatformApplication> s_platform_application{ nullptr };

namespace
{
    struct PlatformApplication
    {
    #if ZV_OS_WINDOWS
        Win32State m_state{};
    #endif
        UniquePtr<Renderer> m_renderer{ nullptr };
    };
};

void Platform::initialize(const CreationInfo& creation_info)
{
    zv_assert_msg(s_platform_application == nullptr, "Platform application already initialized");
    
    s_platform_application = make_unique_ptr<PlatformApplication>();

#if ZV_OS_WINDOWS
    win32_create_state(&s_platform_application->m_state, creation_info.m_instance, creation_info.m_window_title, creation_info.m_width, creation_info.m_height, creation_info.m_thread_count);
    s_platform_application->m_renderer = make_unique_ptr<Renderer>(s_platform_application->m_state.m_window.m_window_handle, creation_info.m_width, creation_info.m_height, true);
#endif
}

void Platform::shutdown()
{
    s_platform_application = nullptr;
}

Renderer* Platform::get_renderer()
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    return s_platform_application->m_renderer.get();
}

void Platform::add_job(JobPriority priority, JobQueueCallback* callback, void* data)
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");

#if ZV_OS_WINDOWS
    switch (priority)
    {
        case JobPriority::High:
        {
            win32_add_job_entry(&s_platform_application->m_state.m_high_priority_queue, callback, data);
            break;
        }
        case JobPriority::Low:
        {
            win32_add_job_entry(&s_platform_application->m_state.m_low_priority_queue, callback, data);
            break;
        }
    }
#endif
}

void Platform::complete_all_jobs(JobPriority priority)
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");

#if ZV_OS_WINDOWS
    switch (priority)
    {
        case JobPriority::High:
        {
            win32_complete_all_jobs(&s_platform_application->m_state.m_high_priority_queue);
            break;
        }
        case JobPriority::Low:
        {
            win32_complete_all_jobs(&s_platform_application->m_state.m_low_priority_queue);
            break;
        }
    }
#endif
}

bool Platform::window_resize(u32 width, u32 height)
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");

    auto& state = s_platform_application->m_state;

    if (state.m_client_width != width || state.m_client_height != height)
    {
      // Don't allow 0 size swap chain back buffers.
      state.m_client_width = ZV::max(1u, width );
      state.m_client_height = ZV::max( 1u, height);
  
      return true;
    }
  
    return false;
}

void Platform::window_toggle_fullscreen()
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");

#if ZV_OS_WINDOWS
    win32_toggle_fullscreen(&s_platform_application->m_state.m_window);
#endif
}
    
#if ZV_OS_WINDOWS
HWND Platform::window_get_handle()
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    return s_platform_application->m_state.m_window.m_window_handle;
}
#endif

u32 Platform::window_get_client_width()
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    return s_platform_application->m_state.m_client_width;
}

u32 Platform::window_get_client_height()
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    return s_platform_application->m_state.m_client_height;
}

// TODO: Handle Renderer update here?
void Platform::window_set_client_size(u32 width, u32 height)
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    s_platform_application->m_state.m_client_width = width;
    s_platform_application->m_state.m_client_height = height;
}

bool Platform::app_is_running()
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    return s_platform_application->m_state.m_running;
}

bool Platform::app_is_paused()
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    return s_platform_application->m_state.m_pause;
}

void Platform::app_set_running(bool running)
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    s_platform_application->m_state.m_running = running;
}

void Platform::app_set_paused(bool paused)
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    s_platform_application->m_state.m_pause = paused;
}

s64 Platform::app_get_perf_count_frequency()
{
    zv_assert_msg(s_platform_application != nullptr, "Platform application not initialized");
    return s_platform_application->m_state.m_perf_count_frequency;
}
