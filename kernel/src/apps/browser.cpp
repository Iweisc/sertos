#include "../../include/apps/browser.hpp"
#include "../../include/graphics/framebuffer.hpp"
#include "../../include/net/http.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::apps {

using wm::WindowManager;
using wm::Window;
using wm::Rect;
using graphics::Color;
using graphics::Font;
using graphics::Framebuffer;

// --- String helpers ---

static u32 slen(const char* s) {
    u32 len = 0;
    while (s[len]) len++;
    return len;
}

static void scpy(char* dst, const char* src, u32 maxLen) {
    u32 i = 0;
    while (src[i] && i < maxLen - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void scat(char* dst, const char* src, u32 maxLen) {
    u32 dstLen = slen(dst);
    u32 i = 0;
    while (src[i] && dstLen + i < maxLen - 1) {
        dst[dstLen + i] = src[i]; i++;
    }
    dst[dstLen + i] = '\0';
}

static bool startsWith(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return false;
        str++; prefix++;
    }
    return true;
}

static void smemset(void* dst, u8 val, u32 n) {
    u8* d = reinterpret_cast<u8*>(dst);
    for (u32 i = 0; i < n; i++) d[i] = val;
}

// --- Constructor ---

BrowserApp::BrowserApp()
    : mUrlLength(0), mUrlCursorPos(0), mUrlBarFocused(true),
      mRenderLineCount(0), mLinkCount(0),
      mScrollOffset(0), mVisibleLines(0),
      mHistoryCount(0), mHistoryPos(0),
      mPageState(PageState::Idle), mInitialized(false) {
    mUrlBuffer[0] = '\0';
    mPageTitle[0] = '\0';
    mStatusText[0] = '\0';
    smemset(mHistory, 0, sizeof(mHistory));
    showWelcomePage();
    mInitialized = true;
}

// --- Main render ---

void BrowserApp::render() {
    Window* win = WindowManager::getWindow(mWindowId);
    if (!win) return;

    Rect client = win->clientArea();

    // Background
    Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(client.y),
                          client.width, client.height, Color(245, 245, 245));

    renderNavBar(client);
    renderUrlBar(client);
    renderContent(client);
    renderStatusBar(client);
    renderScrollbar(client);
}

// --- Navigation bar (back/forward/reload) ---

void BrowserApp::renderNavBar(const Rect& client) {
    i32 bx = client.x;
    i32 by = client.y;
    u32 bw = client.width;

    // Nav bar background
    Framebuffer::fillRect(static_cast<u32>(bx), static_cast<u32>(by),
                          bw, NAV_BAR_HEIGHT, Color(230, 230, 230));

    // Bottom border
    Framebuffer::fillRect(static_cast<u32>(bx), static_cast<u32>(by + NAV_BAR_HEIGHT - 1),
                          bw, 1, Color(200, 200, 200));

    // Back button [<]
    Color btnColor = mHistoryPos > 0 ? Color(60, 60, 60) : Color(180, 180, 180);
    WindowManager::drawTextClipped(bx + 8, by + 4, "<", btnColor, client);

    // Forward button [>]
    btnColor = mHistoryPos + 1 < mHistoryCount ? Color(60, 60, 60) : Color(180, 180, 180);
    WindowManager::drawTextClipped(bx + 28, by + 4, ">", btnColor, client);

    // Reload button [R]
    WindowManager::drawTextClipped(bx + 48, by + 4, "R", Color(60, 60, 60), client);
}

// --- URL bar ---

void BrowserApp::renderUrlBar(const Rect& client) {
    i32 bx = client.x;
    i32 by = client.y + NAV_BAR_HEIGHT;
    u32 bw = client.width;

    // URL bar background
    Framebuffer::fillRect(static_cast<u32>(bx), static_cast<u32>(by),
                          bw, URL_BAR_HEIGHT, Color(255, 255, 255));

    // Border
    Color borderColor = mUrlBarFocused ? Color(66, 133, 244) : Color(200, 200, 200);
    Framebuffer::drawRect(static_cast<u32>(bx + 4), static_cast<u32>(by + 2),
                          bw - 8, URL_BAR_HEIGHT - 4, borderColor);

    // URL text
    WindowManager::drawTextClipped(bx + 10, by + 6, mUrlBuffer, Color(30, 30, 30), client);

    // Cursor
    if (mUrlBarFocused) {
        u32 cursorX = static_cast<u32>(bx + 10) + mUrlCursorPos * CHAR_WIDTH;
        Framebuffer::fillRect(cursorX, static_cast<u32>(by + 5), 1, 16, Color(0, 0, 0));
    }

    // Bottom border
    Framebuffer::fillRect(static_cast<u32>(bx), static_cast<u32>(by + URL_BAR_HEIGHT - 1),
                          bw, 1, Color(200, 200, 200));
}

