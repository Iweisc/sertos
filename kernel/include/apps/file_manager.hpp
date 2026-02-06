#pragma once

#include "app.hpp"
#include "../fs/sertfs.hpp"
#include "../input/keyboard.hpp"

namespace sertos::apps {

class FileManagerApp : public App {
public:
    FileManagerApp();
    ~FileManagerApp() override = default;

    void render() override;
    void handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) override;
    void handleMouseClick(i32 x, i32 y, bool doubleClick) override;

private:
    struct Entry {
        char name[fs::MAX_FILENAME];
        fs::FileType type;
        u64 size;
    };

    static constexpr u32 MAX_ENTRIES = 256;

    Entry mEntries[MAX_ENTRIES];
    u32 mEntryCount;
    i32 mSelectedIndex;
    i32 mScrollOffset;
    char mCurrentPath[fs::MAX_PATH];
    char mStatus[128];

    void refresh();
    void openSelected();
    void selectIndex(i32 index);
    void ensureSelectionVisible();
    void setStatus(const char* text);
    void setPath(const char* path);
};

}
