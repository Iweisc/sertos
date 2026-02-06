#include "../../include/wm/wm.hpp"
#include "../../include/apps/app.hpp"

namespace sertos::wm {

Window WindowManager::sWindows[MAX_WINDOWS];
u32 WindowManager::sWindowCount = 0;
u32 WindowManager::sFocusedWindow = 0;
Color WindowManager::sDesktopColor(0, 80, 120);
bool WindowManager::sInitialized = false;
u32 WindowManager::sNextWindowId = 1;

DragMode WindowManager::sDragMode = DragMode::None;
u32 WindowManager::sDragWindow = 0;
i32 WindowManager::sDragStartX = 0;
i32 WindowManager::sDragStartY = 0;
i32 WindowManager::sDragWindowX = 0;
i32 WindowManager::sDragWindowY = 0;
u32 WindowManager::sDragWindowW = 0;
u32 WindowManager::sDragWindowH = 0;
CursorType WindowManager::sCursorType = CursorType::Arrow;

u32 GraphicalConsole::sWindowId = 0;
u32 GraphicalConsole::sCursorCol = 0;
u32 GraphicalConsole::sCursorRow = 0;
u32 GraphicalConsole::sColumns = 80;
u32 GraphicalConsole::sRows = 25;
Color GraphicalConsole::sForeground(200, 200, 200);
Color GraphicalConsole::sBackground(30, 30, 30);
char GraphicalConsole::sBuffer[50][100];
bool GraphicalConsole::sInitialized = false;

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

}

Window::Window()
    : mX(0), mY(0), mWidth(400), mHeight(300),
      mFlags(WindowFlags::None), mBackgroundColor(240, 240, 240),
      mId(0), mValid(false) {
    mTitle[0] = '\0';
}

void Window::setTitle(const char* title) {
    strCopy(mTitle, title, sizeof(mTitle));
}

void Window::setPosition(i32 x, i32 y) {
    mX = x;
    mY = y;
}

void Window::setSize(u32 width, u32 height) {
    mWidth = width;
    mHeight = height;
}

Rect Window::bounds() const {
    return {mX, mY, mWidth, mHeight};
}

Rect Window::clientArea() const {
    i32 clientX = mX;
    i32 clientY = mY;
    u32 clientWidth = mWidth;
    u32 clientHeight = mHeight;
    
    if (hasFlag(mFlags, WindowFlags::HasBorder)) {
        clientX += BORDER_WIDTH;
        clientY += BORDER_WIDTH;
        clientWidth -= BORDER_WIDTH * 2;
        clientHeight -= BORDER_WIDTH * 2;
    }
    
    if (hasFlag(mFlags, WindowFlags::HasTitlebar)) {
        clientY += TITLEBAR_HEIGHT;
        clientHeight -= TITLEBAR_HEIGHT;
    }
    
    return {clientX, clientY, clientWidth, clientHeight};
}

Rect Window::titlebarArea() const {
    if (!hasFlag(mFlags, WindowFlags::HasTitlebar)) {
        return {0, 0, 0, 0};
    }
    
    i32 tbX = mX;
    i32 tbY = mY;
    u32 tbWidth = mWidth;
    
    if (hasFlag(mFlags, WindowFlags::HasBorder)) {
        tbX += BORDER_WIDTH;
        tbY += BORDER_WIDTH;
        tbWidth -= BORDER_WIDTH * 2;
    }
    
    return {tbX, tbY, tbWidth, TITLEBAR_HEIGHT};
}

void WindowManager::initialize() {
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        sWindows[i].setValid(false);
    }
    sWindowCount = 0;
    sFocusedWindow = 0;
    sNextWindowId = 1;
    sInitialized = true;
}

u32 WindowManager::createWindow(const char* title, i32 x, i32 y, u32 width, u32 height, WindowFlags flags) {
    if (!sInitialized) return 0;
    
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (!sWindows[i].valid()) {
            sWindows[i].setValid(true);
            sWindows[i].setId(sNextWindowId++);
            sWindows[i].setTitle(title);
            sWindows[i].setPosition(x, y);
            sWindows[i].setSize(width, height);
            sWindows[i].setFlags(flags);
            sWindowCount++;
            
            if (sFocusedWindow == 0) {
                sFocusedWindow = sWindows[i].id();
                sWindows[i].setFlags(flags | WindowFlags::Focused);
            }
            
            return sWindows[i].id();
        }
    }
    
    return 0;
}

void WindowManager::destroyWindow(u32 windowId) {
    apps::AppManager::destroyApp(windowId);

    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid() && sWindows[i].id() == windowId) {
            sWindows[i].setValid(false);
            sWindowCount--;
            
            if (sFocusedWindow == windowId) {
                sFocusedWindow = 0;
                for (u32 j = 0; j < MAX_WINDOWS; j++) {
                    if (sWindows[j].valid()) {
                        focusWindow(sWindows[j].id());
                        break;
                    }
                }
            }
            break;
        }
    }
}

Window* WindowManager::getWindow(u32 windowId) {
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid() && sWindows[i].id() == windowId) {
            return &sWindows[i];
        }
    }
    return nullptr;
}

Window* WindowManager::getWindowByIndex(u32 index) {
    u32 count = 0;
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid()) {
            if (count == index) {
                return &sWindows[i];
            }
            count++;
        }
    }
    return nullptr;
}

void WindowManager::focusWindow(u32 windowId) {
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid()) {
            WindowFlags flags = sWindows[i].flags();
            if (sWindows[i].id() == windowId) {
                sWindows[i].setFlags(flags | WindowFlags::Focused);
            } else {
                sWindows[i].setFlags(static_cast<WindowFlags>(
                    static_cast<u32>(flags) & ~static_cast<u32>(WindowFlags::Focused)));
            }
        }
    }
    sFocusedWindow = windowId;
}

