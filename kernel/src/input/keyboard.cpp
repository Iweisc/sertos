#include "../../include/input/keyboard.hpp"
#include "../../include/graphics/console.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::input {

KeyEvent Keyboard::sBuffer[KEYBOARD_BUFFER_SIZE];
usize Keyboard::sHead = 0;
usize Keyboard::sTail = 0;
bool Keyboard::sShiftPressed = false;
bool Keyboard::sCtrlPressed = false;
bool Keyboard::sAltPressed = false;
bool Keyboard::sCapsLock = false;
bool Keyboard::sInitialized = false;

namespace {

constexpr u8 SCANCODE_RELEASE = 0x80;
constexpr u8 SCANCODE_EXTENDED = 0xE0;

constexpr u8 SC_LSHIFT = 0x2A;
constexpr u8 SC_RSHIFT = 0x36;
constexpr u8 SC_LCTRL = 0x1D;
constexpr u8 SC_LALT = 0x38;
constexpr u8 SC_CAPSLOCK = 0x3A;

const char scancodeMapLower[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
    '7', '8', '9', '-',
    '4', '5', '6', '+',
    '1', '2', '3', '0', '.',
    0, 0, 0, 0, 0
};

const char scancodeMapUpper[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
    '7', '8', '9', '-',
    '4', '5', '6', '+',
    '1', '2', '3', '0', '.',
    0, 0, 0, 0, 0
};

KeyCode scancodeToKeyCode(u8 scancode) {
    if (scancode >= sizeof(scancodeMapLower)) {
        return KeyCode::None;
    }
    return static_cast<KeyCode>(scancode);
}

}

void Keyboard::initialize() {
    sHead = 0;
    sTail = 0;
    sShiftPressed = false;
    sCtrlPressed = false;
    sAltPressed = false;
    sCapsLock = false;
    sInitialized = true;
}

void Keyboard::handleScancode(u8 scancode) {
    if (!sInitialized) return;
    
    static bool extended = false;
    
    if (scancode == SCANCODE_EXTENDED) {
        extended = true;
        return;
    }
    
    bool released = (scancode & SCANCODE_RELEASE) != 0;
    u8 code = scancode & ~SCANCODE_RELEASE;
    
    if (code == SC_LSHIFT || code == SC_RSHIFT) {
        sShiftPressed = !released;
        extended = false;
        return;
    }
    
    if (code == SC_LCTRL) {
        sCtrlPressed = !released;
        extended = false;
        return;
    }
    
    if (code == SC_LALT) {
        sAltPressed = !released;
        extended = false;
        return;
    }
    
    if (code == SC_CAPSLOCK && !released) {
        sCapsLock = !sCapsLock;
        extended = false;
        return;
    }
    
    KeyEvent event;
    event.pressed = !released;
    event.shift = sShiftPressed;
    event.ctrl = sCtrlPressed;
    event.alt = sAltPressed;
    
    if (extended) {
        switch (code) {
            case 0x48: event.code = KeyCode::Up; break;
            case 0x50: event.code = KeyCode::Down; break;
            case 0x4B: event.code = KeyCode::Left; break;
            case 0x4D: event.code = KeyCode::Right; break;
            case 0x47: event.code = KeyCode::Home; break;
            case 0x4F: event.code = KeyCode::End; break;
            case 0x49: event.code = KeyCode::PageUp; break;
            case 0x51: event.code = KeyCode::PageDown; break;
            case 0x52: event.code = KeyCode::Insert; break;
            case 0x53: event.code = KeyCode::Delete; break;
            default: event.code = KeyCode::None; break;
        }
        event.ascii = 0;
        extended = false;
    } else {
        event.code = scancodeToKeyCode(code);
        event.ascii = scancodeToAscii(code, sShiftPressed ^ sCapsLock);
    }
    
    if (event.code != KeyCode::None) {
        pushKey(event);
    }
}

bool Keyboard::hasKey() {
    return sHead != sTail;
}

KeyEvent Keyboard::getKey() {
    if (!hasKey()) {
        return {KeyCode::None, 0, false, false, false, false};
    }
    
    KeyEvent event = sBuffer[sTail];
    sTail = (sTail + 1) % KEYBOARD_BUFFER_SIZE;
    return event;
}

char Keyboard::getChar() {
    while (true) {
        if (!hasKey()) {
            cpu::hlt();
            continue;
        }
        
        KeyEvent event = getKey();
        if (event.pressed && event.ascii != 0) {
            return event.ascii;
        }
    }
}

bool Keyboard::readLine(char* buffer, usize maxLen) {
    if (!buffer || maxLen == 0) return false;
    
    usize pos = 0;
    
    while (pos < maxLen - 1) {
        char c = getChar();
        
        if (c == '\n') {
            buffer[pos] = '\0';
            graphics::Console::putChar('\n');
            return true;
        }
        
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                graphics::Console::putChar('\b');
            }
            continue;
        }
        
        if (c >= 32 && c < 127) {
            buffer[pos++] = c;
            graphics::Console::putChar(c);
        }
    }
    
    buffer[pos] = '\0';
    return true;
}

bool Keyboard::isShiftPressed() { return sShiftPressed; }
bool Keyboard::isCtrlPressed() { return sCtrlPressed; }
bool Keyboard::isAltPressed() { return sAltPressed; }

void Keyboard::pushKey(const KeyEvent& event) {
    usize nextHead = (sHead + 1) % KEYBOARD_BUFFER_SIZE;
    if (nextHead == sTail) {
        return;
    }
    
    sBuffer[sHead] = event;
    sHead = nextHead;
}

char Keyboard::scancodeToAscii(u8 scancode, bool shift) {
    if (scancode >= sizeof(scancodeMapLower)) {
        return 0;
    }
    
    if (shift) {
        return scancodeMapUpper[scancode];
    }
    return scancodeMapLower[scancode];
}

}
