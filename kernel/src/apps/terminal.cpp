#include "../../include/apps/terminal.hpp"
#include "../../include/graphics/framebuffer.hpp"
#include "../../include/fs/sertfs.hpp"

namespace sertos::apps {

using wm::WindowManager;
using wm::Window;
using wm::Rect;
using graphics::Color;
using graphics::Font;
using graphics::Framebuffer;

TerminalApp* TerminalApp::sActiveTerminal = nullptr;

TerminalApp::TerminalApp()
    : mCursorCol(0), mCursorRow(0), mColumns(80), mRows(25),
      mForeground(200, 200, 200), mBackground(30, 30, 30),
      mInputPos(0), mInitialized(false) {
    mInputBuffer[0] = '\0';
}

void TerminalApp::shellPrintCallback(const char* str) {
    if (sActiveTerminal) {
        sActiveTerminal->print(str);
    }
}

void TerminalApp::shellSetColorCallback(Color color) {
    if (sActiveTerminal) {
        sActiveTerminal->setForeground(color);
    }
}

void TerminalApp::shellClearCallback() {
    if (sActiveTerminal) {
        sActiveTerminal->clear();
    }
}

void TerminalApp::initializeBuffer() {
    if (mInitialized) return;

    Window* win = WindowManager::getWindow(mWindowId);
    if (win) {
        Rect client = win->clientArea();
        mColumns = client.width / Font::CHAR_WIDTH;
        mRows = client.height / (Font::CHAR_HEIGHT + 2);
        if (mColumns > MAX_COLS) mColumns = MAX_COLS;
        if (mRows > MAX_ROWS) mRows = MAX_ROWS;
    }

    for (u32 r = 0; r < MAX_ROWS; r++) {
        for (u32 c = 0; c < MAX_COLS; c++) {
            mBuffer[r][c] = '\0';
        }
    }

    mCursorCol = 0;
    mCursorRow = 0;

    setForeground(Color(0, 200, 200));
    println("SertOS Terminal v1.0");
    println("Type 'help' for available commands.");
    setForeground(Color(200, 200, 200));
    println("");

    printPrompt();
    mInitialized = true;
}

void TerminalApp::clear() {
    for (u32 r = 0; r < mRows; r++) {
        for (u32 c = 0; c < mColumns; c++) {
            mBuffer[r][c] = '\0';
        }
    }
    mCursorCol = 0;
    mCursorRow = 0;
}

void TerminalApp::putChar(char c) {
    if (c == '\n') {
        newLine();
        return;
    }

    if (c == '\r') {
        mCursorCol = 0;
        return;
    }

    if (c == '\b') {
        if (mCursorCol > 0) {
            mCursorCol--;
            mBuffer[mCursorRow][mCursorCol] = '\0';
        }
        return;
    }

    if (c == '\t') {
        u32 spaces = 4 - (mCursorCol % 4);
        for (u32 i = 0; i < spaces && mCursorCol < mColumns; i++) {
            mBuffer[mCursorRow][mCursorCol] = ' ';
            mCursorCol++;
        }
        return;
    }

    if (mCursorCol >= mColumns) {
        newLine();
    }

    mBuffer[mCursorRow][mCursorCol] = c;
    mCursorCol++;
}

void TerminalApp::print(const char* str) {
    while (*str) {
        putChar(*str++);
    }
}

void TerminalApp::println(const char* str) {
    print(str);
    newLine();
}

void TerminalApp::newLine() {
    mCursorCol = 0;
    mCursorRow++;

    if (mCursorRow >= mRows) {
        scroll();
        mCursorRow = mRows - 1;
    }
}

void TerminalApp::scroll() {
    for (u32 r = 0; r < mRows - 1; r++) {
        for (u32 c = 0; c < mColumns; c++) {
            mBuffer[r][c] = mBuffer[r + 1][c];
        }
    }

    for (u32 c = 0; c < mColumns; c++) {
        mBuffer[mRows - 1][c] = '\0';
    }
}

void TerminalApp::render() {
    if (!mInitialized) {
        initializeBuffer();
    }

    Window* win = WindowManager::getWindow(mWindowId);
    if (!win) return;

    Rect client = win->clientArea();

    Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(client.y),
                          client.width, client.height, mBackground);

