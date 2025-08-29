#include <Platform/Input.h>

#include <Platform/Platform.h>
#include <Platform/Win32/Win32Platform.h>
#include <Utility.h>


namespace { class InputManager; }
static UniquePtr<InputManager> s_input_manager{ nullptr };

namespace
{
  class InputManager : public Singleton<InputManager>
  {
  private:
    InputState m_input_states[2] = {};
    InputState* m_current_frame_input = nullptr;
    InputState* m_previous_frame_input = nullptr;

    InputSettings m_settings{};

    HWND m_window_handle = nullptr;

  public:
    InputManager();

  public:
    void update();

    void set_mouse_sensitivity(f32 sensitivity) { m_settings.m_mouse_sensitivity = sensitivity; }
    f32 get_mouse_sensitivity() const { return m_settings.m_mouse_sensitivity; }
    void set_camera_speed(f32 speed) { m_settings.m_camera_speed = speed; }
    f32 get_camera_speed() const { return m_settings.m_camera_speed; }
    void set_mouse_captured(bool is_captured);
    bool is_mouse_captured() const { return m_settings.m_is_mouse_captured; }

    bool is_quit_requested() const { return m_current_frame_input->m_quit_requested; }

    bool is_key_down(KeyboardKeyWin32 key) const;
    bool is_key_up(KeyboardKeyWin32 key) const;
    bool was_key_pressed(KeyboardKeyWin32 key) const;
    bool was_key_released(KeyboardKeyWin32 key) const;

    bool is_mouse_button_down(MouseButtonWin32 button) const;
    bool is_mouse_button_up(MouseButtonWin32 button) const;
    bool was_mouse_button_pressed(MouseButtonWin32 button) const;
    bool was_mouse_button_released(MouseButtonWin32 button) const;
    Vector2 get_mouse_delta() const;
    Vector2 get_mouse_position() const;

    bool is_gamepad_button_down(GamepadButtonWin32 button) const;
    bool is_gamepad_button_up(GamepadButtonWin32 button) const;
    bool was_gamepad_button_pressed(GamepadButtonWin32 button) const;
    bool was_gamepad_button_released(GamepadButtonWin32 button) const;

  private:
    void reset_current_input_state();
  
    bool is_down(const DigitalInput& input) const;
    bool is_up(const DigitalInput& input) const;
    bool was_pressed(const DigitalInput& input) const;
    bool was_released(const DigitalInput& input) const;
  };

  InputManager::InputManager()
    : BaseType(this)
  {
    m_current_frame_input = &m_input_states[0];
    m_previous_frame_input = &m_input_states[1];

    m_window_handle = Platform::window_get_handle();

    POINT mouse_p;
    GetCursorPos(&mouse_p);
    ScreenToClient(m_window_handle, &mouse_p);
    m_previous_frame_input->m_mouse.m_position.x = (f32)mouse_p.x;
    m_previous_frame_input->m_mouse.m_position.y = (f32)((Platform::window_get_client_height() - 1) - mouse_p.y);
  }

  void InputManager::reset_current_input_state()
  {
    *m_current_frame_input = {};

    for (u32 i = 0; i < k_max_keyboard_keys; ++i)
    {
      m_current_frame_input->m_keyboard.m_keys[i].m_is_down = m_previous_frame_input->m_keyboard.m_keys[i].m_is_down;
    }

    for (u32 i = 0; i < k_max_mouse_buttons; ++i)
    {
      m_current_frame_input->m_mouse.m_buttons[i].m_is_down = m_previous_frame_input->m_mouse.m_buttons[i].m_is_down;
    }

    for (u32 i = 0; i < k_max_gamepad_buttons; ++i)
    {
      m_current_frame_input->m_gamepad.m_buttons[i].m_is_down = m_previous_frame_input->m_gamepad.m_buttons[i].m_is_down;
    }
  }

  bool InputManager::is_down(const DigitalInput& input) const
  {
    return input.m_is_down;
  }

  bool InputManager::is_up(const DigitalInput& input) const
  {
    return !input.m_is_down;
  }

  bool InputManager::was_pressed(const DigitalInput& input) const
  {
    return input.m_num_transitions > 1 ||
      (input.m_num_transitions == 1 && input.m_is_down);
  }