// --- Content area ---

u32 BrowserApp::getContentAreaTop(const Rect& client) {
    return static_cast<u32>(client.y) + TOOLBAR_HEIGHT;
}

u32 BrowserApp::getContentAreaHeight(const Rect& client) {
    u32 usedHeight = TOOLBAR_HEIGHT + STATUS_BAR_HEIGHT;
    if (client.height > usedHeight) return client.height - usedHeight;
    return 0;
}

void BrowserApp::renderContent(const Rect& client) {
    u32 contentTop = getContentAreaTop(client);
    u32 contentHeight = getContentAreaHeight(client);
    u32 contentWidth = client.width > SCROLLBAR_WIDTH ? client.width - SCROLLBAR_WIDTH : client.width;

    if (contentHeight == 0) return;

    // Content background
    Framebuffer::fillRect(static_cast<u32>(client.x), contentTop,
                          contentWidth, contentHeight, Color(255, 255, 255));

    mVisibleLines = contentHeight / LINE_HEIGHT;
    if (mVisibleLines == 0) mVisibleLines = 1;

    // Draw loading message
    if (mPageState == PageState::Loading) {
        WindowManager::drawTextClipped(client.x + 20, static_cast<i32>(contentTop + 20),
                                       "Loading...", Color(100, 100, 100), client);
        return;
    }

    if (mPageState == PageState::Error) {
        WindowManager::drawTextClipped(client.x + 20, static_cast<i32>(contentTop + 20),
                                       mStatusText, Color(200, 50, 50), client);
        return;
    }

    // Render visible lines
    u32 y = contentTop + 4;
    for (u32 i = mScrollOffset; i < mRenderLineCount && y + LINE_HEIGHT <= contentTop + contentHeight; i++) {
        const RenderLine* rl = &mRenderLines[i];

        // Spacing
        if (rl->spacingBefore > 0) {
            y += rl->spacingBefore * (LINE_HEIGHT / 2);
            if (y + LINE_HEIGHT > contentTop + contentHeight) break;
        }

        i32 lineX = client.x + 8 + static_cast<i32>(rl->indent * CHAR_WIDTH);
        i32 lineY = static_cast<i32>(y);

        if (rl->isHr) {
            // Draw horizontal rule
            Framebuffer::fillRect(static_cast<u32>(client.x + 8), static_cast<u32>(lineY + LINE_HEIGHT / 2),
                                  contentWidth - 16, 1, Color(rl->colorR, rl->colorG, rl->colorB));
            y += LINE_HEIGHT;
            continue;
        }

        Color textColor(rl->colorR, rl->colorG, rl->colorB);

        // Draw each character
        for (u32 ci = 0; ci < rl->textLength; ci++) {
            char ch = rl->text[ci];
            if (ch == '\0') break;

            i32 charX = lineX + static_cast<i32>(ci * CHAR_WIDTH);
            if (charX + static_cast<i32>(CHAR_WIDTH) > client.x + static_cast<i32>(contentWidth)) break;
            if (charX < client.x) continue;

            const u8* glyph = Font::getGlyph(ch);
            for (u32 row = 0; row < Font::CHAR_HEIGHT; row++) {
                for (u32 col = 0; col < Font::CHAR_WIDTH; col++) {
                    if (glyph[row] & (0x80 >> col)) {
                        i32 px = charX + static_cast<i32>(col);
                        i32 py = lineY + static_cast<i32>(row);
                        if (px >= client.x && px < client.x + static_cast<i32>(client.width) &&
                            py >= static_cast<i32>(contentTop) && py < static_cast<i32>(contentTop + contentHeight)) {
                            Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), textColor);

                            // Bold: draw shifted copy
                            if (rl->bold && px + 1 < client.x + static_cast<i32>(contentWidth)) {
                                Framebuffer::putPixel(static_cast<u32>(px + 1), static_cast<u32>(py), textColor);
                            }
                        }
                    }
                }
            }

            // Underline
            if (rl->underline) {
                i32 ulY = lineY + static_cast<i32>(Font::CHAR_HEIGHT);
                if (ulY >= static_cast<i32>(contentTop) && ulY < static_cast<i32>(contentTop + contentHeight)) {
                    for (u32 ux = 0; ux < CHAR_WIDTH; ux++) {
                        i32 px = charX + static_cast<i32>(ux);
                        if (px >= client.x && px < client.x + static_cast<i32>(contentWidth)) {
                            Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(ulY), textColor);
                        }
                    }
                }
            }
        }

        y += LINE_HEIGHT;
    }
}

