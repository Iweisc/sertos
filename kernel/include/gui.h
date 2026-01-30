#ifndef GUI_H
#define GUI_H

#include "types.h"

#define DESKTOP_COLOR 16
#define TASKBAR_COLOR 17
#define TASKBAR_HIGHLIGHT 18
#define STARTMENU_COLOR 19
#define STARTMENU_BORDER 20

#define TASKBAR_HEIGHT 20
#define STARTMENU_WIDTH 100
#define STARTMENU_HEIGHT 120

#define START_BUTTON_WIDTH 50
#define START_BUTTON_HEIGHT 16

typedef struct {
    int x;
    int y;
    int width;
    int height;
    bool visible;
} rect_t;

typedef struct {
    rect_t bounds;
    bool visible;
    bool hovered;
} start_menu_t;

typedef struct {
    rect_t bounds;
    bool start_button_hovered;
    bool start_button_pressed;
    char clock_text[16];
} taskbar_t;

typedef struct {
    taskbar_t taskbar;
    start_menu_t start_menu;
    bool needs_redraw;
} desktop_t;

void gui_init(void);
void gui_update(void);
void gui_draw(void);
void gui_handle_key(uint8_t scancode, bool pressed);
void gui_handle_mouse(int32_t x, int32_t y, bool left_button, bool right_button);
void gui_toggle_start_menu(void);

desktop_t* gui_get_desktop(void);

#endif
