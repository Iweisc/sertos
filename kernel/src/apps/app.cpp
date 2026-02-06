#include "../../include/apps/app.hpp"
#include "../../include/apps/about.hpp"
#include "../../include/apps/file_manager.hpp"
#include "../../include/apps/terminal.hpp"
#include "../../include/apps/text_editor.hpp"
#include "../../include/apps/browser.hpp"

namespace sertos::apps {

App* AppManager::sApps[MAX_APPS] = {nullptr};
u32 AppManager::sAppWindowIds[MAX_APPS] = {0};
u32 AppManager::sAppCount = 0;
bool AppManager::sInitialized = false;

void AppManager::initialize() {
    for (u32 i = 0; i < MAX_APPS; i++) {
        sApps[i] = nullptr;
        sAppWindowIds[i] = 0;
    }
    sAppCount = 0;
    sInitialized = true;
}

App* AppManager::createApp(wm::AppType type, u32 windowId) {
    if (!sInitialized || sAppCount >= MAX_APPS) return nullptr;
    
    App* app = nullptr;
    
    switch (type) {
        case wm::AppType::About:
            app = new AboutApp();
            break;
        case wm::AppType::FileManager:
            app = new FileManagerApp();
            break;
        case wm::AppType::Terminal:
            app = new TerminalApp();
            break;
        case wm::AppType::TextEditor:
            app = new TextEditorApp();
            break;
        case wm::AppType::Settings:
            break;
        case wm::AppType::Browser:
            app = new BrowserApp();
            break;
    }
    
    if (app) {
        app->setWindowId(windowId);
        app->setActive(true);
        
        for (u32 i = 0; i < MAX_APPS; i++) {
            if (sApps[i] == nullptr) {
                sApps[i] = app;
                sAppWindowIds[i] = windowId;
                sAppCount++;
                break;
            }
        }
    }
    
    return app;
}

void AppManager::destroyApp(u32 windowId) {
    for (u32 i = 0; i < MAX_APPS; i++) {
        if (sAppWindowIds[i] == windowId && sApps[i] != nullptr) {
            delete sApps[i];
            sApps[i] = nullptr;
            sAppWindowIds[i] = 0;
            sAppCount--;
            break;
        }
    }
}

App* AppManager::getApp(u32 windowId) {
    for (u32 i = 0; i < MAX_APPS; i++) {
        if (sAppWindowIds[i] == windowId && sApps[i] != nullptr) {
            return sApps[i];
        }
    }
    return nullptr;
}

void AppManager::renderAll() {
    for (u32 i = 0; i < MAX_APPS; i++) {
        if (sApps[i] != nullptr && sApps[i]->isActive()) {
            sApps[i]->render();
        }
    }
}

void AppManager::renderWindow(u32 windowId) {
    App* app = getApp(windowId);
    if (app && app->isActive()) {
        app->render();
    }
}

void AppManager::handleKeyPress(u32 windowId, input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) {
    App* app = getApp(windowId);
    if (app) {
        app->handleKeyPress(code, ascii, ctrl, alt, shift);
    }
}

void AppManager::handleMouseClick(u32 windowId, i32 x, i32 y, bool doubleClick) {
    App* app = getApp(windowId);
    if (app) {
        app->handleMouseClick(x, y, doubleClick);
    }
}

void AppManager::openFileInTextEditor(const char* path) {
    if (!sInitialized || sAppCount >= MAX_APPS) return;

    static u32 nextWindowX = 150;
    static u32 nextWindowY = 80;

    wm::WindowFlags flags = wm::WindowFlags::Visible |
                           wm::WindowFlags::Movable |
                           wm::WindowFlags::Resizable |
                           wm::WindowFlags::HasTitlebar |
                           wm::WindowFlags::HasBorder;

    u32 winId = wm::WindowManager::createWindow("Text Editor",
        static_cast<i32>(nextWindowX),
        static_cast<i32>(nextWindowY),
        650, 500, flags);

    if (winId != 0) {
        wm::WindowManager::setWindowBackgroundColor(winId, graphics::Color(255, 255, 255));
        wm::WindowManager::focusWindow(winId);

        TextEditorApp* editor = new TextEditorApp();
        editor->setWindowId(winId);
        editor->setActive(true);
        editor->loadFile(path);

        for (u32 i = 0; i < MAX_APPS; i++) {
            if (sApps[i] == nullptr) {
                sApps[i] = editor;
                sAppWindowIds[i] = winId;
                sAppCount++;
                break;
            }
        }

        nextWindowX += 30;
        nextWindowY += 30;
        if (nextWindowX > 400) nextWindowX = 150;
        if (nextWindowY > 300) nextWindowY = 80;
    }
}

}