// --- Status bar ---

void BrowserApp::renderStatusBar(const Rect& client) {
    u32 sbY = static_cast<u32>(client.y) + client.height - STATUS_BAR_HEIGHT;

    Framebuffer::fillRect(static_cast<u32>(client.x), sbY,
                          client.width, STATUS_BAR_HEIGHT, Color(240, 240, 240));

    // Top border
    Framebuffer::fillRect(static_cast<u32>(client.x), sbY,
                          client.width, 1, Color(200, 200, 200));

    const char* status = "Ready";
    if (mPageState == PageState::Loading) status = "Loading...";
    else if (mPageState == PageState::Error) status = "Error";
    else if (mPageState == PageState::Loaded) status = mStatusText[0] ? mStatusText : "Done";

    WindowManager::drawTextClipped(client.x + 6, static_cast<i32>(sbY + 3),
                                   status, Color(100, 100, 100), client);
}

// --- Scrollbar ---

void BrowserApp::renderScrollbar(const Rect& client) {
    u32 contentTop = getContentAreaTop(client);
    u32 contentHeight = getContentAreaHeight(client);
    if (contentHeight == 0 || mRenderLineCount == 0) return;

    u32 sbX = static_cast<u32>(client.x) + client.width - SCROLLBAR_WIDTH;

    // Scrollbar track
    Framebuffer::fillRect(sbX, contentTop, SCROLLBAR_WIDTH, contentHeight, Color(235, 235, 235));

    // Scrollbar thumb
    u32 totalLines = mRenderLineCount > mVisibleLines ? mRenderLineCount : mVisibleLines;
    u32 thumbHeight = (mVisibleLines * contentHeight) / totalLines;
    if (thumbHeight < 20) thumbHeight = 20;
    if (thumbHeight > contentHeight) thumbHeight = contentHeight;

    u32 thumbY = contentTop;
    if (totalLines > mVisibleLines) {
        thumbY = contentTop + (mScrollOffset * (contentHeight - thumbHeight)) / (totalLines - mVisibleLines);
    }

    Framebuffer::fillRect(sbX + 2, thumbY, SCROLLBAR_WIDTH - 4, thumbHeight, Color(180, 180, 180));
}

// --- Key handling ---

void BrowserApp::handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) {
    (void)shift;

    if (ctrl) {
        if (code == input::KeyCode::L) {
            // Focus URL bar
            mUrlBarFocused = true;
            return;
        }
        if (code == input::KeyCode::R) {
            reload();
            return;
        }
        return;
    }

    if (alt) {
        if (code == input::KeyCode::Left) {
            goBack();
            return;
        }
        if (code == input::KeyCode::Right) {
            goForward();
            return;
        }
        return;
    }

    if (mUrlBarFocused) {
        switch (code) {
            case input::KeyCode::Enter:
                urlBarSubmit();
                break;
            case input::KeyCode::Backspace:
                urlBarDeleteChar();
                break;
            case input::KeyCode::Escape:
                mUrlBarFocused = false;
                break;
            case input::KeyCode::Left:
                if (mUrlCursorPos > 0) mUrlCursorPos--;
                break;
            case input::KeyCode::Right:
                if (mUrlCursorPos < mUrlLength) mUrlCursorPos++;
                break;
            case input::KeyCode::Home:
                mUrlCursorPos = 0;
                break;
            case input::KeyCode::End:
                mUrlCursorPos = mUrlLength;
                break;
            default:
                if (ascii >= 32 && ascii < 127) {
                    urlBarInsertChar(static_cast<char>(ascii));
                }
                break;
        }
        return;
    }

    // Content navigation
    switch (code) {
        case input::KeyCode::Up:
            if (mScrollOffset > 0) mScrollOffset--;
            break;
        case input::KeyCode::Down:
            if (mScrollOffset + mVisibleLines < mRenderLineCount) mScrollOffset++;
            break;
        case input::KeyCode::PageUp:
            if (mScrollOffset > mVisibleLines) mScrollOffset -= mVisibleLines;
            else mScrollOffset = 0;
            break;
        case input::KeyCode::PageDown:
            if (mScrollOffset + mVisibleLines < mRenderLineCount)
                mScrollOffset += mVisibleLines;
            break;
        case input::KeyCode::Home:
            mScrollOffset = 0;
            break;
        case input::KeyCode::End:
            if (mRenderLineCount > mVisibleLines)
                mScrollOffset = mRenderLineCount - mVisibleLines;
            break;
        case input::KeyCode::Backspace:
            goBack();
            break;
        default:
            // Tab or other key to focus URL bar
            if (code == input::KeyCode::Tab) {
                mUrlBarFocused = true;
            }
            break;
    }
}

