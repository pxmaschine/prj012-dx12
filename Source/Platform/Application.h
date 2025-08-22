#pragma once

#include <CoreDefs.h>
#include <Platform/PlatformContext.h>
#include <Utility.h>

#if ZV_OS_WINDOWS
#include <Platform/Win32/Win32Platform.h>
#endif

class Renderer;

class Application : public Singleton<Application>
{
public:
    // Constructors
#if ZV_OS_WINDOWS
    Application(HINSTANCE instance, const wchar_t* window_title, u32 width = 1280, u32 height = 720);
    ~Application();
#endif

    bool resize(u32 width, u32 height);

    // void initialize();
    // s32 run();

    // Getters

#if ZV_OS_WINDOWS
    Win32Window* get_window() { return &m_state.m_window; }
#endif

    Renderer* get_renderer() const { return m_renderer.get(); }

    u32 get_client_width() const { return m_state.m_client_width; }
    u32 get_client_height() const { return m_state.m_client_height; }

    bool is_running() const { return m_state.m_running; }
    bool is_paused() const { return m_state.m_pause; }

    // TODO: Cleanup?
    s64 get_perf_count_frequency() const { return m_state.m_perf_count_frequency; }

    // Setters

    // TODO: Handle Renderer update here?
    void set_client_size(u32 width, u32 height) { m_state.m_client_width = width; m_state.m_client_height = height; }
    void set_running(bool running) { m_state.m_running = running; }
    void set_paused(bool paused) { m_state.m_pause = paused; }

private:
#if ZV_OS_WINDOWS
    Win32State m_state{};
#endif

    UniquePtr<Renderer> m_renderer = nullptr;
};