void WindowManager::setWindowBackgroundColor(u32 windowId, Color color) {
    Window* win = getWindow(windowId);
    if (win) {
        win->setBackgroundColor(color);
    }
}

void WindowManager::cycleWindowFocus() {
    if (sWindowCount == 0) return;
    
    u32 currentIndex = 0;
    bool foundCurrent = false;
    
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid() && sWindows[i].id() == sFocusedWindow) {
            currentIndex = i;
            foundCurrent = true;
            break;
        }
    }
    
    if (!foundCurrent) {
        for (u32 i = 0; i < MAX_WINDOWS; i++) {
            if (sWindows[i].valid()) {
                focusWindow(sWindows[i].id());
                return;
            }
        }
        return;
    }
    
    for (u32 i = 1; i <= MAX_WINDOWS; i++) {
        u32 nextIndex = (currentIndex + i) % MAX_WINDOWS;
        if (sWindows[nextIndex].valid()) {
            focusWindow(sWindows[nextIndex].id());
            return;
        }
    }
}

void WindowManager::moveWindow(u32 windowId, i32 dx, i32 dy) {
    Window* win = getWindow(windowId);
    if (!win) return;
    if (!hasFlag(win->flags(), WindowFlags::Movable)) return;
    
    i32 newX = win->x() + dx;
    i32 newY = win->y() + dy;
    
    if (newX < 0) newX = 0;
    if (newY < 0) newY = 0;
    if (newX + static_cast<i32>(win->width()) > static_cast<i32>(Framebuffer::width())) {
        newX = static_cast<i32>(Framebuffer::width()) - static_cast<i32>(win->width());
    }
    if (newY + static_cast<i32>(win->height()) > static_cast<i32>(Framebuffer::height())) {
        newY = static_cast<i32>(Framebuffer::height()) - static_cast<i32>(win->height());
    }
    
    win->setPosition(newX, newY);
}

void WindowManager::resizeWindow(u32 windowId, i32 dw, i32 dh) {
    Window* win = getWindow(windowId);
    if (!win) return;
    if (!hasFlag(win->flags(), WindowFlags::Resizable)) return;
    
    i32 newWidth = static_cast<i32>(win->width()) + dw;
    i32 newHeight = static_cast<i32>(win->height()) + dh;
    
    if (newWidth < static_cast<i32>(MIN_WINDOW_WIDTH)) newWidth = MIN_WINDOW_WIDTH;
    if (newHeight < static_cast<i32>(MIN_WINDOW_HEIGHT)) newHeight = MIN_WINDOW_HEIGHT;
    
    if (win->x() + newWidth > static_cast<i32>(Framebuffer::width())) {
        newWidth = static_cast<i32>(Framebuffer::width()) - win->x();
    }
    if (win->y() + newHeight > static_cast<i32>(Framebuffer::height())) {
        newHeight = static_cast<i32>(Framebuffer::height()) - win->y();
    }
    
    win->setSize(static_cast<u32>(newWidth), static_cast<u32>(newHeight));
}

void WindowManager::moveFocusedWindow(i32 dx, i32 dy) {
    if (sFocusedWindow != 0) {
        moveWindow(sFocusedWindow, dx, dy);
    }
}

void WindowManager::resizeFocusedWindow(i32 dw, i32 dh) {
    if (sFocusedWindow != 0) {
        resizeWindow(sFocusedWindow, dw, dh);
    }
}

void WindowManager::render() {
    if (!sInitialized) return;
    
    Framebuffer::fillRect(0, 0, Framebuffer::width(), Framebuffer::height(), sDesktopColor);
    
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid() && sWindows[i].isVisible() && !sWindows[i].isFocused()) {
            renderWindow(&sWindows[i]);
            apps::AppManager::renderWindow(sWindows[i].id());
            if (sWindows[i].id() == GraphicalConsole::window()) {
                GraphicalConsole::render();
            }
        }
    }
    
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid() && sWindows[i].isVisible() && sWindows[i].isFocused()) {
            renderWindow(&sWindows[i]);
            apps::AppManager::renderWindow(sWindows[i].id());
            if (sWindows[i].id() == GraphicalConsole::window()) {
                GraphicalConsole::render();
            }
        }
    }
}

void WindowManager::renderWindow(Window* window) {
    if (!window || !window->isVisible()) return;
    
    if (hasFlag(window->flags(), WindowFlags::HasBorder)) {
        renderBorder(window);
    }
    
    if (hasFlag(window->flags(), WindowFlags::HasTitlebar)) {
        renderTitlebar(window);
    }
    
    renderClientArea(window);
}

void WindowManager::renderTitlebar(Window* window) {
    Rect tb = window->titlebarArea();
    
    Color tbColor = window->isFocused() ? Color(0, 80, 160) : Color(100, 100, 100);
    Framebuffer::fillRect(static_cast<u32>(tb.x), static_cast<u32>(tb.y), tb.width, tb.height, tbColor);
    
    i32 textX = tb.x + 8;
    i32 textY = tb.y + (TITLEBAR_HEIGHT - Font::CHAR_HEIGHT) / 2;
    drawText(textX, textY, window->title(), Color(255, 255, 255));
    
    u32 closeX = static_cast<u32>(tb.x) + tb.width - 20;
    u32 closeY = static_cast<u32>(tb.y) + 4;
    Framebuffer::fillRect(closeX, closeY, 16, 16, Color(200, 60, 60));
    drawText(static_cast<i32>(closeX + 4), static_cast<i32>(closeY + 2), "X", Color(255, 255, 255));
}

