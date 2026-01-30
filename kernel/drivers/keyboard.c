#include "../include/keyboard.h"
#include "../include/idt.h"
#include "../include/ports.h"

static bool key_states[256];
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static keyboard_callback_t user_callback = NULL;

static const char scancode_to_ascii_lower[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char scancode_to_ascii_upper[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

static void keyboard_handler(registers_t* regs) {
    (void)regs;
    
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    bool released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7F;
    
    if (released) {
        key_states[key] = false;
        
        if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT) {
            shift_pressed = false;
        } else if (key == KEY_LEFT_CTRL) {
            ctrl_pressed = false;
        } else if (key == KEY_LEFT_ALT) {
            alt_pressed = false;
        }
    } else {
        key_states[key] = true;
        
        if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT) {
            shift_pressed = true;
        } else if (key == KEY_LEFT_CTRL) {
            ctrl_pressed = true;
        } else if (key == KEY_LEFT_ALT) {
            alt_pressed = true;
        }
    }
    
    if (user_callback != NULL) {
        key_event_t event;
        event.scancode = key;
        event.pressed = !released;
        event.shift = shift_pressed;
        event.ctrl = ctrl_pressed;
        event.alt = alt_pressed;
        event.ascii = keyboard_scancode_to_ascii(key, shift_pressed);
        user_callback(&event);
    }
}

void keyboard_init(void) {
    for (int i = 0; i < 256; i++) {
        key_states[i] = false;
    }
    
    register_interrupt_handler(IRQ1, keyboard_handler);
}

void keyboard_set_callback(keyboard_callback_t callback) {
    user_callback = callback;
}

char keyboard_scancode_to_ascii(uint8_t scancode, bool shift) {
    if (scancode >= sizeof(scancode_to_ascii_lower)) {
        return 0;
    }
    
    if (shift) {
        return scancode_to_ascii_upper[scancode];
    }
    return scancode_to_ascii_lower[scancode];
}

bool keyboard_is_key_pressed(uint8_t scancode) {
    return key_states[scancode & 0x7F];
}
