#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

#define MOUSE_DATA_PORT    0x60
#define MOUSE_STATUS_PORT  0x64
#define MOUSE_COMMAND_PORT 0x64

#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

typedef struct {
    int32_t x;
    int32_t y;
    int32_t delta_x;
    int32_t delta_y;
    bool left_button;
    bool right_button;
    bool middle_button;
} mouse_event_t;

typedef void (*mouse_callback_t)(mouse_event_t*);

void mouse_init(void);
void mouse_set_callback(mouse_callback_t callback);
void mouse_get_position(int32_t* x, int32_t* y);
bool mouse_is_left_pressed(void);
bool mouse_is_right_pressed(void);
bool mouse_is_middle_pressed(void);
void mouse_set_bounds(int32_t max_x, int32_t max_y);

#endif
