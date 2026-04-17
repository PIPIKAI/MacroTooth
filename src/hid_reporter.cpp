#include "hid_reporter.h"
#include <thread>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iostream>

HIDReporter::HIDReporter() {
    keyboard_report_.fill(0);
    mouse_report_.fill(0);
}

void HIDReporter::keyPress(KeyCode key, Modifier mod) {
    keyboard_report_.fill(0);
    keyboard_report_[0] = mod;
    keyboard_report_[2] = key;
    std::cout << "keyPress: key=" << (int)key << ", mod=" << (int)mod 
              << " -> report[0]=" << (int)keyboard_report_[0] 
              << ", report[2]=" << (int)keyboard_report_[2] << std::endl;
}

void HIDReporter::keyRelease() {
    keyboard_report_.fill(0);
}

void HIDReporter::keyCombo(KeyCode key, Modifier mod, int duration_ms) {
    keyPress(key, mod);
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    keyRelease();
}

void HIDReporter::typeString(const std::string& text, int delay_ms) {
    for (char c : text) {
        Modifier mod = MOD_NONE;
        uint8_t keycode;
        
        if (std::isupper(c)) {
            mod = MOD_LSHIFT;
            c = std::tolower(c);
        }
        
        switch (c) {
        case '!': mod = MOD_LSHIFT; keycode = HID_KEY_1; break;
        case '@': mod = MOD_LSHIFT; keycode = HID_KEY_2; break;
        case '#': mod = MOD_LSHIFT; keycode = HID_KEY_3; break;
        case '$': mod = MOD_LSHIFT; keycode = HID_KEY_4; break;
        case '%': mod = MOD_LSHIFT; keycode = HID_KEY_5; break;
        case '^': mod = MOD_LSHIFT; keycode = HID_KEY_6; break;
        case '&': mod = MOD_LSHIFT; keycode = HID_KEY_7; break;
        case '*': mod = MOD_LSHIFT; keycode = HID_KEY_8; break;
        case '(': mod = MOD_LSHIFT; keycode = HID_KEY_9; break;
        case ')': mod = MOD_LSHIFT; keycode = HID_KEY_0; break;
        case '_': mod = MOD_LSHIFT; keycode = HID_KEY_MINUS; break;
        case '+': mod = MOD_LSHIFT; keycode = HID_KEY_EQUAL; break;
        case '{': mod = MOD_LSHIFT; keycode = HID_KEY_LEFTBRACE; break;
        case '}': mod = MOD_LSHIFT; keycode = HID_KEY_RIGHTBRACE; break;
        case '|': mod = MOD_LSHIFT; keycode = HID_KEY_BACKSLASH; break;
        case ':': mod = MOD_LSHIFT; keycode = HID_KEY_SEMICOLON; break;
        case '"': mod = MOD_LSHIFT; keycode = HID_KEY_APOSTROPHE; break;
        case '<': mod = MOD_LSHIFT; keycode = HID_KEY_COMMA; break;
        case '>': mod = MOD_LSHIFT; keycode = HID_KEY_DOT; break;
        case '?': mod = MOD_LSHIFT; keycode = HID_KEY_SLASH; break;
        case '~': mod = MOD_LSHIFT; keycode = HID_KEY_GRAVE; break;
        default:
            keycode = charToKeyCode(c);
            break;
        }
        
        if (keycode != HID_KEY_NONE) {
            // Press
            keyPress(static_cast<KeyCode>(keycode), mod);
            if (send_cb_)
                send_cb_(keyboard_report_.data(), keyboard_report_.size());
            
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            
            // Release
            keyRelease();
            if (send_cb_)
                send_cb_(keyboard_report_.data(), keyboard_report_.size());
            
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }
}

void HIDReporter::mouseMove(int8_t dx, int8_t dy, int8_t wheel) {
    mouse_report_[0] = mouse_report_[0] & 0x07;
    mouse_report_[1] = dx;
    mouse_report_[2] = dy;
    mouse_report_[3] = wheel;
}

void HIDReporter::mousePress(MouseButton btn) {
    mouse_report_[0] |= btn;
}

void HIDReporter::mouseRelease(MouseButton btn) {
    mouse_report_[0] &= ~btn;
}

void HIDReporter::mouseClick(MouseButton btn, int duration_ms) {
    mousePress(btn);
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    mouseRelease(btn);
}

uint8_t HIDReporter::charToKeyCode(char c) {
    switch (std::tolower(c)) {
    case 'a': return HID_KEY_A;
    case 'b': return HID_KEY_B;
    case 'c': return HID_KEY_C;
    case 'd': return HID_KEY_D;
    case 'e': return HID_KEY_E;
    case 'f': return HID_KEY_F;
    case 'g': return HID_KEY_G;
    case 'h': return HID_KEY_H;
    case 'i': return HID_KEY_I;
    case 'j': return HID_KEY_J;
    case 'k': return HID_KEY_K;
    case 'l': return HID_KEY_L;
    case 'm': return HID_KEY_M;
    case 'n': return HID_KEY_N;
    case 'o': return HID_KEY_O;
    case 'p': return HID_KEY_P;
    case 'q': return HID_KEY_Q;
    case 'r': return HID_KEY_R;
    case 's': return HID_KEY_S;
    case 't': return HID_KEY_T;
    case 'u': return HID_KEY_U;
    case 'v': return HID_KEY_V;
    case 'w': return HID_KEY_W;
    case 'x': return HID_KEY_X;
    case 'y': return HID_KEY_Y;
    case 'z': return HID_KEY_Z;
    case '1': return HID_KEY_1;
    case '2': return HID_KEY_2;
    case '3': return HID_KEY_3;
    case '4': return HID_KEY_4;
    case '5': return HID_KEY_5;
    case '6': return HID_KEY_6;
    case '7': return HID_KEY_7;
    case '8': return HID_KEY_8;
    case '9': return HID_KEY_9;
    case '0': return HID_KEY_0;
    case ' ': return HID_KEY_SPACE;
    case '-': return HID_KEY_MINUS;
    case '=': return HID_KEY_EQUAL;
    case '[': return HID_KEY_LEFTBRACE;
    case ']': return HID_KEY_RIGHTBRACE;
    case '\\': return HID_KEY_BACKSLASH;
    case ';': return HID_KEY_SEMICOLON;
    case '\'': return HID_KEY_APOSTROPHE;
    case '`': return HID_KEY_GRAVE;
    case ',': return HID_KEY_COMMA;
    case '.': return HID_KEY_DOT;
    case '/': return HID_KEY_SLASH;
    case '\n': return HID_KEY_ENTER;
    case '\t': return HID_KEY_TAB;
    default: return HID_KEY_NONE;
    }
}