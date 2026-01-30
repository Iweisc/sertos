#include "../../include/graphics/framebuffer.hpp"

namespace sertos::graphics {

u64 Framebuffer::sAddress = 0;
u32 Framebuffer::sWidth = 0;
u32 Framebuffer::sHeight = 0;
u32 Framebuffer::sPitch = 0;
u32 Framebuffer::sBpp = 0;
u8 Framebuffer::sRedShift = 0;
u8 Framebuffer::sGreenShift = 0;
u8 Framebuffer::sBlueShift = 0;
bool Framebuffer::sInitialized = false;

void Framebuffer::initialize(const boot::FramebufferInfo& info) {
    sAddress = info.address;
    sWidth = info.width;
    sHeight = info.height;
    sPitch = info.pitch;
    sBpp = info.bpp;
    sRedShift = info.redMaskShift;
    sGreenShift = info.greenMaskShift;
    sBlueShift = info.blueMaskShift;
    sInitialized = true;
}

void Framebuffer::clear(Color color) {
    if (!sInitialized) return;
    fillRect(0, 0, sWidth, sHeight, color);
}

void Framebuffer::putPixel(u32 x, u32 y, Color color) {
    if (!sInitialized || x >= sWidth || y >= sHeight) return;
    
    u32 pixel = colorToPixel(color);
    u32* fb = reinterpret_cast<u32*>(sAddress);
    fb[y * (sPitch / 4) + x] = pixel;
}

void Framebuffer::drawRect(u32 x, u32 y, u32 width, u32 height, Color color) {
    if (!sInitialized) return;
    
    for (u32 i = x; i < x + width && i < sWidth; i++) {
        putPixel(i, y, color);
        putPixel(i, y + height - 1, color);
    }
    
    for (u32 i = y; i < y + height && i < sHeight; i++) {
        putPixel(x, i, color);
        putPixel(x + width - 1, i, color);
    }
}

void Framebuffer::fillRect(u32 x, u32 y, u32 width, u32 height, Color color) {
    if (!sInitialized) return;
    
    u32 pixel = colorToPixel(color);
    u32* fb = reinterpret_cast<u32*>(sAddress);
    
    u32 endX = (x + width > sWidth) ? sWidth : x + width;
    u32 endY = (y + height > sHeight) ? sHeight : y + height;
    
    for (u32 py = y; py < endY; py++) {
        for (u32 px = x; px < endX; px++) {
            fb[py * (sPitch / 4) + px] = pixel;
        }
    }
}

void Framebuffer::drawLine(u32 x1, u32 y1, u32 x2, u32 y2, Color color) {
    if (!sInitialized) return;
    
    i32 dx = static_cast<i32>(x2) - static_cast<i32>(x1);
    i32 dy = static_cast<i32>(y2) - static_cast<i32>(y1);
    
    i32 absDx = dx < 0 ? -dx : dx;
    i32 absDy = dy < 0 ? -dy : dy;
    
    i32 sx = dx < 0 ? -1 : 1;
    i32 sy = dy < 0 ? -1 : 1;
    
    i32 err = absDx - absDy;
    
    i32 x = static_cast<i32>(x1);
    i32 y = static_cast<i32>(y1);
    
    while (true) {
        if (x >= 0 && x < static_cast<i32>(sWidth) && 
            y >= 0 && y < static_cast<i32>(sHeight)) {
            putPixel(static_cast<u32>(x), static_cast<u32>(y), color);
        }
        
        if (x == static_cast<i32>(x2) && y == static_cast<i32>(y2)) break;
        
        i32 e2 = 2 * err;
        if (e2 > -absDy) {
            err -= absDy;
            x += sx;
        }
        if (e2 < absDx) {
            err += absDx;
            y += sy;
        }
    }
}

u32 Framebuffer::width() { return sWidth; }
u32 Framebuffer::height() { return sHeight; }
u32 Framebuffer::pitch() { return sPitch; }
u32 Framebuffer::bpp() { return sBpp; }
u64 Framebuffer::address() { return sAddress; }
bool Framebuffer::isInitialized() { return sInitialized; }

u32 Framebuffer::colorToPixel(Color color) {
    return (static_cast<u32>(color.r) << sRedShift) |
           (static_cast<u32>(color.g) << sGreenShift) |
           (static_cast<u32>(color.b) << sBlueShift);
}

}
