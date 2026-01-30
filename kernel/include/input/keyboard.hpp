#pragma once

#include "../types.hpp"

namespace sertos::input {

constexpr usize KEYBOARD_BUFFER_SIZE = 256;

enum class KeyCode : u8 {
    None = 0,
    Escape = 1,
    Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,
    Minus, Equals, Backspace, Tab,
    Q, W, E, R, T, Y, U, I, O, P,
    LeftBracket, RightBracket, Enter, LeftCtrl,
    A, S, D, F, G, H, J, K, L,
    Semicolon, Quote, Backtick, LeftShift, Backslash,
    Z, X, C, V, B, N, M,
    Comma, Period, Slash, RightShift,
    NumpadMultiply, LeftAlt, Space, CapsLock,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,
    NumLock, ScrollLock,
    Numpad7, Numpad8, Numpad9, NumpadMinus,
    Numpad4, Numpad5, Numpad6, NumpadPlus,
    Numpad1, Numpad2, Numpad3, Numpad0, NumpadPeriod,
    F11 = 87, F12 = 88,
    
    Up = 200, Down = 208, Left = 203, Right = 205,
    Home = 199, End = 207, PageUp = 201, PageDown = 209,
    Insert = 210, Delete = 211
};

struct KeyEvent {
    KeyCode code;
    char ascii;
    bool pressed;
    bool shift;
    bool ctrl;
    bool alt;
};

class Keyboard {
public:
    static void initialize();
    static void handleScancode(u8 scancode);
    
    static bool hasKey();
    static KeyEvent getKey();
    static char getChar();
    static bool readLine(char* buffer, usize maxLen);
    
    static bool isShiftPressed();
    static bool isCtrlPressed();
    static bool isAltPressed();

private:
    static void pushKey(const KeyEvent& event);
    static char scancodeToAscii(u8 scancode, bool shift);
    
    static KeyEvent sBuffer[KEYBOARD_BUFFER_SIZE];
    static usize sHead;
    static usize sTail;
    static bool sShiftPressed;
    static bool sCtrlPressed;
    static bool sAltPressed;
    static bool sCapsLock;
    static bool sInitialized;
};

}
