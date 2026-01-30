#pragma once

#include "../types.hpp"

namespace sertos::graphics {

class Font {
public:
    static constexpr u32 CHAR_WIDTH = 8;
    static constexpr u32 CHAR_HEIGHT = 16;
    
    static const u8* getGlyph(char c);
    static u32 charWidth();
    static u32 charHeight();
};

extern const u8 FONT_DATA[256][16];

}
