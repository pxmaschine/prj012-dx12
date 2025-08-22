#pragma once

#include <CoreDefs.h>
#include <MathLib.h>

static constexpr u32 k_max_keyboard_keys = 256;
static constexpr u32 k_max_mouse_buttons = 5;
static constexpr u32 k_max_gamepad_buttons = 16;
static constexpr u32 k_max_gamepad_axes = 6;

// Win32 virtual mouse button codes
// https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
enum class MouseButtonWin32 : u8
{
    Left      = 1 << 0,
    Right     = 1 << 1,
    Middle    = 1 << 2,
    Extended1 = 1 << 3,
    Extended2 = 1 << 4,
};

// Win32 xinput gamepad button codes
// https://learn.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-xinput_gamepad
enum class GamepadButtonWin32 : u16
{
    Unknown       = 0,
    DpadUp        = 1 << 0,
    DpadDown      = 1 << 1,
    DpadLeft      = 1 << 2,
    DpadRight     = 1 << 3,
    Start         = 1 << 4,
    Back          = 1 << 5,
    LeftThumb     = 1 << 6,
    RightThumb    = 1 << 7,
    LeftShoulder  = 1 << 8,
    RightShoulder = 1 << 9,
    A             = 1 << 12,
    B             = 1 << 13,
    X             = 1 << 14,
    Y             = 1 << 15,
};

// Gamepad axes
enum class GamepadAxis : u8
{
    LeftX        = 0,  // Gamepad left stick X axis
    LeftY        = 1,  // Gamepad left stick Y axis
    RightX       = 2,  // Gamepad right stick X axis
    RightY       = 3,  // Gamepad right stick Y axis
    LeftTrigger  = 4,  // Gamepad back trigger left
    RightTrigger = 5,  // Gamepad back trigger right
};

// Win32 virtual key codes
// https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
enum class KeyboardKeyWin32 : u8
{
  Null = 0,

  Backspace = 8,
  Tab = 9,
  Numpad5_NoNumLock = 12,
  Enter = 13,
  Shift = 16,
  Ctrl = 17,
  Alt = 18,
  PauseBreak = 19,
  CapsLock = 20,
  Esc = 27,
  Space = 32,
  PageUp = 33,
  PageDown = 34,
  End = 35,
  Home = 36,
  ArrowLeft = 37,
  ArrowUp = 38,
  ArrowRight = 39,
  ArrowDown = 40,
  PrintScreen = 44,
  Insert = 45,
  Delete = 46,

  Key0 = 48,
  Key1 = 49,
  Key2 = 50,
  Key3 = 51,
  Key4 = 52,
  Key5 = 53,
  Key6 = 54,
  Key7 = 55,
  Key8 = 56,
  Key9 = 57,

  A = 65,
  B = 66,
  C = 67,
  D = 68,
  E = 69,
  F = 70,
  G = 71,
  H = 72,
  I = 73,
  J = 74,
  K = 75,
  L = 76,
  M = 77,
  N = 78,
  O = 79,
  P = 80,
  Q = 81,
  R = 82,
  S = 83,
  T = 84,
  U = 85,
  V = 86,
  W = 87,
  X = 88,
  Y = 89,
  Z = 90,

  LeftWin = 91,
  RightWin = 92,
  Menu = 93,

  Numpad0 = 96,
  Numpad1 = 97,
  Numpad2 = 98,
  Numpad3 = 99,
  Numpad4 = 100,
  Numpad5 = 101,
  Numpad6 = 102,
  Numpad7 = 103,
  Numpad8 = 104,
  Numpad9 = 105,
  NumpadMultiply = 106,
  NumpadAdd = 107,
  NumpadSubtract = 109,
  NumpadDecimal = 110,
  NumpadDivide = 111,

  F1 = 112,
  F2 = 113,
  F3 = 114,
  F4 = 115,
  F5 = 116,
  F6 = 117,
  F7 = 118,
  F8 = 119,
  F9 = 120,
  F10 = 121,
  F11 = 122,
  F12 = 123,

  NumLock = 144,
  ScrollLock = 145,

  LeftShift = 160,
  RightShift = 161,
  LeftCtrl = 162,
  RightCtrl = 163
};

struct DigitalInput
{
    bool m_is_down = false;     // Whether the key was down at the end of the frame
    u32 m_num_transitions = 0;  // The number of times the key was transitioned from up to down or down to up within the frame
};

struct AnalogInput
{
    union
    {
        struct
        {
            u16 m_x = 0;  // The average of values read within the frame
            u16 m_y = 0;  // The average of values read within the frame
        } m_stick;
        u32 m_trigger_value = 0;  // The maximum read value within the frame
    };
};

struct InputSettings
{
    f32 m_mouse_sensitivity;
    f32 m_camera_speed;

    bool m_is_mouse_captured = false;
};

struct InputState
{
    InputSettings m_settings{};

    bool m_quit_requested = false;

    struct KeyboardInput
    {
        DigitalInput m_keys[k_max_keyboard_keys];
    } m_keyboard;

    struct MouseInput
    {
        DigitalInput m_buttons[k_max_mouse_buttons];
        Vector2 m_position{};      // Absolute mouse position in client space (pixels)
        Vector2 m_scroll_delta{};  // Mouse scroll delta since last frame
    } m_mouse;

    struct GamepadInput
    {
        u8 m_id;
        bool m_is_connected;
        AnalogInput m_analog_axes[k_max_gamepad_axes];
        DigitalInput m_buttons[k_max_gamepad_buttons];
    } m_gamepad;
};

namespace ZV
{
  namespace Input
  {
    void initialize();
    void shutdown();

    void update();

    // Settings

    void set_mouse_sensitivity(f32 sensitivity);
    f32 get_mouse_sensitivity();
    void set_camera_speed(f32 speed);
    f32 get_camera_speed();
    void set_mouse_captured(bool is_captured);
    bool is_mouse_captured();

    // Input state

    bool is_quit_requested();

    bool is_key_down(KeyboardKeyWin32 key);
    bool is_key_up(KeyboardKeyWin32 key);
    bool was_key_pressed(KeyboardKeyWin32 key);
    bool was_key_released(KeyboardKeyWin32 key);

    bool is_mouse_button_down(MouseButtonWin32 button);
    bool is_mouse_button_up(MouseButtonWin32 button);
    bool was_mouse_button_pressed(MouseButtonWin32 button);
    bool was_mouse_button_released(MouseButtonWin32 button);
    Vector2 get_mouse_delta();
    Vector2 get_mouse_position();

    bool is_gamepad_button_down(GamepadButtonWin32 button);
    bool is_gamepad_button_up(GamepadButtonWin32 button);
    bool was_gamepad_button_pressed(GamepadButtonWin32 button);
    bool was_gamepad_button_released(GamepadButtonWin32 button);
  }
}