void WindowManager::renderBorder(Window* window) {
    Rect bounds = window->bounds();
    Color borderColor = window->isFocused() ? Color(0, 80, 160) : Color(100, 100, 100);
    
    Framebuffer::fillRect(static_cast<u32>(bounds.x), static_cast<u32>(bounds.y),
                          bounds.width, BORDER_WIDTH, borderColor);
    Framebuffer::fillRect(static_cast<u32>(bounds.x), static_cast<u32>(bounds.y + static_cast<i32>(bounds.height) - static_cast<i32>(BORDER_WIDTH)),
                          bounds.width, BORDER_WIDTH, borderColor);
    Framebuffer::fillRect(static_cast<u32>(bounds.x), static_cast<u32>(bounds.y),
                          BORDER_WIDTH, bounds.height, borderColor);
    Framebuffer::fillRect(static_cast<u32>(bounds.x + static_cast<i32>(bounds.width) - static_cast<i32>(BORDER_WIDTH)), static_cast<u32>(bounds.y),
                          BORDER_WIDTH, bounds.height, borderColor);
}

void WindowManager::renderClientArea(Window* window) {
    Rect client = window->clientArea();
    Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(client.y),
                          client.width, client.height, window->backgroundColor());
}

void WindowManager::drawText(i32 x, i32 y, const char* text, Color color) {
    while (*text) {
        const u8* glyph = Font::getGlyph(*text);
        for (u32 row = 0; row < Font::CHAR_HEIGHT; row++) {
            for (u32 col = 0; col < Font::CHAR_WIDTH; col++) {
                if (glyph[row] & (0x80 >> col)) {
                    i32 px = x + static_cast<i32>(col);
                    i32 py = y + static_cast<i32>(row);
                    if (px >= 0 && py >= 0) {
                        Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), color);
                    }
                }
            }
        }
        x += Font::CHAR_WIDTH;
        text++;
    }
}

void WindowManager::drawTextClipped(i32 x, i32 y, const char* text, Color color, const Rect& clip) {
    i32 clipRight = clip.x + static_cast<i32>(clip.width);
    i32 clipBottom = clip.y + static_cast<i32>(clip.height);
    
    while (*text) {
        const u8* glyph = Font::getGlyph(*text);
        for (u32 row = 0; row < Font::CHAR_HEIGHT; row++) {
            for (u32 col = 0; col < Font::CHAR_WIDTH; col++) {
                if (glyph[row] & (0x80 >> col)) {
                    i32 px = x + static_cast<i32>(col);
                    i32 py = y + static_cast<i32>(row);
                    if (px >= clip.x && px < clipRight && py >= clip.y && py < clipBottom) {
                        Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), color);
                    }
                }
            }
        }
        x += Font::CHAR_WIDTH;
        text++;
    }
}

void GraphicalConsole::initialize(u32 windowId) {
    sWindowId = windowId;
    sCursorCol = 0;
    sCursorRow = 0;
    sForeground = Color(200, 200, 200);
    sBackground = Color(30, 30, 30);
    
    Window* win = WindowManager::getWindow(windowId);
    if (win) {
        Rect client = win->clientArea();
        sColumns = client.width / Font::CHAR_WIDTH;
        sRows = client.height / (Font::CHAR_HEIGHT + 2);
        if (sColumns > 100) sColumns = 100;
        if (sRows > 50) sRows = 50;
    }
    
    for (u32 r = 0; r < 50; r++) {
        for (u32 c = 0; c < 100; c++) {
            sBuffer[r][c] = '\0';
        }
    }
    
    sInitialized = true;
}

void GraphicalConsole::setWindow(u32 windowId) {
    sWindowId = windowId;
    Window* win = WindowManager::getWindow(windowId);
    if (win) {
        Rect client = win->clientArea();
        sColumns = client.width / Font::CHAR_WIDTH;
        sRows = client.height / (Font::CHAR_HEIGHT + 2);
        if (sColumns > 100) sColumns = 100;
        if (sRows > 50) sRows = 50;
    }
}

void GraphicalConsole::clear() {
    for (u32 r = 0; r < sRows; r++) {
        for (u32 c = 0; c < sColumns; c++) {
            sBuffer[r][c] = '\0';
        }
    }
    sCursorCol = 0;
    sCursorRow = 0;
}

void GraphicalConsole::putChar(char c) {
    if (!sInitialized) return;
    
    if (c == '\n') {
        newLine();
        return;
    }
    
    if (c == '\r') {
        sCursorCol = 0;
        return;
    }
    
    if (c == '\b') {
        if (sCursorCol > 0) {
            sCursorCol--;
            sBuffer[sCursorRow][sCursorCol] = '\0';
        }
        return;
    }
    
    if (c == '\t') {
        u32 spaces = 4 - (sCursorCol % 4);
        for (u32 i = 0; i < spaces && sCursorCol < sColumns; i++) {
            sBuffer[sCursorRow][sCursorCol] = ' ';
            sCursorCol++;
        }
        return;
    }
    
    if (sCursorCol >= sColumns) {
        newLine();
    }
    
    sBuffer[sCursorRow][sCursorCol] = c;
    sCursorCol++;
}

void GraphicalConsole::print(const char* str) {
    while (*str) {
        putChar(*str++);
    }
}

void GraphicalConsole::println(const char* str) {
    print(str);
    newLine();
}

void GraphicalConsole::printHex(u64 value) {
    const char* hexChars = "0123456789ABCDEF";
    char buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    
    for (int i = 15; i >= 0; i--) {
        buffer[17 - i] = hexChars[(value >> (i * 4)) & 0xF];
    }
    buffer[18] = '\0';
    
    print(buffer);
}

void GraphicalConsole::printDec(u64 value) {
    if (value == 0) {
        putChar('0');
        return;
    }
    
    char buffer[21];
    int i = 20;
    buffer[i] = '\0';
    
    while (value > 0 && i > 0) {
        buffer[--i] = '0' + (value % 10);
        value /= 10;
    }
    
    print(&buffer[i]);
}

