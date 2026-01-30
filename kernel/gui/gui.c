#include "../include/gui.h"
#include "../include/vga.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
#include "../include/string.h"

static desktop_t desktop;
static uint8_t last_seconds = 255;

static void draw_desktop_background(void) {
    vga_draw_rect_filled(0, 0, VGA_WIDTH, VGA_HEIGHT - TASKBAR_HEIGHT, DESKTOP_COLOR);
}

static void draw_taskbar(void) {
    taskbar_t* tb = &desktop.taskbar;
    
    vga_draw_rect_filled(tb->bounds.x, tb->bounds.y, 
                         tb->bounds.width, tb->bounds.height, TASKBAR_COLOR);
    
    vga_draw_line(0, tb->bounds.y, VGA_WIDTH - 1, tb->bounds.y, TASKBAR_HIGHLIGHT);
    
    uint8_t start_bg = tb->start_button_pressed ? STARTMENU_COLOR : 
                       (tb->start_button_hovered ? TASKBAR_HIGHLIGHT : TASKBAR_COLOR);
    
    vga_draw_rect_filled(2, tb->bounds.y + 2, START_BUTTON_WIDTH, START_BUTTON_HEIGHT, start_bg);
    vga_draw_rect(2, tb->bounds.y + 2, START_BUTTON_WIDTH, START_BUTTON_HEIGHT, 15);
    
    vga_draw_string(8, tb->bounds.y + 6, "Start", 15, start_bg);
    
    int clock_x = VGA_WIDTH - 60;
    vga_draw_rect_filled(clock_x, tb->bounds.y + 2, 56, START_BUTTON_HEIGHT, TASKBAR_COLOR);
    vga_draw_string(clock_x + 4, tb->bounds.y + 6, tb->clock_text, 15, TASKBAR_COLOR);
}

static void draw_start_menu(void) {
    if (!desktop.start_menu.visible) {
        return;
    }
    
    start_menu_t* sm = &desktop.start_menu;
    
    vga_draw_rect_filled(sm->bounds.x, sm->bounds.y, 
                         sm->bounds.width, sm->bounds.height, STARTMENU_COLOR);
    
    vga_draw_rect(sm->bounds.x, sm->bounds.y, 
                  sm->bounds.width, sm->bounds.height, STARTMENU_BORDER);
    
    vga_draw_rect_filled(sm->bounds.x + 2, sm->bounds.y + 2, 
                         20, sm->bounds.height - 4, 1);
    
    vga_draw_string(sm->bounds.x + 6, sm->bounds.y + sm->bounds.height - 70, 
                    "S", 15, 1);
    vga_draw_string(sm->bounds.x + 6, sm->bounds.y + sm->bounds.height - 60, 
                    "e", 15, 1);
    vga_draw_string(sm->bounds.x + 6, sm->bounds.y + sm->bounds.height - 50, 
                    "r", 15, 1);
    vga_draw_string(sm->bounds.x + 6, sm->bounds.y + sm->bounds.height - 40, 
                    "t", 15, 1);
    vga_draw_string(sm->bounds.x + 6, sm->bounds.y + sm->bounds.height - 30, 
                    "O", 15, 1);
    vga_draw_string(sm->bounds.x + 6, sm->bounds.y + sm->bounds.height - 20, 
                    "S", 15, 1);
    
    vga_draw_line(sm->bounds.x + 24, sm->bounds.y + 2, 
                  sm->bounds.x + 24, sm->bounds.y + sm->bounds.height - 3, STARTMENU_BORDER);
    
    vga_draw_string(sm->bounds.x + 30, sm->bounds.y + 10, "Programs", 7, STARTMENU_COLOR);
    vga_draw_line(sm->bounds.x + 26, sm->bounds.y + 22, 
                  sm->bounds.x + sm->bounds.width - 4, sm->bounds.y + 22, STARTMENU_BORDER);
    
    vga_draw_string(sm->bounds.x + 30, sm->bounds.y + 30, "Documents", 7, STARTMENU_COLOR);
    vga_draw_line(sm->bounds.x + 26, sm->bounds.y + 42, 
                  sm->bounds.x + sm->bounds.width - 4, sm->bounds.y + 42, STARTMENU_BORDER);
    
    vga_draw_string(sm->bounds.x + 30, sm->bounds.y + 50, "Settings", 7, STARTMENU_COLOR);
    vga_draw_line(sm->bounds.x + 26, sm->bounds.y + 62, 
                  sm->bounds.x + sm->bounds.width - 4, sm->bounds.y + 62, STARTMENU_BORDER);
    
    vga_draw_string(sm->bounds.x + 30, sm->bounds.y + 70, "Find", 7, STARTMENU_COLOR);
    vga_draw_line(sm->bounds.x + 26, sm->bounds.y + 82, 
                  sm->bounds.x + sm->bounds.width - 4, sm->bounds.y + 82, STARTMENU_BORDER);
    
    vga_draw_string(sm->bounds.x + 30, sm->bounds.y + 90, "Help", 7, STARTMENU_COLOR);
    vga_draw_line(sm->bounds.x + 26, sm->bounds.y + 102, 
                  sm->bounds.x + sm->bounds.width - 4, sm->bounds.y + 102, STARTMENU_BORDER);
    
    vga_draw_string(sm->bounds.x + 30, sm->bounds.y + 108, "Shut Down", 7, STARTMENU_COLOR);
}

