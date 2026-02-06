#pragma once

#include "app.hpp"
#include "../wm/wm.hpp"
#include "../graphics/font.hpp"

namespace sertos::apps {

class TextEditorApp : public App {
public:
    TextEditorApp();
    ~TextEditorApp() override = default;

    void render() override;
    void handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) override;
    void handleMouseClick(i32 x, i32 y, bool doubleClick) override;

    void loadFile(const char* path);
    void saveFile();
    void saveFileAs(const char* path);
    void newFile();
    void setFilename(const char* path);

private:
    static constexpr u32 MAX_LINES = 1000;
    static constexpr u32 MAX_LINE_LENGTH = 256;
    static constexpr u32 MAX_FILENAME = 128;
    static constexpr u32 MAX_INPUT_LENGTH = 128;

    enum class EditorMode : u8 {
        Normal = 0,
        SaveAsPrompt,
        OpenFilePrompt
    };

    char mLines[MAX_LINES][MAX_LINE_LENGTH];
    u32 mLineCount;
    u32 mCursorLine;
    u32 mCursorCol;
    u32 mScrollOffset;
    u32 mVisibleLines;
    u32 mVisibleCols;

    char mFilename[MAX_FILENAME];
    bool mModified;
    bool mInitialized;

    EditorMode mMode;
    char mInputBuffer[MAX_INPUT_LENGTH];
    u32 mInputPos;

    graphics::Color mTextColor;
    graphics::Color mBackgroundColor;
    graphics::Color mCursorColor;
    graphics::Color mLineNumberColor;
    graphics::Color mStatusBarColor;

    void initializeBuffer();
    void insertChar(char c);
    void deleteChar();
    void deleteCharForward();
    void insertNewLine();
    void moveCursorUp();
    void moveCursorDown();
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorHome();
    void moveCursorEnd();
    void pageUp();
    void pageDown();
    void ensureCursorVisible();

    void renderLineNumbers(const wm::Rect& client);
    void renderTextArea(const wm::Rect& client);
    void renderCursor(const wm::Rect& client);
    void renderStatusBar(const wm::Rect& client);
    void renderInputDialog(const wm::Rect& client);

    void handleNormalKeyPress(input::KeyCode code, u8 ascii, bool ctrl);
    void handlePromptKeyPress(input::KeyCode code, u8 ascii);

    u32 lineNumberWidth() const;
};

}
