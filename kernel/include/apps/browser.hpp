#pragma once

#include "app.hpp"
#include "html_parser.hpp"
#include "../wm/wm.hpp"
#include "../graphics/font.hpp"

namespace sertos::apps {

class BrowserApp : public App {
public:
    BrowserApp();
    ~BrowserApp() override = default;

    void render() override;
    void handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) override;
    void handleMouseClick(i32 x, i32 y, bool doubleClick) override;

    void navigate(const char* url);

private:
    static constexpr u32 URL_BAR_HEIGHT = 28;
    static constexpr u32 NAV_BAR_HEIGHT = 24;
    static constexpr u32 TOOLBAR_HEIGHT = URL_BAR_HEIGHT + NAV_BAR_HEIGHT;
    static constexpr u32 STATUS_BAR_HEIGHT = 20;
    static constexpr u32 MAX_URL_LENGTH = 512;
    static constexpr u32 MAX_TITLE_LENGTH = 64;
    static constexpr u32 HISTORY_MAX = 32;
    static constexpr u32 LINE_HEIGHT = 18;
    static constexpr u32 CHAR_WIDTH = 8;
    static constexpr u32 SCROLLBAR_WIDTH = 10;

    // URL bar state
    char mUrlBuffer[MAX_URL_LENGTH];
    u32  mUrlLength;
    u32  mUrlCursorPos;
    bool mUrlBarFocused;

    // Page content
    RenderLine  mRenderLines[MAX_RENDER_LINES];
    u32         mRenderLineCount;
    ClickableLink mLinks[MAX_LINKS];
    u32         mLinkCount;
    char        mPageTitle[MAX_TITLE_LENGTH];

    // Scroll state
    u32 mScrollOffset;
    u32 mVisibleLines;

    // Navigation history
    char mHistory[HISTORY_MAX][MAX_URL_LENGTH];
    u32  mHistoryCount;
    u32  mHistoryPos;

    // State
    enum class PageState : u8 {
        Idle = 0,
        Loading,
        Loaded,
        Error
    };
    PageState mPageState;
    char mStatusText[128];
    bool mInitialized;

    // Render sub-functions
    void renderNavBar(const Rect& client);
    void renderUrlBar(const Rect& client);
    void renderContent(const Rect& client);
    void renderStatusBar(const Rect& client);
    void renderScrollbar(const Rect& client);

    // Navigation
    void doNavigate(const char* url);
    void goBack();
    void goForward();
    void reload();
    void pushHistory(const char* url);

    // Content
    void loadPage(const char* url);
    void showWelcomePage();
    void setError(const char* message);
    void layoutContent(u32 widthChars);

    // URL bar
    void urlBarInsertChar(char c);
    void urlBarDeleteChar();
    void urlBarSubmit();

    // Link detection
    i32 findLinkAt(i32 x, i32 y, const Rect& contentArea);

    // Helpers
    void resolveRelativeUrl(const char* href, const char* baseUrl, char* outUrl, u32 maxLen);
    u32  getContentAreaTop(const Rect& client);
    u32  getContentAreaHeight(const Rect& client);
};

}
