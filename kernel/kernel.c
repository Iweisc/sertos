#include "include/types.h"
#include "include/vga.h"
#include "include/idt.h"
#include "include/keyboard.h"
#include "include/mouse.h"
#include "include/timer.h"
#include "include/heap.h"
#include "include/gui.h"
#include "include/string.h"

static void keyboard_event_handler(key_event_t* event) {
    gui_handle_key(event->scancode, event->pressed);
    
    if (event->pressed) {
        desktop_t* desktop = gui_get_desktop();
        desktop->needs_redraw = true;
    }
}

static void mouse_event_handler(mouse_event_t* event) {
    gui_handle_mouse(event->x, event->y, event->left_button, event->right_button);
    
    desktop_t* desktop = gui_get_desktop();
    desktop->needs_redraw = true;
}

static void timer_tick_handler(uint32_t tick) {
    (void)tick;
    gui_update();
    gui_draw();
}

void kernel_main(void) {
    heap_init();
    idt_init();
    
    timer_init(100);
    timer_set_callback(timer_tick_handler);
    
    keyboard_init();
    keyboard_set_callback(keyboard_event_handler);
    
    mouse_init();
    mouse_set_callback(mouse_event_handler);
    mouse_set_bounds(320, 200);
    
    gui_init();
    
    while (1) {
        __asm__ volatile("hlt");
    }
}
