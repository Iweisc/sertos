#include "../include/mouse.h"
#include "../include/idt.h"
#include "../include/ports.h"

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

static int32_t mouse_x = 160;
static int32_t mouse_y = 100;
static int32_t max_x = 320;
static int32_t max_y = 200;
static bool left_button = false;
static bool right_button = false;
static bool middle_button = false;
static mouse_callback_t user_callback = NULL;

static uint8_t mouse_cycle = 0;
static int8_t mouse_bytes[3];

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(PS2_STATUS_PORT) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(PS2_STATUS_PORT) & 2) == 0) return;
        }
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0xD4);
    mouse_wait(1);
    outb(PS2_DATA_PORT, data);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(PS2_DATA_PORT);
}

static void mouse_handler(registers_t* regs) {
    (void)regs;
    
    uint8_t status = inb(PS2_STATUS_PORT);
    if (!(status & 0x20)) {
        inb(PS2_DATA_PORT);
        return;
    }
    
    uint8_t data = inb(PS2_DATA_PORT);
    
    switch (mouse_cycle) {
        case 0:
            mouse_bytes[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_bytes[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_bytes[2] = data;
            mouse_cycle = 0;
            
            if (!(mouse_bytes[0] & 0x08)) {
                break;
            }
            
            int32_t delta_x = mouse_bytes[1];
            int32_t delta_y = mouse_bytes[2];
            
            if (mouse_bytes[0] & 0x10) {
                delta_x |= 0xFFFFFF00;
            }
            if (mouse_bytes[0] & 0x20) {
                delta_y |= 0xFFFFFF00;
            }
            
            mouse_x += delta_x;
            mouse_y -= delta_y;
            
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= max_x) mouse_x = max_x - 1;
            if (mouse_y >= max_y) mouse_y = max_y - 1;
            
            left_button = (mouse_bytes[0] & MOUSE_LEFT_BUTTON) != 0;
            right_button = (mouse_bytes[0] & MOUSE_RIGHT_BUTTON) != 0;
            middle_button = (mouse_bytes[0] & MOUSE_MIDDLE_BUTTON) != 0;
            
            if (user_callback != NULL) {
                mouse_event_t event;
                event.x = mouse_x;
                event.y = mouse_y;
                event.delta_x = delta_x;
                event.delta_y = -delta_y;
                event.left_button = left_button;
                event.right_button = right_button;
                event.middle_button = middle_button;
                user_callback(&event);
            }
            break;
    }
}

void mouse_init(void) {
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0xA8);
    
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0x20);
    mouse_wait(0);
    uint8_t status = inb(PS2_DATA_PORT);
    status |= 2;
    
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0x60);
    mouse_wait(1);
    outb(PS2_DATA_PORT, status);
    
    mouse_write(0xF6);
    mouse_read();
    
    mouse_write(0xF4);
    mouse_read();
    
    register_interrupt_handler(IRQ12, mouse_handler);
}

void mouse_set_callback(mouse_callback_t callback) {
    user_callback = callback;
}

void mouse_get_position(int32_t* x, int32_t* y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}

bool mouse_is_left_pressed(void) {
    return left_button;
}

bool mouse_is_right_pressed(void) {
    return right_button;
}

bool mouse_is_middle_pressed(void) {
    return middle_button;
}

void mouse_set_bounds(int32_t new_max_x, int32_t new_max_y) {
    max_x = new_max_x;
    max_y = new_max_y;
    
    if (mouse_x >= max_x) mouse_x = max_x - 1;
    if (mouse_y >= max_y) mouse_y = max_y - 1;
}
