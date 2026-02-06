#include "../../include/apps/text_editor.hpp"
#include "../../include/graphics/framebuffer.hpp"
#include "../../include/fs/sertfs.hpp"

namespace sertos::apps {

using wm::WindowManager;
using wm::Window;
using wm::Rect;
using graphics::Color;
using graphics::Font;
using graphics::Framebuffer;

namespace {

usize strLen(const char* str) {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

void strCopy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void memSet(void* dest, u8 value, usize size) {
    u8* d = static_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

}

TextEditorApp::TextEditorApp()
    : mLineCount(1), mCursorLine(0), mCursorCol(0), mScrollOffset(0),
      mVisibleLines(20), mVisibleCols(80), mModified(false), mInitialized(false),
      mMode(EditorMode::Normal), mInputPos(0),
      mTextColor(220, 220, 220), mBackgroundColor(30, 30, 35),
      mCursorColor(255, 255, 255), mLineNumberColor(100, 100, 120),
      mStatusBarColor(50, 50, 60) {
    mFilename[0] = '\0';
    mInputBuffer[0] = '\0';
}

void TextEditorApp::initializeBuffer() {
    if (mInitialized) return;

    Window* win = WindowManager::getWindow(mWindowId);
    if (win) {
        Rect client = win->clientArea();
        u32 lineNumWidth = lineNumberWidth();
        mVisibleCols = (client.width - lineNumWidth - 8) / Font::CHAR_WIDTH;
        mVisibleLines = (client.height - 24) / (Font::CHAR_HEIGHT + 2);
        if (mVisibleCols > MAX_LINE_LENGTH) mVisibleCols = MAX_LINE_LENGTH;
        if (mVisibleLines > MAX_LINES) mVisibleLines = MAX_LINES;
    }

    for (u32 i = 0; i < MAX_LINES; i++) {
        memSet(mLines[i], 0, MAX_LINE_LENGTH);
    }

    mLineCount = 1;
    mCursorLine = 0;
    mCursorCol = 0;
    mScrollOffset = 0;
    mModified = false;
    mInitialized = true;
}

void TextEditorApp::newFile() {
    for (u32 i = 0; i < MAX_LINES; i++) {
        memSet(mLines[i], 0, MAX_LINE_LENGTH);
    }
    mLineCount = 1;
    mCursorLine = 0;
    mCursorCol = 0;
    mScrollOffset = 0;
    mFilename[0] = '\0';
    mModified = false;
}

void TextEditorApp::loadFile(const char* path) {
    fs::FileHandle handle = fs::SertFs::open(path, fs::SERTFS_O_READ);
    if (!handle.valid) return;

    newFile();
    strCopy(mFilename, path, MAX_FILENAME);

    char buffer[4096];
    i64 bytesRead;
    u32 currentLine = 0;
    u32 currentCol = 0;

    while ((bytesRead = fs::SertFs::read(&handle, buffer, sizeof(buffer))) > 0) {
        for (i64 i = 0; i < bytesRead; i++) {
            char c = buffer[i];
            if (c == '\n') {
                mLines[currentLine][currentCol] = '\0';
                currentLine++;
                currentCol = 0;
                if (currentLine >= MAX_LINES - 1) break;
            } else if (c == '\r') {
                continue;
            } else if (c == '\t') {
                u32 spaces = 4 - (currentCol % 4);
                for (u32 s = 0; s < spaces && currentCol < MAX_LINE_LENGTH - 1; s++) {
                    mLines[currentLine][currentCol++] = ' ';
                }
            } else if (currentCol < MAX_LINE_LENGTH - 1) {
                mLines[currentLine][currentCol++] = c;
            }
        }
        if (currentLine >= MAX_LINES - 1) break;
    }

    mLines[currentLine][currentCol] = '\0';
    mLineCount = currentLine + 1;
    mModified = false;

    fs::SertFs::close(&handle);
}

void TextEditorApp::saveFile() {
    if (mFilename[0] == '\0') {
        mMode = EditorMode::SaveAsPrompt;
        mInputBuffer[0] = '\0';
        mInputPos = 0;
        const char* defaultPath = fs::SertFs::currentDirectory();
        strCopy(mInputBuffer, defaultPath, MAX_INPUT_LENGTH);
        mInputPos = static_cast<u32>(strLen(mInputBuffer));
        if (mInputPos > 0 && mInputBuffer[mInputPos - 1] != '/') {
            mInputBuffer[mInputPos++] = '/';
            mInputBuffer[mInputPos] = '\0';
        }
        return;
    }

    saveFileAs(mFilename);
}

void TextEditorApp::saveFileAs(const char* path) {
    fs::FileHandle handle = fs::SertFs::open(path, fs::SERTFS_O_WRITE | fs::SERTFS_O_CREATE | fs::SERTFS_O_TRUNCATE);
    if (!handle.valid) return;

    for (u32 i = 0; i < mLineCount; i++) {
        usize len = strLen(mLines[i]);
        if (len > 0) {
            fs::SertFs::write(&handle, mLines[i], len);
        }
        if (i < mLineCount - 1) {
            fs::SertFs::write(&handle, "\n", 1);
        }
    }

    fs::SertFs::close(&handle);
    strCopy(mFilename, path, MAX_FILENAME);
    mModified = false;
}

void TextEditorApp::setFilename(const char* path) {
    strCopy(mFilename, path, MAX_FILENAME);
}

void TextEditorApp::insertChar(char c) {
    if (mCursorCol >= MAX_LINE_LENGTH - 1) return;

    usize len = strLen(mLines[mCursorLine]);
    if (len >= MAX_LINE_LENGTH - 1) return;

    for (usize i = len + 1; i > mCursorCol; i--) {
        mLines[mCursorLine][i] = mLines[mCursorLine][i - 1];
    }
    mLines[mCursorLine][mCursorCol] = c;
    mCursorCol++;
    mModified = true;
}

void TextEditorApp::deleteChar() {
    if (mCursorCol > 0) {
        usize len = strLen(mLines[mCursorLine]);
        for (usize i = mCursorCol - 1; i < len; i++) {
            mLines[mCursorLine][i] = mLines[mCursorLine][i + 1];
        }
        mCursorCol--;
        mModified = true;
    } else if (mCursorLine > 0) {
        usize prevLen = strLen(mLines[mCursorLine - 1]);
        usize currLen = strLen(mLines[mCursorLine]);

        if (prevLen + currLen < MAX_LINE_LENGTH - 1) {
            for (usize i = 0; i <= currLen; i++) {
                mLines[mCursorLine - 1][prevLen + i] = mLines[mCursorLine][i];
            }

            for (u32 i = mCursorLine; i < mLineCount - 1; i++) {
                strCopy(mLines[i], mLines[i + 1], MAX_LINE_LENGTH);
            }
            memSet(mLines[mLineCount - 1], 0, MAX_LINE_LENGTH);
            mLineCount--;

            mCursorLine--;
            mCursorCol = static_cast<u32>(prevLen);
            mModified = true;
        }
    }
}

void TextEditorApp::deleteCharForward() {
    usize len = strLen(mLines[mCursorLine]);
    if (mCursorCol < len) {
        for (usize i = mCursorCol; i < len; i++) {
            mLines[mCursorLine][i] = mLines[mCursorLine][i + 1];
        }
        mModified = true;
    } else if (mCursorLine < mLineCount - 1) {
        usize currLen = strLen(mLines[mCursorLine]);
        usize nextLen = strLen(mLines[mCursorLine + 1]);

        if (currLen + nextLen < MAX_LINE_LENGTH - 1) {
            for (usize i = 0; i <= nextLen; i++) {
                mLines[mCursorLine][currLen + i] = mLines[mCursorLine + 1][i];
            }

            for (u32 i = mCursorLine + 1; i < mLineCount - 1; i++) {
                strCopy(mLines[i], mLines[i + 1], MAX_LINE_LENGTH);
            }
            memSet(mLines[mLineCount - 1], 0, MAX_LINE_LENGTH);
            mLineCount--;
            mModified = true;
        }
    }
}

void TextEditorApp::insertNewLine() {
    if (mLineCount >= MAX_LINES - 1) return;

    for (u32 i = mLineCount; i > mCursorLine + 1; i--) {
        strCopy(mLines[i], mLines[i - 1], MAX_LINE_LENGTH);
    }

    usize len = strLen(mLines[mCursorLine]);
    strCopy(mLines[mCursorLine + 1], &mLines[mCursorLine][mCursorCol], MAX_LINE_LENGTH);
    mLines[mCursorLine][mCursorCol] = '\0';

    (void)len;

    mLineCount++;
    mCursorLine++;
    mCursorCol = 0;
    mModified = true;
    ensureCursorVisible();
}

void TextEditorApp::moveCursorUp() {
    if (mCursorLine > 0) {
        mCursorLine--;
        usize len = strLen(mLines[mCursorLine]);
        if (mCursorCol > len) {
            mCursorCol = static_cast<u32>(len);
        }
        ensureCursorVisible();
    }
}

void TextEditorApp::moveCursorDown() {
    if (mCursorLine < mLineCount - 1) {
        mCursorLine++;
        usize len = strLen(mLines[mCursorLine]);
        if (mCursorCol > len) {
            mCursorCol = static_cast<u32>(len);
        }
        ensureCursorVisible();
    }
}

void TextEditorApp::moveCursorLeft() {
    if (mCursorCol > 0) {
        mCursorCol--;
    } else if (mCursorLine > 0) {
        mCursorLine--;
        mCursorCol = static_cast<u32>(strLen(mLines[mCursorLine]));
        ensureCursorVisible();
    }
}

void TextEditorApp::moveCursorRight() {
    usize len = strLen(mLines[mCursorLine]);
    if (mCursorCol < len) {
        mCursorCol++;
    } else if (mCursorLine < mLineCount - 1) {
        mCursorLine++;
        mCursorCol = 0;
        ensureCursorVisible();
    }
}

void TextEditorApp::moveCursorHome() {
    mCursorCol = 0;
}

void TextEditorApp::moveCursorEnd() {
    mCursorCol = static_cast<u32>(strLen(mLines[mCursorLine]));
}

void TextEditorApp::pageUp() {
    if (mCursorLine >= mVisibleLines) {
        mCursorLine -= mVisibleLines;
    } else {
        mCursorLine = 0;
    }
    usize len = strLen(mLines[mCursorLine]);
    if (mCursorCol > len) {
        mCursorCol = static_cast<u32>(len);
    }
    ensureCursorVisible();
}

void TextEditorApp::pageDown() {
    if (mCursorLine + mVisibleLines < mLineCount) {
        mCursorLine += mVisibleLines;
    } else {
        mCursorLine = mLineCount - 1;
    }
    usize len = strLen(mLines[mCursorLine]);
    if (mCursorCol > len) {
        mCursorCol = static_cast<u32>(len);
    }
    ensureCursorVisible();
}

void TextEditorApp::ensureCursorVisible() {
    if (mCursorLine < mScrollOffset) {
        mScrollOffset = mCursorLine;
    } else if (mCursorLine >= mScrollOffset + mVisibleLines) {
        mScrollOffset = mCursorLine - mVisibleLines + 1;
    }
}

u32 TextEditorApp::lineNumberWidth() const {
    u32 digits = 1;
    u32 lines = mLineCount;
    while (lines >= 10) {
        digits++;
        lines /= 10;
    }
    return (digits + 2) * Font::CHAR_WIDTH;
}

void TextEditorApp::render() {
    if (!mInitialized) {
        initializeBuffer();
    }

    Window* win = WindowManager::getWindow(mWindowId);
    if (!win) return;

    Rect client = win->clientArea();

    Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(client.y),
                          client.width, client.height, mBackgroundColor);

    renderLineNumbers(client);
    renderTextArea(client);
    if (mMode == EditorMode::Normal) {
        renderCursor(client);
    }
    renderStatusBar(client);

    if (mMode != EditorMode::Normal) {
        renderInputDialog(client);
    }
}

void TextEditorApp::renderLineNumbers(const Rect& client) {
    u32 lineNumWidth = lineNumberWidth();
    Color lineNumBg(40, 40, 45);

    Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(client.y),
                          lineNumWidth, client.height - 20, lineNumBg);

