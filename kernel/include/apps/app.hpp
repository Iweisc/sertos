#pragma once

#include "../types.hpp"
#include "../input/keyboard.hpp"
#include "../wm/wm.hpp"

namespace sertos::apps {

using wm::Window;
using wm::WindowManager;
using wm::Rect;
using graphics::Color;

class App {
public:
    virtual ~App() = default;
    
    virtual void render() = 0;
    virtual void handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) = 0;
    virtual void handleMouseClick(i32 x, i32 y, bool doubleClick) = 0;
    
    u32 windowId() const { return mWindowId; }
    void setWindowId(u32 id) { mWindowId = id; }
    
    bool isActive() const { return mActive; }
    void setActive(bool active) { mActive = active; }

protected:
    u32 mWindowId = 0;
    bool mActive = false;
};

class AppManager {
public:
    static void initialize();
    static bool isInitialized() { return sInitialized; }
    
    static App* createApp(wm::AppType type, u32 windowId);
    static void destroyApp(u32 windowId);
    static App* getApp(u32 windowId);
    
    static void renderAll();
    static void renderWindow(u32 windowId);
    static void handleKeyPress(u32 windowId, input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift);
    static void handleMouseClick(u32 windowId, i32 x, i32 y, bool doubleClick);

    static void openFileInTextEditor(const char* path);

private:
    static constexpr u32 MAX_APPS = 32;
    static App* sApps[MAX_APPS];
    static u32 sAppWindowIds[MAX_APPS];
    static u32 sAppCount;
    static bool sInitialized;
};

}