void GraphicalConsole::newLine() {
    sCursorCol = 0;
    sCursorRow++;
    
    if (sCursorRow >= sRows) {
        scroll();
        sCursorRow = sRows - 1;
    }
}

void GraphicalConsole::scroll() {
    for (u32 r = 0; r < sRows - 1; r++) {
        for (u32 c = 0; c < sColumns; c++) {
            sBuffer[r][c] = sBuffer[r + 1][c];
        }
    }
    
    for (u32 c = 0; c < sColumns; c++) {
        sBuffer[sRows - 1][c] = '\0';
    }
}

void GraphicalConsole::render() {
    if (!sInitialized) return;
    
    Window* win = WindowManager::getWindow(sWindowId);
    if (!win) return;
    
    Rect client = win->clientArea();
    
    Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(client.y),
                          client.width, client.height, sBackground);
    
    for (u32 r = 0; r < sRows; r++) {
        for (u32 c = 0; c < sColumns; c++) {
            char ch = sBuffer[r][c];
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
                                Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), sForeground);
                            }
                        }
                    }
                }
            }
        }
    }
    
    u32 cursorX = sCursorCol * Font::CHAR_WIDTH + 4;
    u32 cursorY = sCursorRow * (Font::CHAR_HEIGHT + 2) + 4;
    Framebuffer::fillRect(static_cast<u32>(client.x) + cursorX,
                          static_cast<u32>(client.y) + cursorY,
                          Font::CHAR_WIDTH, Font::CHAR_HEIGHT, sForeground);
}

u32 WindowManager::windowAtPoint(i32 x, i32 y) {
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid() && sWindows[i].isVisible() && sWindows[i].isFocused()) {
            if (sWindows[i].bounds().contains(x, y)) {
                return sWindows[i].id();
            }
        }
    }
    
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (sWindows[i].valid() && sWindows[i].isVisible()) {
            if (sWindows[i].bounds().contains(x, y)) {
                return sWindows[i].id();
            }
        }
    }
    
    return 0;
}

DragMode WindowManager::hitTest(Window* window, i32 x, i32 y) {
    if (!window) return DragMode::None;
    
    Rect bounds = window->bounds();
    
    bool onLeft = x >= bounds.x && x < bounds.x + static_cast<i32>(RESIZE_BORDER);
    bool onRight = x >= bounds.x + static_cast<i32>(bounds.width) - static_cast<i32>(RESIZE_BORDER) &&
                   x < bounds.x + static_cast<i32>(bounds.width);
    bool onTop = y >= bounds.y && y < bounds.y + static_cast<i32>(RESIZE_BORDER);
    bool onBottom = y >= bounds.y + static_cast<i32>(bounds.height) - static_cast<i32>(RESIZE_BORDER) &&
                    y < bounds.y + static_cast<i32>(bounds.height);
    
    if (hasFlag(window->flags(), WindowFlags::Resizable)) {
        if (onTop && onLeft) return DragMode::ResizeNW;
        if (onTop && onRight) return DragMode::ResizeNE;
        if (onBottom && onLeft) return DragMode::ResizeSW;
        if (onBottom && onRight) return DragMode::ResizeSE;
        if (onTop) return DragMode::ResizeN;
        if (onBottom) return DragMode::ResizeS;
        if (onLeft) return DragMode::ResizeW;
        if (onRight) return DragMode::ResizeE;
    }
    
    if (hasFlag(window->flags(), WindowFlags::HasTitlebar) &&
        hasFlag(window->flags(), WindowFlags::Movable)) {
        Rect titlebar = window->titlebarArea();
        if (titlebar.contains(x, y)) {
            return DragMode::Move;
        }
    }
    
    return DragMode::None;
}

void WindowManager::handleMouseEvent(const MouseEvent& event) {
    i32 mx = Mouse::x();
    i32 my = Mouse::y();
    
    if (hasButton(event.pressed, MouseButton::Left)) {
        handleMouseDown(MouseButton::Left, mx, my);
    }
    
    if (hasButton(event.released, MouseButton::Left)) {
        handleMouseUp(MouseButton::Left, mx, my);
    }
    
    if (event.deltaX != 0 || event.deltaY != 0) {
        handleMouseMove(mx, my);
    }
}

void WindowManager::handleMouseDown(MouseButton button, i32 x, i32 y) {
    if (button != MouseButton::Left) return;
    
    u32 winId = windowAtPoint(x, y);
    if (winId == 0) return;
    
    if (winId != sFocusedWindow) {
        focusWindow(winId);
    }
    
    Window* win = getWindow(winId);
    if (!win) return;
    
    if (hasFlag(win->flags(), WindowFlags::HasTitlebar)) {
        Rect titlebar = win->titlebarArea();
        i32 closeX = titlebar.x + static_cast<i32>(titlebar.width) - 20;
        i32 closeY = titlebar.y + 4;
        if (x >= closeX && x < closeX + 16 && y >= closeY && y < closeY + 16) {
            destroyWindow(winId);
            return;
        }
    }
    
    DragMode mode = hitTest(win, x, y);
    if (mode != DragMode::None) {
        sDragMode = mode;
        sDragWindow = winId;
        sDragStartX = x;
        sDragStartY = y;
        sDragWindowX = win->x();
        sDragWindowY = win->y();
        sDragWindowW = win->width();
        sDragWindowH = win->height();
    }
}

void WindowManager::handleMouseUp(MouseButton button, i32 x, i32 y) {
    (void)x;
    (void)y;
    
    if (button != MouseButton::Left) return;
    
    sDragMode = DragMode::None;
    sDragWindow = 0;
}