// --- Mouse click ---

void BrowserApp::handleMouseClick(i32 x, i32 y, bool doubleClick) {
    (void)doubleClick;

    Window* win = WindowManager::getWindow(mWindowId);
    if (!win) return;

    Rect client = win->clientArea();

    // Relative to client area
    i32 relX = x;
    i32 relY = y;

    // Check nav bar clicks
    if (relY >= client.y && relY < client.y + static_cast<i32>(NAV_BAR_HEIGHT)) {
        i32 btnX = relX - client.x;
        if (btnX >= 4 && btnX < 24) {
            goBack();
        } else if (btnX >= 24 && btnX < 44) {
            goForward();
        } else if (btnX >= 44 && btnX < 64) {
            reload();
        }
        return;
    }

    // Check URL bar click
    if (relY >= client.y + static_cast<i32>(NAV_BAR_HEIGHT) &&
        relY < client.y + static_cast<i32>(TOOLBAR_HEIGHT)) {
        mUrlBarFocused = true;
        // Set cursor position based on click x
        u32 charPos = static_cast<u32>(relX - client.x - 10) / CHAR_WIDTH;
        if (charPos > mUrlLength) charPos = mUrlLength;
        mUrlCursorPos = charPos;
        return;
    }

    // Check content area click (link detection)
    i32 contentAreaY = findLinkAt(relX, relY, client);
    if (contentAreaY >= 0) {
        navigate(mLinks[contentAreaY].href);
    } else {
        // Clicking content unfocuses URL bar
        mUrlBarFocused = false;
    }
}

// --- Link detection ---

i32 BrowserApp::findLinkAt(i32 x, i32 y, const Rect& contentArea) {
    u32 contentTop = getContentAreaTop(contentArea);
    u32 contentHeight = getContentAreaHeight(contentArea);

    if (y < static_cast<i32>(contentTop) || y >= static_cast<i32>(contentTop + contentHeight)) return -1;

    u32 relY = static_cast<u32>(y) - contentTop - 4;
    u32 lineIndex = relY / LINE_HEIGHT + mScrollOffset;

    i32 relX = x - contentArea.x - 8;
    if (relX < 0) return -1;
    u32 charCol = static_cast<u32>(relX) / CHAR_WIDTH;

    for (u32 i = 0; i < mLinkCount; i++) {
        if (mLinks[i].lineIndex == lineIndex &&
            charCol >= mLinks[i].startCol && charCol < mLinks[i].endCol) {
            return static_cast<i32>(i);
        }
    }

    return -1;
}

// --- URL bar operations ---

void BrowserApp::urlBarInsertChar(char c) {
    if (mUrlLength >= MAX_URL_LENGTH - 1) return;

    // Shift characters right
    for (u32 i = mUrlLength; i > mUrlCursorPos; i--) {
        mUrlBuffer[i] = mUrlBuffer[i - 1];
    }
    mUrlBuffer[mUrlCursorPos] = c;
    mUrlLength++;
    mUrlCursorPos++;
    mUrlBuffer[mUrlLength] = '\0';
}

void BrowserApp::urlBarDeleteChar() {
    if (mUrlCursorPos == 0) return;

    for (u32 i = mUrlCursorPos - 1; i < mUrlLength - 1; i++) {
        mUrlBuffer[i] = mUrlBuffer[i + 1];
    }
    mUrlLength--;
    mUrlCursorPos--;
    mUrlBuffer[mUrlLength] = '\0';
}

void BrowserApp::urlBarSubmit() {
    mUrlBarFocused = false;
    doNavigate(mUrlBuffer);
}

// --- Navigation ---

void BrowserApp::navigate(const char* url) {
    // Set URL bar
    scpy(mUrlBuffer, url, MAX_URL_LENGTH);
    mUrlLength = slen(mUrlBuffer);
    mUrlCursorPos = mUrlLength;
    doNavigate(url);
}

