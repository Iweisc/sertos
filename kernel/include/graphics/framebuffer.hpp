#pragma once

#include "../types.hpp"
#include "../../../boot/include/bootinfo.hpp"

namespace sertos::graphics {

struct Color {
    u8 r, g, b, a;
    
    constexpr Color() : r(0), g(0), b(0), a(255) {}
    
    constexpr Color(u8 red, u8 green, u8 blue, u8 alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
    
    static constexpr Color black() { return Color(0, 0, 0); }
    static constexpr Color white() { return Color(255, 255, 255); }
    static constexpr Color red() { return Color(255, 0, 0); }
    static constexpr Color green() { return Color(0, 255, 0); }
    static constexpr Color blue() { return Color(0, 0, 255); }
    static constexpr Color yellow() { return Color(255, 255, 0); }
    static constexpr Color cyan() { return Color(0, 255, 255); }
    static constexpr Color magenta() { return Color(255, 0, 255); }
    static constexpr Color gray() { return Color(128, 128, 128); }
    static constexpr Color darkGray() { return Color(64, 64, 64); }
    static constexpr Color lightGray() { return Color(192, 192, 192); }
};

class Framebuffer {
public:
    static void initialize(const boot::FramebufferInfo& info);
    
    static void clear(Color color = Color::black());
    static void putPixel(u32 x, u32 y, Color color);
    static void drawRect(u32 x, u32 y, u32 width, u32 height, Color color);
    static void fillRect(u32 x, u32 y, u32 width, u32 height, Color color);
    static void drawLine(u32 x1, u32 y1, u32 x2, u32 y2, Color color);
    
    static u32 width();
    static u32 height();
    static u32 pitch();
    static u32 bpp();
    static u64 address();
    static bool isInitialized();

private:
    static u32 colorToPixel(Color color);
    
    static u64 sAddress;
    static u32 sWidth;
    static u32 sHeight;
    static u32 sPitch;
    static u32 sBpp;
    static u8 sRedShift;
    static u8 sGreenShift;
    static u8 sBlueShift;
    static bool sInitialized;
};

}