void WindowManager::handleMouseMove(i32 x, i32 y) {
    if (sDragMode == DragMode::None) {
        u32 winId = windowAtPoint(x, y);
        if (winId != 0) {
            Window* win = getWindow(winId);
            DragMode mode = hitTest(win, x, y);
            
            switch (mode) {
                case DragMode::Move:
                    sCursorType = CursorType::Hand;
                    break;
                case DragMode::ResizeN:
                case DragMode::ResizeS:
                    sCursorType = CursorType::ResizeV;
                    break;
                case DragMode::ResizeE:
                case DragMode::ResizeW:
                    sCursorType = CursorType::ResizeH;
                    break;
                case DragMode::ResizeNE:
                case DragMode::ResizeSW:
                    sCursorType = CursorType::ResizeNESW;
                    break;
                case DragMode::ResizeNW:
                case DragMode::ResizeSE:
                    sCursorType = CursorType::ResizeNWSE;
                    break;
                default:
                    sCursorType = CursorType::Arrow;
                    break;
            }
        } else {
            sCursorType = CursorType::Arrow;
        }
        return;
    }
    
    Window* win = getWindow(sDragWindow);
    if (!win) {
        sDragMode = DragMode::None;
        return;
    }
    
    i32 dx = x - sDragStartX;
    i32 dy = y - sDragStartY;
    
    switch (sDragMode) {
        case DragMode::Move: {
            i32 newX = sDragWindowX + dx;
            i32 newY = sDragWindowY + dy;
            
            if (newX < 0) newX = 0;
            if (newY < 0) newY = 0;
            if (newX + static_cast<i32>(win->width()) > static_cast<i32>(Framebuffer::width())) {
                newX = static_cast<i32>(Framebuffer::width()) - static_cast<i32>(win->width());
            }
            if (newY + static_cast<i32>(win->height()) > static_cast<i32>(Framebuffer::height())) {
                newY = static_cast<i32>(Framebuffer::height()) - static_cast<i32>(win->height());
            }
            
            win->setPosition(newX, newY);
            break;
        }
        
        case DragMode::ResizeE: {
            i32 newW = static_cast<i32>(sDragWindowW) + dx;
            if (newW < static_cast<i32>(MIN_WINDOW_WIDTH)) newW = MIN_WINDOW_WIDTH;
            if (sDragWindowX + newW > static_cast<i32>(Framebuffer::width())) {
                newW = static_cast<i32>(Framebuffer::width()) - sDragWindowX;
            }
            win->setSize(static_cast<u32>(newW), win->height());
            break;
        }
        
        case DragMode::ResizeW: {
            i32 newX = sDragWindowX + dx;
            i32 newW = static_cast<i32>(sDragWindowW) - dx;
            if (newW < static_cast<i32>(MIN_WINDOW_WIDTH)) {
                newW = MIN_WINDOW_WIDTH;
                newX = sDragWindowX + static_cast<i32>(sDragWindowW) - static_cast<i32>(MIN_WINDOW_WIDTH);
            }
            if (newX < 0) {
                newW += newX;
                newX = 0;
            }
            win->setPosition(newX, win->y());
            win->setSize(static_cast<u32>(newW), win->height());
            break;
        }
        
        case DragMode::ResizeS: {
            i32 newH = static_cast<i32>(sDragWindowH) + dy;
            if (newH < static_cast<i32>(MIN_WINDOW_HEIGHT)) newH = MIN_WINDOW_HEIGHT;
            if (sDragWindowY + newH > static_cast<i32>(Framebuffer::height())) {
                newH = static_cast<i32>(Framebuffer::height()) - sDragWindowY;
            }
            win->setSize(win->width(), static_cast<u32>(newH));
            break;
        }
        
        case DragMode::ResizeN: {
            i32 newY = sDragWindowY + dy;
            i32 newH = static_cast<i32>(sDragWindowH) - dy;
            if (newH < static_cast<i32>(MIN_WINDOW_HEIGHT)) {
                newH = MIN_WINDOW_HEIGHT;
                newY = sDragWindowY + static_cast<i32>(sDragWindowH) - static_cast<i32>(MIN_WINDOW_HEIGHT);
            }
            if (newY < 0) {
                newH += newY;
                newY = 0;
            }
            win->setPosition(win->x(), newY);
            win->setSize(win->width(), static_cast<u32>(newH));
            break;
        }
        
        case DragMode::ResizeSE: {
            i32 newW = static_cast<i32>(sDragWindowW) + dx;
            i32 newH = static_cast<i32>(sDragWindowH) + dy;
            if (newW < static_cast<i32>(MIN_WINDOW_WIDTH)) newW = MIN_WINDOW_WIDTH;
            if (newH < static_cast<i32>(MIN_WINDOW_HEIGHT)) newH = MIN_WINDOW_HEIGHT;
            if (sDragWindowX + newW > static_cast<i32>(Framebuffer::width())) {
                newW = static_cast<i32>(Framebuffer::width()) - sDragWindowX;
            }
            if (sDragWindowY + newH > static_cast<i32>(Framebuffer::height())) {
                newH = static_cast<i32>(Framebuffer::height()) - sDragWindowY;
            }
            win->setSize(static_cast<u32>(newW), static_cast<u32>(newH));
            break;
        }
        
        case DragMode::ResizeNW: {
            i32 newX = sDragWindowX + dx;
            i32 newY = sDragWindowY + dy;
            i32 newW = static_cast<i32>(sDragWindowW) - dx;
            i32 newH = static_cast<i32>(sDragWindowH) - dy;
            
            if (newW < static_cast<i32>(MIN_WINDOW_WIDTH)) {
                newW = MIN_WINDOW_WIDTH;
                newX = sDragWindowX + static_cast<i32>(sDragWindowW) - static_cast<i32>(MIN_WINDOW_WIDTH);
            }
            if (newH < static_cast<i32>(MIN_WINDOW_HEIGHT)) {
                newH = MIN_WINDOW_HEIGHT;
                newY = sDragWindowY + static_cast<i32>(sDragWindowH) - static_cast<i32>(MIN_WINDOW_HEIGHT);
            }
            if (newX < 0) { newW += newX; newX = 0; }
            if (newY < 0) { newH += newY; newY = 0; }
            
            win->setPosition(newX, newY);
            win->setSize(static_cast<u32>(newW), static_cast<u32>(newH));
            break;
        }
        
        case DragMode::ResizeNE: {
            i32 newY = sDragWindowY + dy;
            i32 newW = static_cast<i32>(sDragWindowW) + dx;
            i32 newH = static_cast<i32>(sDragWindowH) - dy;
            
            if (newW < static_cast<i32>(MIN_WINDOW_WIDTH)) newW = MIN_WINDOW_WIDTH;
            if (newH < static_cast<i32>(MIN_WINDOW_HEIGHT)) {
                newH = MIN_WINDOW_HEIGHT;
                newY = sDragWindowY + static_cast<i32>(sDragWindowH) - static_cast<i32>(MIN_WINDOW_HEIGHT);
            }
            if (sDragWindowX + newW > static_cast<i32>(Framebuffer::width())) {
                newW = static_cast<i32>(Framebuffer::width()) - sDragWindowX;
            }
            if (newY < 0) { newH += newY; newY = 0; }
            
            win->setPosition(win->x(), newY);
            win->setSize(static_cast<u32>(newW), static_cast<u32>(newH));
            break;
        }
        
        case DragMode::ResizeSW: {
            i32 newX = sDragWindowX + dx;
            i32 newW = static_cast<i32>(sDragWindowW) - dx;
            i32 newH = static_cast<i32>(sDragWindowH) + dy;
            
            if (newW < static_cast<i32>(MIN_WINDOW_WIDTH)) {
                newW = MIN_WINDOW_WIDTH;
                newX = sDragWindowX + static_cast<i32>(sDragWindowW) - static_cast<i32>(MIN_WINDOW_WIDTH);
            }
            if (newH < static_cast<i32>(MIN_WINDOW_HEIGHT)) newH = MIN_WINDOW_HEIGHT;
            if (newX < 0) { newW += newX; newX = 0; }
            if (sDragWindowY + newH > static_cast<i32>(Framebuffer::height())) {
                newH = static_cast<i32>(Framebuffer::height()) - sDragWindowY;
            }
            
            win->setPosition(newX, win->y());
            win->setSize(static_cast<u32>(newW), static_cast<u32>(newH));
            break;
        }
        
        default:
            break;
    }
}

