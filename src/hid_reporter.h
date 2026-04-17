#ifndef HID_REPORTER_H
#define HID_REPORTER_H

#include <cstdint>
#include <functional>
#include <vector>
#include <array>
#include <string>

// 避免与 Linux input.h 中的宏冲突
#ifdef KEY_A
#undef KEY_A
#endif
#ifdef KEY_B
#undef KEY_B
#endif
#ifdef KEY_C
#undef KEY_C
#endif
#ifdef KEY_D
#undef KEY_D
#endif
#ifdef KEY_E
#undef KEY_E
#endif
#ifdef KEY_F
#undef KEY_F
#endif
#ifdef KEY_G
#undef KEY_G
#endif
#ifdef KEY_H
#undef KEY_H
#endif
#ifdef KEY_I
#undef KEY_I
#endif
#ifdef KEY_J
#undef KEY_J
#endif
#ifdef KEY_K
#undef KEY_K
#endif
#ifdef KEY_L
#undef KEY_L
#endif
#ifdef KEY_M
#undef KEY_M
#endif
#ifdef KEY_N
#undef KEY_N
#endif
#ifdef KEY_O
#undef KEY_O
#endif
#ifdef KEY_P
#undef KEY_P
#endif
#ifdef KEY_Q
#undef KEY_Q
#endif
#ifdef KEY_R
#undef KEY_R
#endif
#ifdef KEY_S
#undef KEY_S
#endif
#ifdef KEY_T
#undef KEY_T
#endif
#ifdef KEY_U
#undef KEY_U
#endif
#ifdef KEY_V
#undef KEY_V
#endif
#ifdef KEY_W
#undef KEY_W
#endif
#ifdef KEY_X
#undef KEY_X
#endif
#ifdef KEY_Y
#undef KEY_Y
#endif
#ifdef KEY_Z
#undef KEY_Z
#endif
#ifdef KEY_1
#undef KEY_1
#endif
#ifdef KEY_2
#undef KEY_2
#endif
#ifdef KEY_3
#undef KEY_3
#endif
#ifdef KEY_4
#undef KEY_4
#endif
#ifdef KEY_5
#undef KEY_5
#endif
#ifdef KEY_6
#undef KEY_6
#endif
#ifdef KEY_7
#undef KEY_7
#endif
#ifdef KEY_8
#undef KEY_8
#endif
#ifdef KEY_9
#undef KEY_9
#endif
#ifdef KEY_0
#undef KEY_0
#endif
#ifdef KEY_ENTER
#undef KEY_ENTER
#endif
#ifdef KEY_ESC
#undef KEY_ESC
#endif
#ifdef KEY_BACKSPACE
#undef KEY_BACKSPACE
#endif
#ifdef KEY_TAB
#undef KEY_TAB
#endif
#ifdef KEY_SPACE
#undef KEY_SPACE
#endif
#ifdef KEY_HOME
#undef KEY_HOME
#endif
#ifdef KEY_END
#undef KEY_END
#endif
#ifdef KEY_UP
#undef KEY_UP
#endif
#ifdef KEY_DOWN
#undef KEY_DOWN
#endif
#ifdef KEY_LEFT
#undef KEY_LEFT
#endif
#ifdef KEY_RIGHT
#undef KEY_RIGHT
#endif
#ifdef KEY_DELETE
#undef KEY_DELETE
#endif
#ifdef BTN_LEFT
#undef BTN_LEFT
#endif
#ifdef BTN_RIGHT
#undef BTN_RIGHT
#endif
#ifdef BTN_MIDDLE
#undef BTN_MIDDLE
#endif

class HIDReporter {
public:
    // 键盘修饰键位掩码
    enum Modifier : uint8_t {
        MOD_NONE      = 0x00,
        MOD_LCTRL     = 0x01,
        MOD_LSHIFT    = 0x02,
        MOD_LALT      = 0x04,
        MOD_LGUI      = 0x08,
        MOD_RCTRL     = 0x10,
        MOD_RSHIFT    = 0x20,
        MOD_RALT      = 0x40,
        MOD_RGUI      = 0x80
    };
    
    // 鼠标按键位掩码（使用不同的名称避免冲突）
    enum MouseButton : uint8_t {
        MOUSE_BTN_NONE   = 0x00,
        MOUSE_BTN_LEFT   = 0x01,
        MOUSE_BTN_RIGHT  = 0x02,
        MOUSE_BTN_MIDDLE = 0x04
    };
    