    u32 lineHeight = Font::CHAR_HEIGHT + 2;
    u32 maxVisibleLines = (client.height - 24) / lineHeight;

    for (u32 i = 0; i < maxVisibleLines && (mScrollOffset + i) < mLineCount; i++) {
        u32 lineNum = mScrollOffset + i + 1;
        char numStr[12];
        u32 idx = 0;
        u32 temp = lineNum;
        char tempBuf[12];
        u32 tempIdx = 0;

        do {
            tempBuf[tempIdx++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0);

        while (tempIdx > 0) {
            numStr[idx++] = tempBuf[--tempIdx];
        }
        numStr[idx] = '\0';

        i32 textX = client.x + static_cast<i32>(lineNumWidth) - static_cast<i32>((idx + 1) * Font::CHAR_WIDTH);
        i32 textY = client.y + static_cast<i32>(i * lineHeight) + 2;

        Color numColor = (mScrollOffset + i == mCursorLine) ? Color(180, 180, 200) : mLineNumberColor;
        WindowManager::drawText(textX, textY, numStr, numColor);
    }
}

void TextEditorApp::renderTextArea(const Rect& client) {
    u32 lineNumWidth = lineNumberWidth();
    u32 textStartX = static_cast<u32>(client.x) + lineNumWidth + 4;
    u32 lineHeight = Font::CHAR_HEIGHT + 2;
    u32 maxVisibleLines = (client.height - 24) / lineHeight;

    for (u32 i = 0; i < maxVisibleLines && (mScrollOffset + i) < mLineCount; i++) {
        u32 lineIdx = mScrollOffset + i;
        i32 textY = client.y + static_cast<i32>(i * lineHeight) + 2;

        const char* line = mLines[lineIdx];
        i32 textX = static_cast<i32>(textStartX);

        while (*line) {
            const u8* glyph = Font::getGlyph(*line);
            for (u32 row = 0; row < Font::CHAR_HEIGHT; row++) {
                for (u32 col = 0; col < Font::CHAR_WIDTH; col++) {
                    if (glyph[row] & (0x80 >> col)) {
                        i32 px = textX + static_cast<i32>(col);
                        i32 py = textY + static_cast<i32>(row);
                        if (px >= client.x && px < client.x + static_cast<i32>(client.width) &&
                            py >= client.y && py < client.y + static_cast<i32>(client.height) - 20) {
                            Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), mTextColor);
                        }
                    }
                }
            }
            textX += Font::CHAR_WIDTH;
            line++;
        }
    }
}

