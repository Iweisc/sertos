#include "../../include/apps/file_manager.hpp"
#include "../../include/apps/app.hpp"
#include "../../include/graphics/framebuffer.hpp"
#include "../../include/graphics/font.hpp"
#include "../../include/wm/wm.hpp"

namespace sertos::apps {

namespace {

constexpr i32 LIST_PADDING_X = 12;
constexpr i32 LIST_PADDING_Y = 10;
constexpr i32 ROW_HEIGHT = 18;
constexpr i32 HEADER_HEIGHT = 24;
constexpr i32 STATUS_HEIGHT = 20;

void copyString(char* dest, const char* src, usize maxLen) {
    if (!dest || !src || maxLen == 0) return;
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void joinPath(const char* base, const char* name, char* out, usize maxLen) {
    if (!base || !name || !out || maxLen == 0) return;
    usize i = 0;
    while (base[i] && i < maxLen - 1) {
        out[i] = base[i];
        i++;
    }
    if (i == 0) {
        out[0] = '/';
        i = 1;
    }
    if (i > 1 && out[i - 1] != '/' && i < maxLen - 1) {
        out[i++] = '/';
    }
    if (name[0] == '/' && i < maxLen - 1) {
        name++;
    }
    usize j = 0;
    while (name[j] && i < maxLen - 1) {
        out[i++] = name[j++];
    }
    out[i] = '\0';
}

bool isDotEntry(const char* name) {
    return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

void formatSize(u64 size, char* out, usize maxLen) {
    if (!out || maxLen == 0) return;
    const char* suffix = "B";
    u64 value = size;
    if (size >= 1024ULL * 1024ULL) {
        suffix = "M";
        value = size / (1024ULL * 1024ULL);
    } else if (size >= 1024ULL) {
        suffix = "K";
        value = size / 1024ULL;
    }
    char buffer[32];
    usize idx = 0;
    if (value == 0) {
        buffer[idx++] = '0';
    } else {
        char temp[32];
        usize tempIdx = 0;
        while (value > 0 && tempIdx < sizeof(temp)) {
            temp[tempIdx++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
        while (tempIdx > 0 && idx < sizeof(buffer) - 1) {
            buffer[idx++] = temp[--tempIdx];
        }
    }
    if (idx < sizeof(buffer) - 2) {
        buffer[idx++] = suffix[0];
        buffer[idx] = '\0';
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
    }
    copyString(out, buffer, maxLen);
}

}

FileManagerApp::FileManagerApp()
    : mEntryCount(0), mSelectedIndex(-1), mScrollOffset(0) {
    copyString(mCurrentPath, fs::SertFs::currentDirectory(), fs::MAX_PATH);
    mStatus[0] = '\0';
    refresh();
}

void FileManagerApp::setStatus(const char* text) {
    copyString(mStatus, text ? text : "", sizeof(mStatus));
}

void FileManagerApp::setPath(const char* path) {
    if (!path) return;
    copyString(mCurrentPath, path, fs::MAX_PATH);
}

void FileManagerApp::refresh() {
    mEntryCount = 0;
    mSelectedIndex = -1;
    mScrollOffset = 0;

    fs::DirHandle dir = fs::SertFs::openDir(mCurrentPath);
    if (!dir.valid) {
        setStatus("Unable to open directory");
        return;
    }

    fs::DirEntry entry;
    while (fs::SertFs::readDir(&dir, &entry)) {
        if (isDotEntry(entry.name)) {
            continue;
        }
        if (mEntryCount >= MAX_ENTRIES) {
            break;
        }
        Entry& dst = mEntries[mEntryCount++];
        copyString(dst.name, entry.name, fs::MAX_FILENAME);
        dst.type = entry.type;
        dst.size = 0;

        if (entry.type == fs::FileType::Regular || entry.type == fs::FileType::Directory) {
            char fullPath[fs::MAX_PATH];
            joinPath(mCurrentPath, entry.name, fullPath, fs::MAX_PATH);
            fs::FileInfo info;
            if (fs::SertFs::getInfo(fullPath, &info)) {
                dst.size = info.size;
            }
        }
    }

    fs::SertFs::closeDir(&dir);

    if (mEntryCount > 0) {
        mSelectedIndex = 0;
    }

    setStatus("Ready");
}

void FileManagerApp::selectIndex(i32 index) {
    if (mEntryCount == 0) {
        mSelectedIndex = -1;
        return;
    }
    if (index < 0) index = 0;
    if (index >= static_cast<i32>(mEntryCount)) index = static_cast<i32>(mEntryCount) - 1;
    mSelectedIndex = index;
    ensureSelectionVisible();
}

void FileManagerApp::ensureSelectionVisible() {
    Window* win = wm::WindowManager::getWindow(mWindowId);
    if (!win) return;
    wm::Rect client = win->clientArea();
    i32 listTop = client.y + LIST_PADDING_Y + HEADER_HEIGHT;
    i32 listBottom = client.y + static_cast<i32>(client.height) - STATUS_HEIGHT - LIST_PADDING_Y;
    i32 visibleRows = (listBottom - listTop) / ROW_HEIGHT;
    if (visibleRows < 1) visibleRows = 1;

    if (mSelectedIndex < mScrollOffset) {
        mScrollOffset = mSelectedIndex;
    } else if (mSelectedIndex >= mScrollOffset + visibleRows) {
        mScrollOffset = mSelectedIndex - visibleRows + 1;
    }
    if (mScrollOffset < 0) mScrollOffset = 0;
}

void FileManagerApp::openSelected() {
    if (mSelectedIndex < 0 || mSelectedIndex >= static_cast<i32>(mEntryCount)) return;
    Entry& entry = mEntries[mSelectedIndex];

    char fullPath[fs::MAX_PATH];
    joinPath(mCurrentPath, entry.name, fullPath, fs::MAX_PATH);

    if (entry.type == fs::FileType::Directory) {
        if (!fs::SertFs::changeDirectory(fullPath)) {
            setStatus("Failed to enter directory");
            return;
        }
        setPath(fs::SertFs::currentDirectory());
        refresh();
    } else if (entry.type == fs::FileType::Regular) {
        AppManager::openFileInTextEditor(fullPath);
        setStatus("Opened in Text Editor");
    } else {
        setStatus("Cannot open this file type");
    }
}

void FileManagerApp::render() {
    Window* win = wm::WindowManager::getWindow(mWindowId);
    if (!win) return;

    wm::Rect client = win->clientArea();

    graphics::Color headerBg(230, 235, 245);
    graphics::Color headerText(40, 60, 90);
    graphics::Color textColor(40, 40, 40);
    graphics::Color mutedColor(120, 120, 120);
    graphics::Color rowHighlight(200, 220, 245);
    graphics::Color divider(200, 200, 210);

    i32 headerX = client.x + LIST_PADDING_X;
    i32 headerY = client.y + LIST_PADDING_Y;

    graphics::Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(client.y),
                                    client.width, HEADER_HEIGHT, headerBg);

    wm::WindowManager::drawText(headerX, headerY + 4, mCurrentPath, headerText);

    i32 listTop = client.y + LIST_PADDING_Y + HEADER_HEIGHT;
    i32 listBottom = client.y + static_cast<i32>(client.height) - STATUS_HEIGHT - LIST_PADDING_Y;
    if (listBottom < listTop) return;

    graphics::Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(listTop),
                                    client.width, static_cast<u32>(listBottom - listTop),
                                    graphics::Color(250, 250, 250));

    i32 columnNameX = client.x + LIST_PADDING_X;
    i32 columnTypeX = client.x + static_cast<i32>(client.width) - 150;
    i32 columnSizeX = client.x + static_cast<i32>(client.width) - 70;

    wm::WindowManager::drawText(columnNameX, listTop, "Name", mutedColor);
    wm::WindowManager::drawText(columnTypeX, listTop, "Type", mutedColor);
    wm::WindowManager::drawText(columnSizeX, listTop, "Size", mutedColor);

    graphics::Framebuffer::drawLine(static_cast<u32>(client.x), static_cast<u32>(listTop + 14),
                                    static_cast<u32>(client.x + static_cast<i32>(client.width)),
                                    static_cast<u32>(listTop + 14), divider);

    i32 listY = listTop + 18;
    i32 rowsAvailable = (listBottom - listY) / ROW_HEIGHT;
    if (rowsAvailable < 0) rowsAvailable = 0;

    for (i32 i = 0; i < rowsAvailable; i++) {
        i32 entryIndex = mScrollOffset + i;
        if (entryIndex >= static_cast<i32>(mEntryCount)) break;

        i32 rowY = listY + i * ROW_HEIGHT;
        if (entryIndex == mSelectedIndex) {
            graphics::Framebuffer::fillRect(static_cast<u32>(client.x + 4), static_cast<u32>(rowY - 2),
                                            client.width - 8, static_cast<u32>(ROW_HEIGHT), rowHighlight);
        }

        Entry& entry = mEntries[entryIndex];
        graphics::Color entryColor = entry.type == fs::FileType::Directory ? graphics::Color(40, 90, 150) : textColor;

        wm::WindowManager::drawText(columnNameX, rowY, entry.name, entryColor);

        if (entry.type == fs::FileType::Directory) {
            wm::WindowManager::drawText(columnTypeX, rowY, "DIR", mutedColor);
        } else {
            wm::WindowManager::drawText(columnTypeX, rowY, "FILE", mutedColor);
        }

        char sizeText[16];
        formatSize(entry.size, sizeText, sizeof(sizeText));
        wm::WindowManager::drawText(columnSizeX, rowY, sizeText, mutedColor);
    }

    i32 statusY = client.y + static_cast<i32>(client.height) - STATUS_HEIGHT;
    graphics::Framebuffer::fillRect(static_cast<u32>(client.x), static_cast<u32>(statusY),
                                    client.width, STATUS_HEIGHT, headerBg);

    wm::WindowManager::drawText(client.x + LIST_PADDING_X, statusY + 2, mStatus, headerText);
}

void FileManagerApp::handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) {
    (void)ascii;
    (void)alt;

    if (ctrl) {
        if (code == input::KeyCode::R) {
            refresh();
        }
        return;
    }

    switch (code) {
        case input::KeyCode::Up:
            selectIndex(mSelectedIndex - 1);
            break;
        case input::KeyCode::Down:
            selectIndex(mSelectedIndex + 1);
            break;
        case input::KeyCode::PageUp:
            selectIndex(mSelectedIndex - 10);
            break;
        case input::KeyCode::PageDown:
            selectIndex(mSelectedIndex + 10);
            break;
        case input::KeyCode::Enter:
            openSelected();
            break;
        case input::KeyCode::Backspace:
            if (fs::SertFs::changeDirectory("..")) {
                setPath(fs::SertFs::currentDirectory());
                refresh();
            }
            break;
        case input::KeyCode::Home:
            selectIndex(0);
            break;
        case input::KeyCode::End:
            selectIndex(static_cast<i32>(mEntryCount) - 1);
            break;
        default:
            if (shift && code == input::KeyCode::R) {
                refresh();
            }
            break;
    }
}

void FileManagerApp::handleMouseClick(i32 x, i32 y, bool doubleClick) {
    Window* win = wm::WindowManager::getWindow(mWindowId);
    if (!win) return;

    wm::Rect client = win->clientArea();
    if (!client.contains(x, y)) return;

    i32 listTop = client.y + LIST_PADDING_Y + HEADER_HEIGHT + 18;
    i32 listBottom = client.y + static_cast<i32>(client.height) - STATUS_HEIGHT - LIST_PADDING_Y;
    if (y < listTop || y > listBottom) return;

    i32 row = (y - listTop) / ROW_HEIGHT;
    i32 entryIndex = mScrollOffset + row;
    if (entryIndex >= 0 && entryIndex < static_cast<i32>(mEntryCount)) {
        selectIndex(entryIndex);
        if (doubleClick) {
            openSelected();
        }
    }
}

}
