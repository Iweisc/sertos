#include "../../include/apps/about.hpp"
#include "../../include/graphics/framebuffer.hpp"

namespace sertos::apps {

AboutApp::AboutApp() {
}

void AboutApp::render() {
    Window* win = WindowManager::getWindow(mWindowId);
    if (!win) return;
    
    Rect client = win->clientArea();
    
    i32 logoX = client.x + 20;
    i32 logoY = client.y + 20;
    drawLogo(logoX, logoY, client);
    
    i32 infoX = client.x + 80;
    i32 infoY = client.y + 20;
    drawSystemInfo(infoX, infoY, client);
}

void AboutApp::handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) {
    (void)code;
    (void)ascii;
    (void)ctrl;
    (void)alt;
    (void)shift;
}

void AboutApp::handleMouseClick(i32 x, i32 y, bool doubleClick) {
    (void)x;
    (void)y;
    (void)doubleClick;
}

void AboutApp::drawLogo(i32 x, i32 y, const Rect& clip) {
    Color logoColor(0, 100, 180);
    Color accentColor(0, 150, 220);
    
    i32 clipRight = clip.x + static_cast<i32>(clip.width);
    i32 clipBottom = clip.y + static_cast<i32>(clip.height);
    
    for (u32 row = 0; row < 40; row++) {
        for (u32 col = 0; col < 40; col++) {
            i32 px = x + static_cast<i32>(col);
            i32 py = y + static_cast<i32>(row);
            
            if (px < clip.x || px >= clipRight || py < clip.y || py >= clipBottom) {
                continue;
            }
            
            i32 cx = 20;
            i32 cy = 20;
            i32 dx = static_cast<i32>(col) - cx;
            i32 dy = static_cast<i32>(row) - cy;
            i32 distSq = dx * dx + dy * dy;
            
            if (distSq < 18 * 18 && distSq > 12 * 12) {
                graphics::Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), logoColor);
            } else if (distSq <= 12 * 12 && distSq > 6 * 6) {
                graphics::Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), accentColor);
            } else if (distSq <= 6 * 6) {
                graphics::Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), Color(255, 255, 255));
            }
        }
    }
    
    WindowManager::drawTextClipped(x + 14, y + 14, "S", Color(0, 80, 160), clip);
}

void AboutApp::drawSystemInfo(i32 x, i32 y, const Rect& clip) {
    Color titleColor(0, 80, 160);
    Color textColor(60, 60, 60);
    Color versionColor(100, 100, 100);
    
    WindowManager::drawTextClipped(x, y, "SertOS", titleColor, clip);
    WindowManager::drawTextClipped(x, y + 20, "Version 1.0.0", versionColor, clip);
    
    WindowManager::drawTextClipped(x, y + 50, "A modern operating system", textColor, clip);
    WindowManager::drawTextClipped(x, y + 66, "written in C++ from scratch.", textColor, clip);
    
    WindowManager::drawTextClipped(x, y + 96, "Features:", titleColor, clip);
    WindowManager::drawTextClipped(x, y + 112, "- UEFI Boot", textColor, clip);
    WindowManager::drawTextClipped(x, y + 128, "- Window Manager", textColor, clip);
    WindowManager::drawTextClipped(x, y + 144, "- PS/2 Mouse & Keyboard", textColor, clip);
    WindowManager::drawTextClipped(x, y + 160, "- SertFS Filesystem", textColor, clip);
    WindowManager::drawTextClipped(x, y + 176, "- Shell Commands", textColor, clip);
    
    WindowManager::drawTextClipped(x, y + 206, "(c) 2026 SertOS Project", versionColor, clip);
}

}