void TextEditorApp::renderCursor(const Rect& client) {
    if (mCursorLine < mScrollOffset || mCursorLine >= mScrollOffset + mVisibleLines) {
        return;
    }

    u32 lineNumWidth = lineNumberWidth();
    u32 lineHeight = Font::CHAR_HEIGHT + 2;
    u32 visibleLine = mCursorLine - mScrollOffset;

    i32 cursorX = client.x + static_cast<i32>(lineNumWidth) + 4 + static_cast<i32>(mCursorCol * Font::CHAR_WIDTH);
    i32 cursorY = client.y + static_cast<i32>(visibleLine * lineHeight) + 2;

    Framebuffer::fillRect(static_cast<u32>(cursorX), static_cast<u32>(cursorY),
                          2, Font::CHAR_HEIGHT, mCursorColor);
}

void TextEditorApp::renderStatusBar(const Rect& client) {
    i32 statusY = client.y + static_cast<i32>(client.height) - 20;

    Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(statusY),
                          client.width, 20, mStatusBarColor);

    char statusText[128];
    u32 idx = 0;

    if (mFilename[0] != '\0') {
        const char* fn = mFilename;
        while (*fn && idx < 60) {
            statusText[idx++] = *fn++;
        }
    } else {
        const char* untitled = "Untitled";
        while (*untitled && idx < 60) {
            statusText[idx++] = *untitled++;
        }
    }

    if (mModified) {
        statusText[idx++] = ' ';
        statusText[idx++] = '*';
    }

    statusText[idx++] = ' ';
    statusText[idx++] = '|';
    statusText[idx++] = ' ';
    statusText[idx++] = 'L';
    statusText[idx++] = 'n';
    statusText[idx++] = ' ';

    char numBuf[12];
    u32 numIdx = 0;
    u32 lineNum = mCursorLine + 1;
    char tempBuf[12];
    u32 tempIdx = 0;
    do {
        tempBuf[tempIdx++] = '0' + (lineNum % 10);
        lineNum /= 10;
    } while (lineNum > 0);
    while (tempIdx > 0) {
        numBuf[numIdx++] = tempBuf[--tempIdx];
    }
    numBuf[numIdx] = '\0';

    for (u32 i = 0; i < numIdx; i++) {
        statusText[idx++] = numBuf[i];
    }

    statusText[idx++] = ',';
    statusText[idx++] = ' ';
    statusText[idx++] = 'C';
    statusText[idx++] = 'o';
    statusText[idx++] = 'l';
    statusText[idx++] = ' ';

    numIdx = 0;
    tempIdx = 0;
    u32 colNum = mCursorCol + 1;
    do {
        tempBuf[tempIdx++] = '0' + (colNum % 10);
        colNum /= 10;
    } while (colNum > 0);
    while (tempIdx > 0) {
        numBuf[numIdx++] = tempBuf[--tempIdx];
    }
    numBuf[numIdx] = '\0';

    for (u32 i = 0; i < numIdx; i++) {
        statusText[idx++] = numBuf[i];
    }

    statusText[idx] = '\0';

    WindowManager::drawText(client.x + 4, statusY + 4, statusText, Color(180, 180, 180));

    const char* helpText = "Ctrl+S:Save Ctrl+O:Open Ctrl+N:New";
    i32 helpX = client.x + static_cast<i32>(client.width) - static_cast<i32>(strLen(helpText) * Font::CHAR_WIDTH) - 8;
    WindowManager::drawText(helpX, statusY + 4, helpText, Color(120, 120, 140));
}