void WindowManager::drawCursorArrow(i32 x, i32 y) {
    (void)x;
    (void)y;
}

void WindowManager::drawCursorHand(i32 x, i32 y) {
    (void)x;
    (void)y;
}

void WindowManager::drawCursorResizeH(i32 x, i32 y) {
    (void)x;
    (void)y;
}

void WindowManager::drawCursorResizeV(i32 x, i32 y) {
    (void)x;
    (void)y;
}

void WindowManager::drawCursorResizeDiag(i32 x, i32 y, bool nwse) {
    (void)x;
    (void)y;
    (void)nwse;
}

void WindowManager::renderCursor() {
    i32 mx = Mouse::x();
    i32 my = Mouse::y();
    
    constexpr u32 BLOCK_SIZE = 8;
    
    Color white(255, 255, 255);
    Color black(0, 0, 0);
    
    for (u32 dy = 0; dy < BLOCK_SIZE; dy++) {
        for (u32 dx = 0; dx < BLOCK_SIZE; dx++) {
            i32 px = mx + static_cast<i32>(dx);
            i32 py = my + static_cast<i32>(dy);
            
            if (px >= 0 && py >= 0 && 
                px < static_cast<i32>(Framebuffer::width()) && 
                py < static_cast<i32>(Framebuffer::height())) {
                
                bool isBorder = (dx == 0 || dx == BLOCK_SIZE - 1 || 
                                 dy == 0 || dy == BLOCK_SIZE - 1);
                
                Framebuffer::putPixel(static_cast<u32>(px), static_cast<u32>(py), 
                                      isBorder ? black : white);
            }
        }
    }
}

AppInfo StartMenu::sApps[MAX_APPS];
u32 StartMenu::sAppCount = 0;
bool StartMenu::sVisible = false;
bool StartMenu::sInitialized = false;
i32 StartMenu::sHoveredItem = -1;
AppLaunchCallback StartMenu::sLaunchCallback = nullptr;

bool Taskbar::sInitialized = false;
bool Taskbar::sStartHovered = false;

void StartMenu::initialize() {
    sAppCount = 0;
    sVisible = false;
    sHoveredItem = -1;
    sLaunchCallback = nullptr;
    
    registerApps();
    
    sInitialized = true;
}

void StartMenu::registerApps() {
    strCopy(sApps[sAppCount].name, "Terminal", 32);
    sApps[sAppCount].type = AppType::Terminal;
    sApps[sAppCount].available = true;
    sAppCount++;
    
    strCopy(sApps[sAppCount].name, "File Manager", 32);
    sApps[sAppCount].type = AppType::FileManager;
    sApps[sAppCount].available = true;
    sAppCount++;
    
    strCopy(sApps[sAppCount].name, "Text Editor", 32);
    sApps[sAppCount].type = AppType::TextEditor;
    sApps[sAppCount].available = true;
    sAppCount++;
    
    strCopy(sApps[sAppCount].name, "Settings", 32);
    sApps[sAppCount].type = AppType::Settings;
    sApps[sAppCount].available = true;
    sAppCount++;
    
    strCopy(sApps[sAppCount].name, "About SertOS", 32);
    sApps[sAppCount].type = AppType::About;
    sApps[sAppCount].available = true;
    sAppCount++;

    strCopy(sApps[sAppCount].name, "Web Browser", 32);
    sApps[sAppCount].type = AppType::Browser;
    sApps[sAppCount].available = true;
    sAppCount++;
}

