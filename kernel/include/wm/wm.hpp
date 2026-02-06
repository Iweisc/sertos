#pragma once

#include "../types.hpp"
#include "../graphics/framebuffer.hpp"
#include "../graphics/font.hpp"
#include "../input/mouse.hpp"

namespace sertos::wm {

using graphics::Color;
using graphics::Font;
using graphics::Framebuffer;
using input::Mouse;
using input::MouseButton;
using input::MouseEvent;

constexpr u32 MAX_WINDOWS = 32;
constexpr u32 TITLEBAR_HEIGHT = 24;
constexpr u32 BORDER_WIDTH = 2;
constexpr u32 RESIZE_BORDER = 6;
constexpr u32 CURSOR_SIZE = 12;
constexpr u32 TASKBAR_HEIGHT = 32;
constexpr u32 START_BUTTON_WIDTH = 80;
constexpr u32 MAX_APPS = 16;

enum class WindowFlags : u32 {
    None = 0,
    Visible = 1 << 0,
    Focused = 1 << 1,
    Movable = 1 << 2,
    Resizable = 1 << 3,
    HasTitlebar = 1 << 4,
    HasBorder = 1 << 5,
    Maximized = 1 << 6,
    Minimized = 1 << 7
};

inline WindowFlags operator|(WindowFlags a, WindowFlags b) {
    return static_cast<WindowFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline WindowFlags operator&(WindowFlags a, WindowFlags b) {
    return static_cast<WindowFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}

inline bool hasFlag(WindowFlags flags, WindowFlags flag) {
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

struct Rect {
    i32 x, y;
    u32 width, height;
    
    bool contains(i32 px, i32 py) const {
        return px >= x && px < x + static_cast<i32>(width) &&
               py >= y && py < y + static_cast<i32>(height);
    }
};

class Window {
public:
    Window();
    
    void setTitle(const char* title);
    const char* title() const { return mTitle; }
    
    void setPosition(i32 x, i32 y);
    void setSize(u32 width, u32 height);
    void setFlags(WindowFlags flags) { mFlags = flags; }
    WindowFlags flags() const { return mFlags; }
    
    i32 x() const { return mX; }
    i32 y() const { return mY; }
    u32 width() const { return mWidth; }
    u32 height() const { return mHeight; }
    
    Rect bounds() const;
    Rect clientArea() const;
    Rect titlebarArea() const;
    
    bool isVisible() const { return hasFlag(mFlags, WindowFlags::Visible); }
    bool isFocused() const { return hasFlag(mFlags, WindowFlags::Focused); }
    
    void setBackgroundColor(Color color) { mBackgroundColor = color; }
    Color backgroundColor() const { return mBackgroundColor; }
    
    u32 id() const { return mId; }
    void setId(u32 id) { mId = id; }
    
    bool valid() const { return mValid; }
    void setValid(bool valid) { mValid = valid; }

private:
    char mTitle[64];
    i32 mX, mY;
    u32 mWidth, mHeight;
    WindowFlags mFlags;
    Color mBackgroundColor;
    u32 mId;
    bool mValid;
};

enum class DragMode : u8 {
    None = 0,
    Move,
    ResizeN,
    ResizeS,
    ResizeE,
    ResizeW,
    ResizeNE,
    ResizeNW,
    ResizeSE,
    ResizeSW
};

enum class CursorType : u8 {
    Arrow = 0,
    Hand,
    ResizeH,
    ResizeV,
    ResizeNESW,
    ResizeNWSE
};

class WindowManager {
public:
    static void initialize();
    static bool isInitialized() { return sInitialized; }
    
    static u32 createWindow(const char* title, i32 x, i32 y, u32 width, u32 height, WindowFlags flags);
    static void destroyWindow(u32 windowId);
    static Window* getWindow(u32 windowId);
    static Window* getWindowByIndex(u32 index);
    
    static void focusWindow(u32 windowId);
    static u32 focusedWindow() { return sFocusedWindow; }
    
    static void setDesktopColor(Color color) { sDesktopColor = color; }
    static Color desktopColor() { return sDesktopColor; }
    
    static void setWindowBackgroundColor(u32 windowId, Color color);
    
    static void render();
    static void renderWindow(Window* window);
    
    static u32 windowCount() { return sWindowCount; }
    
    static void cycleWindowFocus();
    
    static void moveWindow(u32 windowId, i32 dx, i32 dy);
    static void resizeWindow(u32 windowId, i32 dw, i32 dh);
    static void moveFocusedWindow(i32 dx, i32 dy);
    static void resizeFocusedWindow(i32 dw, i32 dh);
    
    static void handleMouseEvent(const MouseEvent& event);
    static void handleMouseMove(i32 x, i32 y);
    static void handleMouseDown(MouseButton button, i32 x, i32 y);
    static void handleMouseUp(MouseButton button, i32 x, i32 y);
    
    static void renderCursor();
    static void setCursorType(CursorType type) { sCursorType = type; }
    static CursorType cursorType() { return sCursorType; }
    
    static u32 windowAtPoint(i32 x, i32 y);
    static DragMode hitTest(Window* window, i32 x, i32 y);
    
    static constexpr i32 MOVE_STEP = 20;
    static constexpr i32 RESIZE_STEP = 20;
    static constexpr u32 MIN_WINDOW_WIDTH = 100;
    static constexpr u32 MIN_WINDOW_HEIGHT = 80;
    
    static void drawText(i32 x, i32 y, const char* text, Color color);
    static void drawTextClipped(i32 x, i32 y, const char* text, Color color, const Rect& clip);

private:
    static void renderTitlebar(Window* window);
    static void renderBorder(Window* window);
    static void renderClientArea(Window* window);
    static void drawCursorArrow(i32 x, i32 y);
    static void drawCursorHand(i32 x, i32 y);
    static void drawCursorResizeH(i32 x, i32 y);
    static void drawCursorResizeV(i32 x, i32 y);
    static void drawCursorResizeDiag(i32 x, i32 y, bool nwse);
    
    static Window sWindows[MAX_WINDOWS];
    static u32 sWindowCount;
    static u32 sFocusedWindow;
    static Color sDesktopColor;
    static bool sInitialized;
    static u32 sNextWindowId;
    
    static DragMode sDragMode;
    static u32 sDragWindow;
    static i32 sDragStartX;
    static i32 sDragStartY;
    static i32 sDragWindowX;
    static i32 sDragWindowY;
    static u32 sDragWindowW;
    static u32 sDragWindowH;
    static CursorType sCursorType;
};

class GraphicalConsole {
public:
    static void initialize(u32 windowId);
    static void setWindow(u32 windowId);
    static u32 window() { return sWindowId; }
    
    static void clear();
    static void putChar(char c);
    static void print(const char* str);
    static void println(const char* str);
    
    static void printHex(u64 value);
    static void printDec(u64 value);
    
    static void setForeground(Color color) { sForeground = color; }
    static void setBackground(Color color) { sBackground = color; }
    
    static void render();
    static void scroll();
    
    static u32 columns() { return sColumns; }
    static u32 rows() { return sRows; }
    static u32 cursorCol() { return sCursorCol; }
    static u32 cursorRow() { return sCursorRow; }

private:
    static void newLine();
    
    static u32 sWindowId;
    static u32 sCursorCol;
    static u32 sCursorRow;
    static u32 sColumns;
    static u32 sRows;
    static Color sForeground;
    static Color sBackground;
    static char sBuffer[50][100];
    static bool sInitialized;
};

enum class AppType : u8 {
    Terminal = 0,
    FileManager,
    TextEditor,
    Settings,
    About,
    Browser
};

struct AppInfo {
    char name[32];
    AppType type;
    bool available;
};

using AppLaunchCallback = void (*)(AppType);

class StartMenu {
    friend class Taskbar;
public:
    static void initialize();
    static bool isInitialized() { return sInitialized; }
    
    static void show();
    static void hide();
    static bool isVisible() { return sVisible; }
    static void toggle();
    
    static void render();
    static bool handleClick(i32 x, i32 y);
    
    static void setLaunchCallback(AppLaunchCallback callback) { sLaunchCallback = callback; }
    
    static Rect bounds();
    static u32 appCount() { return sAppCount; }
    static const AppInfo* getApp(u32 index);

private:
    static void registerApps();
    static void renderMenuItem(u32 index, i32 x, i32 y, bool highlighted);
    
    static AppInfo sApps[MAX_APPS];
    static u32 sAppCount;
    static bool sVisible;
    static bool sInitialized;
    static i32 sHoveredItem;
    static AppLaunchCallback sLaunchCallback;
};

class Taskbar {
public:
    static void initialize();
    static bool isInitialized() { return sInitialized; }
    
    static void render();
    static bool handleClick(i32 x, i32 y);
    static void handleMouseMove(i32 x, i32 y);
    
    static Rect bounds();
    static Rect startButtonBounds();
    
    static u32 height() { return TASKBAR_HEIGHT; }
    static u32 usableScreenHeight();

private:
    static void renderStartButton();
    static void renderWindowButtons();
    static void renderClock();
    
    static bool sInitialized;
    static bool sStartHovered;
};

}