static void update_clock(void) {
    uint8_t hours = rtc_get_hours();
    uint8_t minutes = rtc_get_minutes();
    uint8_t seconds = rtc_get_seconds();
    
    if (seconds != last_seconds) {
        last_seconds = seconds;
        desktop.needs_redraw = true;
        
        char* clock = desktop.taskbar.clock_text;
        
        clock[0] = '0' + (hours / 10);
        clock[1] = '0' + (hours % 10);
        clock[2] = ':';
        clock[3] = '0' + (minutes / 10);
        clock[4] = '0' + (minutes % 10);
        clock[5] = ':';
        clock[6] = '0' + (seconds / 10);
        clock[7] = '0' + (seconds % 10);
        clock[8] = '\0';
    }
}

void gui_init(void) {
    desktop.taskbar.bounds.x = 0;
    desktop.taskbar.bounds.y = VGA_HEIGHT - TASKBAR_HEIGHT;
    desktop.taskbar.bounds.width = VGA_WIDTH;
    desktop.taskbar.bounds.height = TASKBAR_HEIGHT;
    desktop.taskbar.bounds.visible = true;
    desktop.taskbar.start_button_hovered = false;
    desktop.taskbar.start_button_pressed = false;
    strcpy(desktop.taskbar.clock_text, "00:00:00");
    
    desktop.start_menu.bounds.x = 2;
    desktop.start_menu.bounds.y = VGA_HEIGHT - TASKBAR_HEIGHT - STARTMENU_HEIGHT;
    desktop.start_menu.bounds.width = STARTMENU_WIDTH;
    desktop.start_menu.bounds.height = STARTMENU_HEIGHT;
    desktop.start_menu.visible = false;
    desktop.start_menu.hovered = false;
    
    desktop.needs_redraw = true;
    
    vga_init();
    
    gui_draw();
}

void gui_update(void) {
    update_clock();
}

void gui_draw(void) {
    if (!desktop.needs_redraw) {
        return;
    }
    
    draw_desktop_background();
    draw_taskbar();
    draw_start_menu();
    
    desktop.needs_redraw = false;
}

void gui_handle_key(uint8_t scancode, bool pressed) {
    if (pressed && scancode == KEY_ESCAPE) {
        if (desktop.start_menu.visible) {
            desktop.start_menu.visible = false;
            desktop.needs_redraw = true;
        }
    }
    
    if (pressed && scancode == KEY_SPACE) {
        gui_toggle_start_menu();
    }
}

void gui_toggle_start_menu(void) {
    desktop.start_menu.visible = !desktop.start_menu.visible;
    desktop.needs_redraw = true;
}

desktop_t* gui_get_desktop(void) {
    return &desktop;
}
