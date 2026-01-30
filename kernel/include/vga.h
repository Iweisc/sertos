#ifndef VGA_H
#define VGA_H

#include "types.h"

#define VGA_WIDTH 320
#define VGA_HEIGHT 200
#define VGA_MEMORY 0xA0000

#define VGA_TEXT_WIDTH 80
#define VGA_TEXT_HEIGHT 25
#define VGA_TEXT_MEMORY 0xB8000

#define VGA_AC_INDEX 0x3C0
#define VGA_AC_WRITE 0x3C0
#define VGA_AC_READ 0x3C1
#define VGA_MISC_WRITE 0x3C2
#define VGA_SEQ_INDEX 0x3C4
#define VGA_SEQ_DATA 0x3C5
#define VGA_DAC_READ_INDEX 0x3C7
#define VGA_DAC_WRITE_INDEX 0x3C8
#define VGA_DAC_DATA 0x3C9
#define VGA_MISC_READ 0x3CC
#define VGA_GC_INDEX 0x3CE
#define VGA_GC_DATA 0x3CF
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5
#define VGA_INSTAT_READ 0x3DA

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

void vga_init(void);
void vga_set_mode_13h(void);
void vga_set_text_mode(void);
void vga_clear_screen(uint8_t color);
void vga_put_pixel(int x, int y, uint8_t color);
uint8_t vga_get_pixel(int x, int y);
void vga_draw_rect(int x, int y, int width, int height, uint8_t color);
void vga_draw_rect_filled(int x, int y, int width, int height, uint8_t color);
void vga_draw_line(int x1, int y1, int x2, int y2, uint8_t color);
void vga_draw_char(int x, int y, char c, uint8_t fg, uint8_t bg);
void vga_draw_string(int x, int y, const char* str, uint8_t fg, uint8_t bg);
void vga_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void vga_swap_buffers(void);

void text_clear_screen(void);
void text_print(const char* str);
void text_print_at(const char* str, int x, int y, uint8_t color);
void text_set_cursor(int x, int y);

#endif