void StartMenu::show() {
    sVisible = true;
    sHoveredItem = -1;
}

void StartMenu::hide() {
    sVisible = false;
    sHoveredItem = -1;
}

void StartMenu::toggle() {
    if (sVisible) {
        hide();
    } else {
        show();
    }
}

Rect StartMenu::bounds() {
    constexpr u32 MENU_WIDTH = 200;
    constexpr u32 ITEM_HEIGHT = 28;
    u32 menuHeight = sAppCount * ITEM_HEIGHT + 8;
    
    i32 x = 0;
    i32 y = static_cast<i32>(Framebuffer::height()) - static_cast<i32>(TASKBAR_HEIGHT) - static_cast<i32>(menuHeight);
    
    return {x, y, MENU_WIDTH, menuHeight};
}

const AppInfo* StartMenu::getApp(u32 index) {
    if (index < sAppCount) {
        return &sApps[index];
    }
    return nullptr;
}

void StartMenu::render() {
    if (!sVisible || !sInitialized) return;
    
    Rect menuBounds = bounds();
    
    Color menuBg(50, 50, 60);
    Color menuBorder(80, 80, 100);
    
    Framebuffer::fillRect(static_cast<u32>(menuBounds.x), static_cast<u32>(menuBounds.y),
                          menuBounds.width, menuBounds.height, menuBg);
    
    Framebuffer::fillRect(static_cast<u32>(menuBounds.x), static_cast<u32>(menuBounds.y),
                          menuBounds.width, 2, menuBorder);
    Framebuffer::fillRect(static_cast<u32>(menuBounds.x + static_cast<i32>(menuBounds.width) - 2), 
                          static_cast<u32>(menuBounds.y),
                          2, menuBounds.height, menuBorder);
    
    constexpr u32 ITEM_HEIGHT = 28;
    i32 itemY = menuBounds.y + 4;
    
    for (u32 i = 0; i < sAppCount; i++) {
        bool highlighted = (static_cast<i32>(i) == sHoveredItem);
        renderMenuItem(i, menuBounds.x + 4, itemY, highlighted);
        itemY += ITEM_HEIGHT;
    }
}

void StartMenu::renderMenuItem(u32 index, i32 x, i32 y, bool highlighted) {
    constexpr u32 ITEM_WIDTH = 192;
    constexpr u32 ITEM_HEIGHT = 24;
    
    Color bgColor = highlighted ? Color(70, 100, 150) : Color(50, 50, 60);
    Color textColor = sApps[index].available ? Color(220, 220, 220) : Color(120, 120, 120);
    
    Framebuffer::fillRect(static_cast<u32>(x), static_cast<u32>(y), ITEM_WIDTH, ITEM_HEIGHT, bgColor);
    
    i32 textX = x + 8;
    i32 textY = y + (ITEM_HEIGHT - Font::CHAR_HEIGHT) / 2;
    WindowManager::drawText(textX, textY, sApps[index].name, textColor);
}

bool StartMenu::handleClick(i32 x, i32 y) {
    if (!sVisible) return false;
    
    Rect menuBounds = bounds();
    if (!menuBounds.contains(x, y)) {
        hide();
        return true;
    }
    
    constexpr u32 ITEM_HEIGHT = 28;
    i32 relY = y - menuBounds.y - 4;
    
    if (relY >= 0) {
        u32 itemIndex = static_cast<u32>(relY) / ITEM_HEIGHT;
        if (itemIndex < sAppCount && sApps[itemIndex].available) {
            if (sLaunchCallback) {
                sLaunchCallback(sApps[itemIndex].type);
            }
            hide();
            return true;
        }
    }
    
    return true;
}

void Taskbar::initialize() {
    sStartHovered = false;
    sInitialized = true;
    
    StartMenu::initialize();
}

Rect Taskbar::bounds() {
    return {
        0,
        static_cast<i32>(Framebuffer::height()) - static_cast<i32>(TASKBAR_HEIGHT),
        Framebuffer::width(),
        TASKBAR_HEIGHT
    };
}

Rect Taskbar::startButtonBounds() {
    return {
        0,
        static_cast<i32>(Framebuffer::height()) - static_cast<i32>(TASKBAR_HEIGHT),
        START_BUTTON_WIDTH,
        TASKBAR_HEIGHT
    };
}

u32 Taskbar::usableScreenHeight() {
    return Framebuffer::height() - TASKBAR_HEIGHT;
}

void Taskbar::render() {
    if (!sInitialized) return;
    
    Rect tb = bounds();
    
    Color taskbarBg(40, 40, 50);
    Framebuffer::fillRect(static_cast<u32>(tb.x), static_cast<u32>(tb.y),
                          tb.width, tb.height, taskbarBg);
    
    Color topBorder(60, 60, 80);
    Framebuffer::fillRect(static_cast<u32>(tb.x), static_cast<u32>(tb.y),
                          tb.width, 1, topBorder);
    
    renderStartButton();
    renderWindowButtons();
    
    StartMenu::render();
}

void Taskbar::renderStartButton() {
    Rect btn = startButtonBounds();
    
    Color btnBg = sStartHovered || StartMenu::isVisible() ? Color(60, 80, 120) : Color(50, 60, 80);
    Framebuffer::fillRect(static_cast<u32>(btn.x), static_cast<u32>(btn.y) + 1,
                          btn.width, btn.height - 1, btnBg);
    
    Color borderColor(80, 100, 140);
    Framebuffer::fillRect(static_cast<u32>(btn.x + static_cast<i32>(btn.width) - 1), 
                          static_cast<u32>(btn.y) + 1,
                          1, btn.height - 1, borderColor);
    
    i32 textX = btn.x + (static_cast<i32>(btn.width) - 5 * static_cast<i32>(Font::CHAR_WIDTH)) / 2;
    i32 textY = btn.y + (static_cast<i32>(btn.height) - static_cast<i32>(Font::CHAR_HEIGHT)) / 2;
    WindowManager::drawText(textX, textY, "Start", Color(220, 220, 220));
}