  bool InputManager::was_released(const DigitalInput& input) const
  {
    return input.m_num_transitions > 1 ||
      (input.m_num_transitions == 1 && !input.m_is_down);
  }

  void InputManager::update()
  {
    // Reset input and process message queue
    {
      InputState* temp = m_current_frame_input;
      m_current_frame_input = m_previous_frame_input;
      m_previous_frame_input = temp;

      reset_current_input_state();

      win32_process_pending_messages(m_current_frame_input);
    }

    if (m_settings.m_is_mouse_captured)
    {
      s32 x_client = (s32)(Platform::window_get_client_width() * 0.5f);
      s32 y_client = (s32)(Platform::window_get_client_height() * 0.5f);

      m_previous_frame_input->m_mouse.m_position = { (f32)x_client, (f32)y_client };
    }

    // Update mouse input
    {
      // POINT mouse_p;
      POINT mouse_p;
      GetCursorPos(&mouse_p);
      ScreenToClient(m_window_handle, &mouse_p);
      // TODO: Clip space?
      m_current_frame_input->m_mouse.m_position.x = (f32)mouse_p.x;
      m_current_frame_input->m_mouse.m_position.y = (f32)((Platform::window_get_client_height() - 1) - mouse_p.y);  // TODO: backbuffer height (1080)
      m_current_frame_input->m_mouse.m_scroll_delta.x = 0;  // TODO: Support mousewheel?
      m_current_frame_input->m_mouse.m_scroll_delta.y = 0;  // TODO: Support mousewheel?
    
      DWORD win_button_id[k_max_mouse_buttons] =
      {
        VK_LBUTTON,
        VK_MBUTTON,
        VK_RBUTTON,
        VK_XBUTTON1,
        VK_XBUTTON2,
      };
      for (u32 button_index = 0; button_index < k_max_mouse_buttons; ++button_index)
      {
        u32 win32_button_id = win_button_id[button_index];
        win32_process_digital_input_message(
          &m_current_frame_input->m_mouse.m_buttons[win32_button_id], 
          GetKeyState(win32_button_id) & (1 << 15));
      }
    }

    // Update gamepad input
    {
      // TODO: xinput handling
    }

    if (m_settings.m_is_mouse_captured)
    {
      s32 x_client = (s32)(Platform::window_get_client_width() * 0.5f);
      s32 y_client = (s32)(Platform::window_get_client_height() * 0.5f);
      win32_set_cursor_position(x_client, y_client);
    }
  }

  void InputManager::set_mouse_captured(bool is_captured)
  {
    m_settings.m_is_mouse_captured = is_captured;

    win32_set_mouse_captured(is_captured);
  }

  bool InputManager::is_key_down(KeyboardKeyWin32 key) const
  {
    u32 key_index = static_cast<u32>(key);

    return is_down(m_current_frame_input->m_keyboard.m_keys[key_index]);
  }

  bool InputManager::is_key_up(KeyboardKeyWin32 key) const
  {
    u32 key_index = static_cast<u32>(key);

    return is_up(m_current_frame_input->m_keyboard.m_keys[key_index]);
  }

  bool InputManager::was_key_pressed(KeyboardKeyWin32 key) const
  {
    u32 key_index = static_cast<u32>(key);

    return was_pressed(m_current_frame_input->m_keyboard.m_keys[key_index]);
  }

  bool InputManager::was_key_released(KeyboardKeyWin32 key) const
  {
    u32 key_index = static_cast<u32>(key);

    return was_released(m_current_frame_input->m_keyboard.m_keys[key_index]);
  }

  bool InputManager::is_mouse_button_down(MouseButtonWin32 button) const
  {
    u32 button_index = static_cast<u32>(button);

    return is_down(m_current_frame_input->m_mouse.m_buttons[button_index]);
  }

  bool InputManager::is_mouse_button_up(MouseButtonWin32 button) const
  {
    u32 button_index = static_cast<u32>(button);

    return is_up(m_current_frame_input->m_mouse.m_buttons[button_index]);
  }

  bool InputManager::was_mouse_button_pressed(MouseButtonWin32 button) const
  {
    u32 button_index = static_cast<u32>(button);

    return was_pressed(m_current_frame_input->m_mouse.m_buttons[button_index]);
  }

