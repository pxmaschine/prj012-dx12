#include <Platform/Application.h>

#include <Rendering.h>


Application::Application(HINSTANCE instance, const wchar_t* window_title, u32 width, u32 height)
    : BaseType(this)
{
    m_state = win32_create_state(instance, window_title, width, height);
    m_renderer = make_unique_ptr<Renderer>(m_state.m_window.m_window_handle, width, height, true);
}

Application::~Application()
{
    m_renderer = nullptr;
}

bool Application::resize(u32 width, u32 height)
{
    if (m_state.m_client_width != width || m_state.m_client_height != height)
    {
      // Don't allow 0 size swap chain back buffers.
      m_state.m_client_width = ZV::max(1u, width );
      m_state.m_client_height = ZV::max( 1u, height);
  
      return true;
    }
  
    return false;
}