void TextEditorApp::handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) {
    (void)alt;
    (void)shift;

    if (!mInitialized) {
        initializeBuffer();
    }

    if (mMode != EditorMode::Normal) {
        handlePromptKeyPress(code, ascii);
        return;
    }

    handleNormalKeyPress(code, ascii, ctrl);
}

void TextEditorApp::handleNormalKeyPress(input::KeyCode code, u8 ascii, bool ctrl) {
    if (ctrl) {
        switch (code) {
            case input::KeyCode::S:
                saveFile();
                return;
            case input::KeyCode::N:
                newFile();
                return;
            case input::KeyCode::O:
                mMode = EditorMode::OpenFilePrompt;
                mInputBuffer[0] = '\0';
                mInputPos = 0;
                {
                    const char* defaultPath = fs::SertFs::currentDirectory();
                    strCopy(mInputBuffer, defaultPath, MAX_INPUT_LENGTH);
                    mInputPos = static_cast<u32>(strLen(mInputBuffer));
                    if (mInputPos > 0 && mInputBuffer[mInputPos - 1] != '/') {
                        mInputBuffer[mInputPos++] = '/';
                        mInputBuffer[mInputPos] = '\0';
                    }
                }
                return;
            default:
                break;
        }
    }

    switch (code) {
        case input::KeyCode::Up:
            moveCursorUp();
            break;
        case input::KeyCode::Down:
            moveCursorDown();
            break;
        case input::KeyCode::Left:
            moveCursorLeft();
            break;
        case input::KeyCode::Right:
            moveCursorRight();
            break;
        case input::KeyCode::Home:
            moveCursorHome();
            break;
        case input::KeyCode::End:
            moveCursorEnd();
            break;
        case input::KeyCode::PageUp:
            pageUp();
            break;
        case input::KeyCode::PageDown:
            pageDown();
            break;
        case input::KeyCode::Enter:
            insertNewLine();
            break;
        case input::KeyCode::Backspace:
            deleteChar();
            break;
        case input::KeyCode::Delete:
            deleteCharForward();
            break;
        case input::KeyCode::Tab:
            for (u32 i = 0; i < 4; i++) {
                insertChar(' ');
            }
            break;
        default:
            if (ascii >= 32 && ascii < 127) {
                insertChar(static_cast<char>(ascii));
            }
            break;
    }
}

