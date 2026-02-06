#pragma once

#include "app.hpp"
#include "../wm/wm.hpp"
#include "../shell/shell.hpp"
#include "../graphics/font.hpp"

namespace sertos::apps {

class TerminalApp : public App {
public:
    TerminalApp();
    ~TerminalApp() override = default;

    void render() override;
    void handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) override;
    void handleMouseClick(i32 x, i32 y, bool doubleClick) override;

    void putChar(char c);
    void print(const char* str);
    void println(const char* str);
    void setForeground(graphics::Color color) { mForeground = color; }
    void clear();

    static TerminalApp* sActiveTerminal;

private:
    static constexpr usize MAX_INPUT_LENGTH = 256;
    static constexpr u32 MAX_COLS = 100;
    static constexpr u32 MAX_ROWS = 50;

    char mBuffer[MAX_ROWS][MAX_COLS];
    u32 mCursorCol;
    u32 mCursorRow;
    u32 mColumns;
    u32 mRows;
    graphics::Color mForeground;
    graphics::Color mBackground;

    char mInputBuffer[MAX_INPUT_LENGTH];
    usize mInputPos;
    bool mInitialized;

    void initializeBuffer();
    void printPrompt();
    void processInput();
    void handleBackspace();
    void handleCharacter(char c);

    void newLine();
    void scroll();

    static void shellPrintCallback(const char* str);
    static void shellSetColorCallback(graphics::Color color);
    static void shellClearCallback();
};

}