    for (u32 r = 0; r < mRows; r++) {
        for (u32 c = 0; c < mColumns; c++) {
            char ch = mBuffer[r][c];
            if (ch != '\0' && ch != ' ') {
                u32 textX = c * Font::CHAR_WIDTH + 4;
                u32 textY = r * (Font::CHAR_HEIGHT + 2) + 4;

                const u8* glyph = Font::getGlyph(ch);
                for (u32 row = 0; row < Font::CHAR_HEIGHT; row++) {
                    for (u32 col = 0; col < Font::CHAR_WIDTH; col++) {
                        if (glyph[row] & (0x80 >> col)) {
                            i32 px = client.x + static_cast<i32>(textX + col);
                            i32 py = client.y + static_cast<i32>(textY + row);
                            if (px >= 0 && py >= 0) {
                                Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), mForeground);
                            }
                        }
                    }
                }
            }
        }
    }

    u32 cursorX = mCursorCol * Font::CHAR_WIDTH + 4;
    u32 cursorY = mCursorRow * (Font::CHAR_HEIGHT + 2) + 4;
    Framebuffer::fillRect(static_cast<u32>(client.x) + cursorX,
                          static_cast<u32>(client.y) + cursorY,
                          Font::CHAR_WIDTH, Font::CHAR_HEIGHT, mForeground);
}

void TerminalApp::handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) {
    (void)alt;
    (void)shift;

    if (!mInitialized) {
        initializeBuffer();
    }

    if (ctrl) {
        if (code == input::KeyCode::C) {
            println("^C");
            mInputBuffer[0] = '\0';
            mInputPos = 0;
            printPrompt();
        } else if (code == input::KeyCode::L) {
            clear();
            printPrompt();
        }
        return;
    }

    switch (code) {
        case input::KeyCode::Enter:
            println("");
            processInput();
            break;

        case input::KeyCode::Backspace:
            handleBackspace();
            break;

        default:
            if (ascii >= 32 && ascii < 127) {
                handleCharacter(static_cast<char>(ascii));
            }
            break;
    }
}

void TerminalApp::handleMouseClick(i32 x, i32 y, bool doubleClick) {
    (void)x;
    (void)y;
    (void)doubleClick;
}

void TerminalApp::printPrompt() {
    setForeground(Color(0, 200, 0));
    print("sertos");
    setForeground(Color(200, 200, 200));
    print(":");
    setForeground(Color(80, 150, 255));
    print(fs::SertFs::currentDirectory());
    setForeground(Color(200, 200, 200));
    print("$ ");
}

void TerminalApp::processInput() {
    mInputBuffer[mInputPos] = '\0';

    if (mInputPos > 0) {
        sActiveTerminal = this;
        shell::Shell::setOutputCallbacks(shellPrintCallback, nullptr, shellSetColorCallback, shellClearCallback);
        shell::Shell::executeCommand(mInputBuffer);
        shell::Shell::clearOutputCallbacks();
        sActiveTerminal = nullptr;
    }

    mInputBuffer[0] = '\0';
    mInputPos = 0;
    printPrompt();
}

void TerminalApp::handleBackspace() {
    if (mInputPos > 0) {
        mInputPos--;
        mInputBuffer[mInputPos] = '\0';
        putChar('\b');
    }
}

void TerminalApp::handleCharacter(char c) {
    if (mInputPos < MAX_INPUT_LENGTH - 1) {
        mInputBuffer[mInputPos++] = c;
        mInputBuffer[mInputPos] = '\0';
        putChar(c);
    }
}

}
