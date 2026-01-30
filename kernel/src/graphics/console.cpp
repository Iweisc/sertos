#include "../../include/graphics/console.hpp"

namespace sertos::graphics {

u32 Console::sCursorCol = 0;
u32 Console::sCursorRow = 0;
u32 Console::sColumns = 0;
u32 Console::sRows = 0;
Color Console::sForeground = Color::white();
Color Console::sBackground = Color::black();
bool Console::sInitialized = false;

void Console::initialize() {
    if (!Framebuffer::isInitialized()) {
        return;
    }
    
    sColumns = Framebuffer::width() / Font::charWidth();
    sRows = Framebuffer::height() / Font::charHeight();
    sCursorCol = 0;
    sCursorRow = 0;
    sInitialized = true;
    
    clear();
}

void Console::clear() {
    if (!sInitialized) return;
    
    Framebuffer::clear(sBackground);
    sCursorCol = 0;
    sCursorRow = 0;
}

void Console::putChar(char c) {
    if (!sInitialized) return;
    
    if (c == '\n') {
        newLine();
        return;
    }
    
    if (c == '\r') {
        sCursorCol = 0;
        return;
    }
    
    if (c == '\t') {
        u32 spaces = 4 - (sCursorCol % 4);
        for (u32 i = 0; i < spaces; i++) {
            putChar(' ');
        }
        return;
    }
    
    if (c == '\b') {
        if (sCursorCol > 0) {
            sCursorCol--;
            drawChar(sCursorCol, sCursorRow, ' ');
        }
        return;
    }
    
    if (sCursorCol >= sColumns) {
        newLine();
    }
    
    drawChar(sCursorCol, sCursorRow, c);
    sCursorCol++;
}

void Console::print(const char* str) {
    if (!str) return;
    
    while (*str) {
        putChar(*str);
        str++;
    }
}

void Console::println(const char* str) {
    print(str);
    newLine();
}

void Console::printHex(u64 value) {
    print("0x");
    
    char buffer[17];
    buffer[16] = '\0';
    
    for (int i = 15; i >= 0; i--) {
        u8 nibble = value & 0xF;
        buffer[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        value >>= 4;
    }
    
    const char* start = buffer;
    while (*start == '0' && *(start + 1) != '\0') {
        start++;
    }
    
    print(start);
}

void Console::printDec(u64 value) {
    if (value == 0) {
        putChar('0');
        return;
    }
    
    char buffer[21];
    buffer[20] = '\0';
    int i = 19;
    
    while (value > 0 && i >= 0) {
        buffer[i] = '0' + (value % 10);
        value /= 10;
        i--;
    }
    
    print(&buffer[i + 1]);
}

void Console::printBin(u64 value) {
    print("0b");
    
    char buffer[65];
    buffer[64] = '\0';
    
    for (int i = 63; i >= 0; i--) {
        buffer[i] = (value & 1) ? '1' : '0';
        value >>= 1;
    }
    
    const char* start = buffer;
    while (*start == '0' && *(start + 1) != '\0') {
        start++;
    }
    
    print(start);
}

void Console::setForeground(Color color) {
    sForeground = color;
}

void Console::setBackground(Color color) {
    sBackground = color;
}

void Console::setCursor(u32 col, u32 row) {
    if (col < sColumns) sCursorCol = col;
    if (row < sRows) sCursorRow = row;
}

u32 Console::cursorCol() { return sCursorCol; }
u32 Console::cursorRow() { return sCursorRow; }
u32 Console::columns() { return sColumns; }
u32 Console::rows() { return sRows; }

void Console::scroll() {
    if (!sInitialized) return;
    
    u32 charHeight = Font::charHeight();
    u32 fbWidth = Framebuffer::width();
    u32 fbHeight = Framebuffer::height();
    u32 pitch = Framebuffer::pitch();
    u64 fbAddr = Framebuffer::address();
    
    u32* fb = reinterpret_cast<u32*>(fbAddr);
    u32 pixelsPerRow = pitch / 4;
    
    for (u32 y = 0; y < fbHeight - charHeight; y++) {
        for (u32 x = 0; x < fbWidth; x++) {
            fb[y * pixelsPerRow + x] = fb[(y + charHeight) * pixelsPerRow + x];
        }
    }
    
    u32 bgPixel = (static_cast<u32>(sBackground.r) << 16) |
                  (static_cast<u32>(sBackground.g) << 8) |
                  static_cast<u32>(sBackground.b);
    
    for (u32 y = fbHeight - charHeight; y < fbHeight; y++) {
        for (u32 x = 0; x < fbWidth; x++) {
            fb[y * pixelsPerRow + x] = bgPixel;
        }
    }
}

void Console::newLine() {
    sCursorCol = 0;
    sCursorRow++;
    
    if (sCursorRow >= sRows) {
        scroll();
        sCursorRow = sRows - 1;
    }
}

void Console::drawChar(u32 col, u32 row, char c) {
    if (!sInitialized) return;
    
    const u8* glyph = Font::getGlyph(c);
    u32 charWidth = Font::charWidth();
    u32 charHeight = Font::charHeight();
    
    u32 startX = col * charWidth;
    u32 startY = row * charHeight;
    
    for (u32 y = 0; y < charHeight; y++) {
        u8 glyphRow = glyph[y];
        for (u32 x = 0; x < charWidth; x++) {
            bool pixel = (glyphRow >> (7 - x)) & 1;
            Framebuffer::putPixel(startX + x, startY + y, pixel ? sForeground : sBackground);
        }
    }
}

}