    // 常用按键码
    enum KeyCode : uint8_t {
        HID_KEY_NONE      = 0x00,
        HID_KEY_A         = 0x04,
        HID_KEY_B         = 0x05,
        HID_KEY_C         = 0x06,
        HID_KEY_D         = 0x07,
        HID_KEY_E         = 0x08,
        HID_KEY_F         = 0x09,
        HID_KEY_G         = 0x0A,
        HID_KEY_H         = 0x0B,
        HID_KEY_I         = 0x0C,
        HID_KEY_J         = 0x0D,
        HID_KEY_K         = 0x0E,
        HID_KEY_L         = 0x0F,
        HID_KEY_M         = 0x10,
        HID_KEY_N         = 0x11,
        HID_KEY_O         = 0x12,
        HID_KEY_P         = 0x13,
        HID_KEY_Q         = 0x14,
        HID_KEY_R         = 0x15,
        HID_KEY_S         = 0x16,
        HID_KEY_T         = 0x17,
        HID_KEY_U         = 0x18,
        HID_KEY_V         = 0x19,
        HID_KEY_W         = 0x1A,
        HID_KEY_X         = 0x1B,
        HID_KEY_Y         = 0x1C,
        HID_KEY_Z         = 0x1D,
        HID_KEY_1         = 0x1E,
        HID_KEY_2         = 0x1F,
        HID_KEY_3         = 0x20,
        HID_KEY_4         = 0x21,
        HID_KEY_5         = 0x22,
        HID_KEY_6         = 0x23,
        HID_KEY_7         = 0x24,
        HID_KEY_8         = 0x25,
        HID_KEY_9         = 0x26,
        HID_KEY_0         = 0x27,
        HID_KEY_ENTER     = 0x28,
        HID_KEY_ESC       = 0x29,
        HID_KEY_BACKSPACE = 0x2A,
        HID_KEY_TAB       = 0x2B,
        HID_KEY_SPACE     = 0x2C,
        HID_KEY_MINUS     = 0x2D,
        HID_KEY_EQUAL     = 0x2E,
        HID_KEY_LEFTBRACE = 0x2F,
        HID_KEY_RIGHTBRACE = 0x30,
        HID_KEY_BACKSLASH = 0x31,
        HID_KEY_SEMICOLON = 0x33,
        HID_KEY_APOSTROPHE = 0x34,
        HID_KEY_GRAVE     = 0x35,
        HID_KEY_COMMA     = 0x36,
        HID_KEY_DOT       = 0x37,
        HID_KEY_SLASH     = 0x38,
        HID_KEY_CAPSLOCK  = 0x39,
        HID_KEY_F1        = 0x3A,
        HID_KEY_F2        = 0x3B,
        HID_KEY_F3        = 0x3C,
        HID_KEY_F4        = 0x3D,
        HID_KEY_F5        = 0x3E,
        HID_KEY_F6        = 0x3F,
        HID_KEY_F7        = 0x40,
        HID_KEY_F8        = 0x41,
        HID_KEY_F9        = 0x42,
        HID_KEY_F10       = 0x43,
        HID_KEY_F11       = 0x44,
        HID_KEY_F12       = 0x45,
        HID_KEY_HOME      = 0x4A,
        HID_KEY_PAGEUP    = 0x4B,
        HID_KEY_DELETE    = 0x4C,
        HID_KEY_END       = 0x4D,
        HID_KEY_PAGEDOWN  = 0x4E,
        HID_KEY_RIGHT     = 0x4F,
        HID_KEY_LEFT      = 0x50,
        HID_KEY_DOWN      = 0x51,
        HID_KEY_UP        = 0x52
    };
    
    HIDReporter();
    
    // Optional callback invoked by typeString() after each key press and
    // release so the caller does not need to poll the report buffer.
    // Signature: bool send(const uint8_t* data, size_t len)
    using SendReportCallback = std::function<bool(const uint8_t*, size_t)>;
    void setSendCallback(SendReportCallback cb) { send_cb_ = cb; }
    
    // 键盘操作
    void keyPress(KeyCode key, Modifier mod = MOD_NONE);
    void keyRelease();
    void keyCombo(KeyCode key, Modifier mod = MOD_NONE, int duration_ms = 100);
    void typeString(const std::string& text, int delay_ms = 50);
    
    // 鼠标操作
    void mouseMove(int8_t dx, int8_t dy, int8_t wheel = 0);
    void mousePress(MouseButton btn);
    void mouseRelease(MouseButton btn);
    void mouseClick(MouseButton btn, int duration_ms = 50);
    
    // 获取报告数据
    const uint8_t* getKeyboardReport() const { return keyboard_report_.data(); }
    const uint8_t* getMouseReport() const { return mouse_report_.data(); }
    size_t getKeyboardReportSize() const { return 8; }
    size_t getMouseReportSize() const { return 4; }
    
private:
    std::array<uint8_t, 8> keyboard_report_;
    std::array<uint8_t, 4> mouse_report_;
    
    SendReportCallback send_cb_;
    
    uint8_t charToKeyCode(char c);
};

#endif // HID_REPORTER_H