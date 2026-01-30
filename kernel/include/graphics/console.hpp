#pragma once

#include "../types.hpp"
#include "framebuffer.hpp"
#include "font.hpp"

namespace sertos::graphics {

class Console {
public:
    static void initialize();
    static void clear();
    
    static void putChar(char c);
    static void print(const char* str);
    static void println(const char* str);
    
    static void printHex(u64 value);
    static void printDec(u64 value);
    static void printBin(u64 value);
    
    static void setForeground(Color color);
    static void setBackground(Color color);
    
    static void setCursor(u32 col, u32 row);
    static u32 cursorCol();
    static u32 cursorRow();
    static u32 columns();
    static u32 rows();

private:
    static void scroll();
    static void newLine();
    static void drawChar(u32 col, u32 row, char c);
    
    static u32 sCursorCol;
    static u32 sCursorRow;
    static u32 sColumns;
    static u32 sRows;
    static Color sForeground;
    static Color sBackground;
    static bool sInitialized;
};

}