void BrowserApp::doNavigate(const char* url) {
    if (!url || url[0] == '\0') return;

    // Add http:// if no scheme
    char fullUrl[MAX_URL_LENGTH];
    if (!startsWith(url, "http://") && !startsWith(url, "https://")) {
        fullUrl[0] = '\0';
        scat(fullUrl, "http://", MAX_URL_LENGTH);
        scat(fullUrl, url, MAX_URL_LENGTH);
    } else {
        scpy(fullUrl, url, MAX_URL_LENGTH);
    }

    // Update URL bar
    scpy(mUrlBuffer, fullUrl, MAX_URL_LENGTH);
    mUrlLength = slen(mUrlBuffer);
    mUrlCursorPos = mUrlLength;

    pushHistory(fullUrl);
    loadPage(fullUrl);
}

void BrowserApp::goBack() {
    if (mHistoryPos == 0) return;
    mHistoryPos--;
    scpy(mUrlBuffer, mHistory[mHistoryPos], MAX_URL_LENGTH);
    mUrlLength = slen(mUrlBuffer);
    mUrlCursorPos = mUrlLength;
    loadPage(mHistory[mHistoryPos]);
}

void BrowserApp::goForward() {
    if (mHistoryPos + 1 >= mHistoryCount) return;
    mHistoryPos++;
    scpy(mUrlBuffer, mHistory[mHistoryPos], MAX_URL_LENGTH);
    mUrlLength = slen(mUrlBuffer);
    mUrlCursorPos = mUrlLength;
    loadPage(mHistory[mHistoryPos]);
}

void BrowserApp::reload() {
    if (mHistoryCount > 0) {
        loadPage(mHistory[mHistoryPos]);
    }
}

void BrowserApp::pushHistory(const char* url) {
    // If we went back and then navigate, truncate forward history
    if (mHistoryPos + 1 < mHistoryCount) {
        mHistoryCount = mHistoryPos + 1;
    }

    if (mHistoryCount >= HISTORY_MAX) {
        // Shift everything down
        for (u32 i = 0; i < HISTORY_MAX - 1; i++) {
            scpy(mHistory[i], mHistory[i + 1], MAX_URL_LENGTH);
        }
        mHistoryCount = HISTORY_MAX - 1;
    }

    scpy(mHistory[mHistoryCount], url, MAX_URL_LENGTH);
    mHistoryPos = mHistoryCount;
    mHistoryCount++;
}

// --- Page loading ---