void Taskbar::renderWindowButtons() {
    constexpr u32 BUTTON_WIDTH = 150;
    constexpr u32 BUTTON_HEIGHT = 26;
    constexpr u32 BUTTON_MARGIN = 4;
    
    i32 buttonX = static_cast<i32>(START_BUTTON_WIDTH) + 8;
    i32 buttonY = static_cast<i32>(Framebuffer::height()) - static_cast<i32>(TASKBAR_HEIGHT) + 3;
    
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        Window* win = WindowManager::getWindow(i + 1);
        if (!win || !win->valid() || !win->isVisible()) continue;
        
        bool isFocused = win->isFocused();
        Color btnBg = isFocused ? Color(70, 100, 150) : Color(55, 55, 65);
        
        Framebuffer::fillRect(static_cast<u32>(buttonX), static_cast<u32>(buttonY),
                              BUTTON_WIDTH, BUTTON_HEIGHT, btnBg);
        
        Color borderColor = isFocused ? Color(100, 140, 200) : Color(70, 70, 80);
        Framebuffer::fillRect(static_cast<u32>(buttonX), static_cast<u32>(buttonY),
                              BUTTON_WIDTH, 1, borderColor);
        Framebuffer::fillRect(static_cast<u32>(buttonX), static_cast<u32>(buttonY),
                              1, BUTTON_HEIGHT, borderColor);
        Framebuffer::fillRect(static_cast<u32>(buttonX + static_cast<i32>(BUTTON_WIDTH) - 1), 
                              static_cast<u32>(buttonY),
                              1, BUTTON_HEIGHT, borderColor);
        Framebuffer::fillRect(static_cast<u32>(buttonX), 
                              static_cast<u32>(buttonY + static_cast<i32>(BUTTON_HEIGHT) - 1),
                              BUTTON_WIDTH, 1, borderColor);
        
        i32 textX = buttonX + 6;
        i32 textY = buttonY + (static_cast<i32>(BUTTON_HEIGHT) - static_cast<i32>(Font::CHAR_HEIGHT)) / 2;
        
        char truncatedTitle[20];
        const char* title = win->title();
        u32 titleLen = 0;
        while (title[titleLen] && titleLen < 18) {
            truncatedTitle[titleLen] = title[titleLen];
            titleLen++;
        }
        if (title[titleLen]) {
            truncatedTitle[titleLen++] = '.';
            truncatedTitle[titleLen++] = '.';
        }
        truncatedTitle[titleLen] = '\0';
        
        WindowManager::drawText(textX, textY, truncatedTitle, Color(200, 200, 200));
        
        buttonX += static_cast<i32>(BUTTON_WIDTH) + static_cast<i32>(BUTTON_MARGIN);
        
        if (buttonX + static_cast<i32>(BUTTON_WIDTH) > static_cast<i32>(Framebuffer::width()) - 100) {
            break;
        }
    }
}

void Taskbar::renderClock() {
}

bool Taskbar::handleClick(i32 x, i32 y) {
    Rect tb = bounds();
    if (!tb.contains(x, y) && !StartMenu::isVisible()) {
        return false;
    }
    
    if (StartMenu::isVisible()) {
        Rect menuBounds = StartMenu::bounds();
        if (menuBounds.contains(x, y)) {
            return StartMenu::handleClick(x, y);
        }
        
        Rect startBtn = startButtonBounds();
        if (startBtn.contains(x, y)) {
            StartMenu::toggle();
            return true;
        }
        
        StartMenu::hide();
        return tb.contains(x, y);
    }
    
    Rect startBtn = startButtonBounds();
    if (startBtn.contains(x, y)) {
        StartMenu::toggle();
        return true;
    }
    
    constexpr u32 BUTTON_WIDTH = 150;
    constexpr u32 BUTTON_HEIGHT = 26;
    constexpr u32 BUTTON_MARGIN = 4;
    
    i32 buttonX = static_cast<i32>(START_BUTTON_WIDTH) + 8;
    i32 buttonY = static_cast<i32>(Framebuffer::height()) - static_cast<i32>(TASKBAR_HEIGHT) + 3;
    
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        Window* win = WindowManager::getWindow(i + 1);
        if (!win || !win->valid() || !win->isVisible()) continue;
        
        Rect btnRect = {buttonX, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT};
        if (btnRect.contains(x, y)) {
            WindowManager::focusWindow(win->id());
            return true;
        }
        
        buttonX += static_cast<i32>(BUTTON_WIDTH) + static_cast<i32>(BUTTON_MARGIN);
        
        if (buttonX + static_cast<i32>(BUTTON_WIDTH) > static_cast<i32>(Framebuffer::width()) - 100) {
            break;
        }
    }
    
    return true;
}

void Taskbar::handleMouseMove(i32 x, i32 y) {
    Rect startBtn = startButtonBounds();
    sStartHovered = startBtn.contains(x, y);
    
    if (StartMenu::isVisible()) {
        Rect menuBounds = StartMenu::bounds();
        if (menuBounds.contains(x, y)) {
            constexpr u32 ITEM_HEIGHT = 28;
            i32 relY = y - menuBounds.y - 4;
            if (relY >= 0) {
                StartMenu::sHoveredItem = static_cast<i32>(static_cast<u32>(relY) / ITEM_HEIGHT);
                if (static_cast<u32>(StartMenu::sHoveredItem) >= StartMenu::sAppCount) {
                    StartMenu::sHoveredItem = -1;
                }
            }
        } else {
            StartMenu::sHoveredItem = -1;
        }
    }
}

}