void TextEditorApp::handlePromptKeyPress(input::KeyCode code, u8 ascii) {
    switch (code) {
        case input::KeyCode::Escape:
            mMode = EditorMode::Normal;
            mInputBuffer[0] = '\0';
            mInputPos = 0;
            break;
        case input::KeyCode::Enter:
            if (mInputPos > 0) {
                if (mMode == EditorMode::SaveAsPrompt) {
                    saveFileAs(mInputBuffer);
                } else if (mMode == EditorMode::OpenFilePrompt) {
                    loadFile(mInputBuffer);
                }
            }
            mMode = EditorMode::Normal;
            mInputBuffer[0] = '\0';
            mInputPos = 0;
            break;
        case input::KeyCode::Backspace:
            if (mInputPos > 0) {
                mInputPos--;
                mInputBuffer[mInputPos] = '\0';
            }
            break;
        default:
            if (ascii >= 32 && ascii < 127 && mInputPos < MAX_INPUT_LENGTH - 1) {
                mInputBuffer[mInputPos++] = static_cast<char>(ascii);
                mInputBuffer[mInputPos] = '\0';
            }
            break;
    }
}

void TextEditorApp::renderInputDialog(const Rect& client) {
    constexpr u32 DIALOG_WIDTH = 400;
    constexpr u32 DIALOG_HEIGHT = 80;

    i32 dialogX = client.x + static_cast<i32>((client.width - DIALOG_WIDTH) / 2);
    i32 dialogY = client.y + static_cast<i32>((client.height - DIALOG_HEIGHT) / 2);

    Color dialogBg(60, 60, 70);
    Color dialogBorder(100, 100, 120);
    Color inputBg(40, 40, 50);

    Framebuffer::fillRect(static_cast<u32>(dialogX), static_cast<u32>(dialogY),
                          DIALOG_WIDTH, DIALOG_HEIGHT, dialogBg);

    Framebuffer::fillRect(static_cast<u32>(dialogX), static_cast<u32>(dialogY),
                          DIALOG_WIDTH, 2, dialogBorder);
    Framebuffer::fillRect(static_cast<u32>(dialogX), static_cast<u32>(dialogY + static_cast<i32>(DIALOG_HEIGHT) - 2),
                          DIALOG_WIDTH, 2, dialogBorder);
    Framebuffer::fillRect(static_cast<u32>(dialogX), static_cast<u32>(dialogY),
                          2, DIALOG_HEIGHT, dialogBorder);
    Framebuffer::fillRect(static_cast<u32>(dialogX + static_cast<i32>(DIALOG_WIDTH) - 2), static_cast<u32>(dialogY),
                          2, DIALOG_HEIGHT, dialogBorder);

    const char* title = (mMode == EditorMode::SaveAsPrompt) ? "Save As:" : "Open File:";
    WindowManager::drawText(dialogX + 10, dialogY + 10, title, Color(220, 220, 220));

    i32 inputX = dialogX + 10;
    i32 inputY = dialogY + 30;
    u32 inputWidth = DIALOG_WIDTH - 20;
    u32 inputHeight = 24;

    Framebuffer::fillRect(static_cast<u32>(inputX), static_cast<u32>(inputY),
                          inputWidth, inputHeight, inputBg);

    WindowManager::drawText(inputX + 4, inputY + 6, mInputBuffer, Color(200, 200, 200));

    i32 cursorX = inputX + 4 + static_cast<i32>(mInputPos * Font::CHAR_WIDTH);
    Framebuffer::fillRect(static_cast<u32>(cursorX), static_cast<u32>(inputY + 4),
                          2, Font::CHAR_HEIGHT, mCursorColor);

    const char* hint = "Enter: Confirm | Esc: Cancel";
    i32 hintX = dialogX + static_cast<i32>((DIALOG_WIDTH - strLen(hint) * Font::CHAR_WIDTH) / 2);
    WindowManager::drawText(hintX, dialogY + 60, hint, Color(120, 120, 140));
}

void TextEditorApp::handleMouseClick(i32 x, i32 y, bool doubleClick) {
    (void)doubleClick;

    Window* win = WindowManager::getWindow(mWindowId);
    if (!win) return;

    Rect client = win->clientArea();
    u32 lineNumWidth = lineNumberWidth();
    u32 lineHeight = Font::CHAR_HEIGHT + 2;

    i32 relX = x - client.x - static_cast<i32>(lineNumWidth) - 4;
    i32 relY = y - client.y;

    if (relX < 0 || relY < 0 || relY >= static_cast<i32>(client.height) - 20) {
        return;
    }

    u32 clickedLine = mScrollOffset + static_cast<u32>(relY) / lineHeight;
    u32 clickedCol = static_cast<u32>(relX) / Font::CHAR_WIDTH;

    if (clickedLine < mLineCount) {
        mCursorLine = clickedLine;
        usize len = strLen(mLines[mCursorLine]);
        mCursorCol = (clickedCol <= len) ? clickedCol : static_cast<u32>(len);
    }
}

}