void BrowserApp::loadPage(const char* url) {
    mPageState = PageState::Loading;
    scpy(mStatusText, "Connecting...", 128);
    mScrollOffset = 0;
    mRenderLineCount = 0;
    mLinkCount = 0;
    mPageTitle[0] = '\0';

    // Fetch via HTTP
    static net::HttpResponse response;
    bool success = net::HttpClient::get(url, &response);

    if (!success || !response.success) {
        if (response.errorMessage[0]) {
            setError(response.errorMessage);
        } else {
            setError("Failed to load page. Check the URL and try again.");
        }
        return;
    }

    if (response.statusCode >= 400) {
        char errMsg[128] = "HTTP Error: ";
        // Append status code
        u32 code = response.statusCode;
        char num[8];
        u32 ni = 0;
        if (code >= 100) num[ni++] = '0' + static_cast<char>((code / 100) % 10);
        if (code >= 10) num[ni++] = '0' + static_cast<char>((code / 10) % 10);
        num[ni++] = '0' + static_cast<char>(code % 10);
        num[ni] = '\0';
        scat(errMsg, num, 128);
        setError(errMsg);
        return;
    }

    // Parse HTML
    static HtmlElement elements[HTML_MAX_ELEMENTS];
    u32 elementCount = HtmlParser::parse(response.body, response.bodyLength,
                                          elements, HTML_MAX_ELEMENTS);

    // Layout
    Window* win = WindowManager::getWindow(mWindowId);
    u32 widthChars = 80;
    if (win) {
        Rect client = win->clientArea();
        u32 contentWidth = client.width > SCROLLBAR_WIDTH + 16 ? client.width - SCROLLBAR_WIDTH - 16 : 60;
        widthChars = contentWidth / CHAR_WIDTH;
        if (widthChars < 20) widthChars = 20;
    }

    mRenderLineCount = HtmlParser::layout(elements, elementCount,
                                            mRenderLines, MAX_RENDER_LINES, widthChars,
                                            mLinks, MAX_LINKS, &mLinkCount,
                                            mPageTitle, MAX_TITLE_LENGTH);

    cpu::serialPuts("[BRW] elements=");
    { u32 n = elementCount; char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
    cpu::serialPuts(" lines=");
    { u32 n = mRenderLineCount; char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
    cpu::serialPuts(" title=");
    cpu::serialPuts(mPageTitle);
    cpu::serialPutc('\n');

    mPageState = PageState::Loaded;
    scpy(mStatusText, "Done", 128);

    // Update window title if we got a page title
    if (mPageTitle[0] && win) {
        char title[64] = {0};
        scpy(title, mPageTitle, 56);
        scat(title, " - Browser", 64);
        win->setTitle(title);
    }
}

void BrowserApp::showWelcomePage() {
    const char* welcomeHtml =
        "<html><body>"
        "<h1>SertOS Browser</h1>"
        "<p>Welcome to the SertOS web browser. This is a lightweight HTTP/HTML viewer.</p>"
        "<h2>Getting Started</h2>"
        "<p>Type a URL in the address bar above and press Enter to navigate.</p>"
        "<p>Try visiting:</p>"
        "<ul>"
        "<li><a href=\"http://example.com\">example.com</a> - A simple test page</li>"
        "<li><a href=\"http://info.cern.ch\">info.cern.ch</a> - The first website ever</li>"
        "</ul>"
        "<h2>Keyboard Shortcuts</h2>"
        "<ul>"
        "<li><b>Ctrl+L</b> - Focus the URL bar</li>"
        "<li><b>Alt+Left</b> or <b>Backspace</b> - Go back</li>"
        "<li><b>Alt+Right</b> - Go forward</li>"
        "<li><b>Ctrl+R</b> - Reload page</li>"
        "<li><b>Up/Down</b> - Scroll</li>"
        "<li><b>PageUp/PageDown</b> - Scroll by page</li>"
        "</ul>"
        "<hr>"
        "<p><i>SertOS Browser v1.0 - HTTP only, no JavaScript or CSS support.</i></p>"
        "</body></html>";

    u32 htmlLen = slen(welcomeHtml);

    static HtmlElement elements[HTML_MAX_ELEMENTS];
    u32 elementCount = HtmlParser::parse(welcomeHtml, htmlLen, elements, HTML_MAX_ELEMENTS);

    mRenderLineCount = HtmlParser::layout(elements, elementCount,
                                            mRenderLines, MAX_RENDER_LINES, 80,
                                            mLinks, MAX_LINKS, &mLinkCount,
                                            mPageTitle, MAX_TITLE_LENGTH);

    mPageState = PageState::Loaded;
    scpy(mStatusText, "Welcome", 128);
}

void BrowserApp::setError(const char* message) {
    mPageState = PageState::Error;
    scpy(mStatusText, message, 128);
    mRenderLineCount = 0;
    mLinkCount = 0;
}

void BrowserApp::resolveRelativeUrl(const char* href, const char* baseUrl, char* outUrl, u32 maxLen) {
    if (!href || !href[0]) {
        scpy(outUrl, baseUrl, maxLen);
        return;
    }

    // Absolute URL
    if (startsWith(href, "http://") || startsWith(href, "https://")) {
        scpy(outUrl, href, maxLen);
        return;
    }

    // Protocol-relative
    if (href[0] == '/' && href[1] == '/') {
        outUrl[0] = '\0';
        scat(outUrl, "http:", maxLen);
        scat(outUrl, href, maxLen);
        return;
    }

    // Extract base host from baseUrl
    const char* p = baseUrl;
    if (startsWith(p, "http://")) p += 7;
    else if (startsWith(p, "https://")) p += 8;

    u32 hostLen = 0;
    while (p[hostLen] && p[hostLen] != '/') hostLen++;

    outUrl[0] = '\0';
    scat(outUrl, "http://", maxLen);

    // Append host
    u32 oLen = slen(outUrl);
    u32 toCopy = hostLen;
    if (oLen + toCopy >= maxLen) toCopy = maxLen - oLen - 1;
    for (u32 i = 0; i < toCopy; i++) outUrl[oLen + i] = p[i];
    outUrl[oLen + toCopy] = '\0';

    // Absolute path
    if (href[0] == '/') {
        scat(outUrl, href, maxLen);
    } else {
        // Relative path - append to base path
        scat(outUrl, "/", maxLen);
        scat(outUrl, href, maxLen);
    }
}

}