  bool InputManager::was_mouse_button_released(MouseButtonWin32 button) const
  {
    u32 button_index = static_cast<u32>(button);

    return was_released(m_current_frame_input->m_mouse.m_buttons[button_index]);
  }

  Vector2 InputManager::get_mouse_delta() const
  {
    return m_current_frame_input->m_mouse.m_position - m_previous_frame_input->m_mouse.m_position;
  }

  Vector2 InputManager::get_mouse_position() const
  {
    return m_current_frame_input->m_mouse.m_position;
  }

  bool InputManager::is_gamepad_button_down(GamepadButtonWin32 button) const
  {
    u32 button_index = static_cast<u32>(button);

    return is_down(m_current_frame_input->m_gamepad.m_buttons[button_index]);
  }

  bool InputManager::is_gamepad_button_up(GamepadButtonWin32 button) const
  {
    u32 button_index = static_cast<u32>(button);

    return is_up(m_current_frame_input->m_gamepad.m_buttons[button_index]);
  }

  bool InputManager::was_gamepad_button_pressed(GamepadButtonWin32 button) const
  {
    u32 button_index = static_cast<u32>(button);

    return was_pressed(m_current_frame_input->m_gamepad.m_buttons[button_index]);
  }

  bool InputManager::was_gamepad_button_released(GamepadButtonWin32 button) const
  {
    u32 button_index = static_cast<u32>(button);

    return was_released(m_current_frame_input->m_gamepad.m_buttons[button_index]);
  }
}

void ZV::Input::initialize()
{
  zv_assert_msg(s_input_manager == nullptr, "Input manager already initialized!");
  s_input_manager = make_unique_ptr<InputManager>();
}

void ZV::Input::shutdown()
{
  s_input_manager = nullptr;
}

void ZV::Input::update()
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  s_input_manager->update();
}

// Settings

void ZV::Input::set_mouse_sensitivity(f32 sensitivity)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  s_input_manager->set_mouse_sensitivity(sensitivity);
}

f32 ZV::Input::get_mouse_sensitivity()
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->get_mouse_sensitivity();
}

void ZV::Input::set_mouse_captured(bool is_captured)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  s_input_manager->set_mouse_captured(is_captured);
}

bool ZV::Input::is_mouse_captured()
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->is_mouse_captured();
}

void ZV::Input::set_camera_speed(f32 speed)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  s_input_manager->set_camera_speed(speed);
}

f32 ZV::Input::get_camera_speed()
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->get_camera_speed();
}

// Input state

bool ZV::Input::is_quit_requested()
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->is_quit_requested();
}

bool ZV::Input::is_key_down(KeyboardKeyWin32 key)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->is_key_down(key);
}

bool ZV::Input::is_key_up(KeyboardKeyWin32 key)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->is_key_up(key);
}

bool ZV::Input::was_key_pressed(KeyboardKeyWin32 key)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->was_key_pressed(key);
}

bool ZV::Input::was_key_released(KeyboardKeyWin32 key)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->was_key_released(key);
}

bool ZV::Input::is_mouse_button_down(MouseButtonWin32 button)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->is_mouse_button_down(button);
}

bool ZV::Input::is_mouse_button_up(MouseButtonWin32 button)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->is_mouse_button_up(button);
}

bool ZV::Input::was_mouse_button_pressed(MouseButtonWin32 button)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->was_mouse_button_pressed(button);
}

bool ZV::Input::was_mouse_button_released(MouseButtonWin32 button)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->was_mouse_button_released(button);
}

Vector2 ZV::Input::get_mouse_delta()
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->get_mouse_delta();
}

Vector2 ZV::Input::get_mouse_position()
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->get_mouse_position();
}

bool ZV::Input::is_gamepad_button_down(GamepadButtonWin32 button)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->is_gamepad_button_down(button);
}

bool ZV::Input::is_gamepad_button_up(GamepadButtonWin32 button)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->is_gamepad_button_up(button);
}

bool ZV::Input::was_gamepad_button_pressed(GamepadButtonWin32 button)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->was_gamepad_button_pressed(button);
}

bool ZV::Input::was_gamepad_button_released(GamepadButtonWin32 button)
{
  zv_assert_msg(s_input_manager != nullptr, "Input manager not initialized!");
  return s_input_manager->was_gamepad_button_released(button);
}
