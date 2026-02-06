#include "../../include/fs/devfs.hpp"
#include "../../include/graphics/console.hpp"
#include "../../include/graphics/framebuffer.hpp"

namespace sertos::fs {

DeviceNode DevFS::sDevices[DEVFS_MAX_DEVICES];
SuperBlock DevFS::sSuperBlock;
u32 DevFS::sDeviceCount = 0;
bool DevFS::sInitialized = false;

const InodeOperations DevFS::sDevInodeOps = {
    devLookup,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    devGetattr,
    nullptr
};

const FileOperations DevFS::sDevFileOps = {
    devRead,
    devWrite,
    devIoctl,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

const FileOperations DevFS::sDevDirOps = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    devReaddir,
    nullptr
};

const SuperBlockOperations DevFS::sDevSbOps = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

void DevFS::initialize() {
    for (u32 i = 0; i < DEVFS_MAX_DEVICES; i++) {
        sDevices[i].valid = false;
    }
    
    sDeviceCount = 0;
    
    sSuperBlock.type = FileSystemType::DevFS;
    sSuperBlock.blockSize = 4096;
    sSuperBlock.totalBlocks = 0;
    sSuperBlock.freeBlocks = 0;
    sSuperBlock.totalInodes = DEVFS_MAX_DEVICES;
    sSuperBlock.freeInodes = DEVFS_MAX_DEVICES;
    sSuperBlock.root = nullptr;
    sSuperBlock.sops = &sDevSbOps;
    sSuperBlock.privateData = nullptr;
    sSuperBlock.valid = true;
    
    createStandardDevices();
    
    sInitialized = true;
}

SuperBlock* DevFS::mount() {
    return &sSuperBlock;
}

void DevFS::umount(SuperBlock* sb) {
    (void)sb;
}

DeviceNode* DevFS::registerDevice(const char* name, DeviceType type, u32 major, u32 minor, u32 mode) {
    for (u32 i = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (!sDevices[i].valid) {
            sDevices[i].valid = true;
            sDevices[i].type = type;
            sDevices[i].major = major;
            sDevices[i].minor = minor;
            sDevices[i].mode = mode;
            sDevices[i].read = nullptr;
            sDevices[i].write = nullptr;
            sDevices[i].ioctl = nullptr;
            sDevices[i].open = nullptr;
            sDevices[i].close = nullptr;
            sDevices[i].privateData = nullptr;
            sDevices[i].next = nullptr;
            
            usize j = 0;
            while (name[j] && j < 63) {
                sDevices[i].name[j] = name[j];
                j++;
            }
            sDevices[i].name[j] = '\0';
            
            sDeviceCount++;
            return &sDevices[i];
        }
    }
    
    return nullptr;
}

void DevFS::unregisterDevice(const char* name) {
    DeviceNode* dev = findDevice(name);
    if (dev) {
        dev->valid = false;
        sDeviceCount--;
    }
}

DeviceNode* DevFS::findDevice(const char* name) {
    for (u32 i = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (sDevices[i].valid) {
            bool match = true;
            for (usize j = 0; name[j] || sDevices[i].name[j]; j++) {
                if (name[j] != sDevices[i].name[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return &sDevices[i];
        }
    }
    
    return nullptr;
}

DeviceNode* DevFS::findDeviceByNumber(u32 major, u32 minor) {
    for (u32 i = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (sDevices[i].valid && sDevices[i].major == major && sDevices[i].minor == minor) {
            return &sDevices[i];
        }
    }
    
    return nullptr;
}

bool DevFS::isInitialized() {
    return sInitialized;
}

void DevFS::createStandardDevices() {
    DeviceNode* null = registerDevice("null", DeviceType::Char, 1, 3, 0666);
    if (null) {
        null->read = nullRead;
        null->write = nullWrite;
    }
    
    DeviceNode* zero = registerDevice("zero", DeviceType::Char, 1, 5, 0666);
    if (zero) {
        zero->read = zeroRead;
        zero->write = zeroWrite;
    }
    
    DeviceNode* full = registerDevice("full", DeviceType::Char, 1, 7, 0666);
    if (full) {
        full->read = fullRead;
        full->write = fullWrite;
    }
    
    DeviceNode* random = registerDevice("random", DeviceType::Char, 1, 8, 0666);
    if (random) {
        random->read = randomRead;
        random->write = nullWrite;
    }
    
    DeviceNode* urandom = registerDevice("urandom", DeviceType::Char, 1, 9, 0666);
    if (urandom) {
        urandom->read = urandomRead;
        urandom->write = nullWrite;
    }
    
    DeviceNode* tty = registerDevice("tty", DeviceType::Char, 5, 0, 0666);
    if (tty) {
        tty->read = ttyRead;
        tty->write = ttyWrite;
        tty->ioctl = ttyIoctl;
    }
    
    DeviceNode* console = registerDevice("console", DeviceType::Char, 5, 1, 0620);
    if (console) {
        console->read = consoleRead;
        console->write = consoleWrite;
        console->ioctl = ttyIoctl;
    }
    
    DeviceNode* tty0 = registerDevice("tty0", DeviceType::Char, 4, 0, 0620);
    if (tty0) {
        tty0->read = ttyRead;
        tty0->write = ttyWrite;
        tty0->ioctl = ttyIoctl;
    }
    
    DeviceNode* fb0 = registerDevice("fb0", DeviceType::Char, 29, 0, 0660);
    if (fb0) {
        fb0->read = fbRead;
        fb0->write = fbWrite;
        fb0->ioctl = fbIoctl;
    }
    
    registerDevice("stdin", DeviceType::Char, 0, 0, 0666);
    registerDevice("stdout", DeviceType::Char, 0, 1, 0666);
    registerDevice("stderr", DeviceType::Char, 0, 2, 0666);
    
    registerDevice("ptmx", DeviceType::Char, 5, 2, 0666);
    
    registerDevice("mem", DeviceType::Char, 1, 1, 0640);
    registerDevice("kmem", DeviceType::Char, 1, 2, 0640);
    registerDevice("port", DeviceType::Char, 1, 4, 0640);
}

i64 DevFS::nullRead(DeviceNode* dev, void* buffer, usize size, u64 offset) {
    (void)dev;
    (void)buffer;
    (void)size;
    (void)offset;
    return 0;
}

i64 DevFS::nullWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset) {
    (void)dev;
    (void)buffer;
    (void)offset;
    return static_cast<i64>(size);
}

i64 DevFS::zeroRead(DeviceNode* dev, void* buffer, usize size, u64 offset) {
    (void)dev;
    (void)offset;
    
    u8* buf = static_cast<u8*>(buffer);
    for (usize i = 0; i < size; i++) {
        buf[i] = 0;
    }
    
    return static_cast<i64>(size);
}

i64 DevFS::zeroWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset) {
    (void)dev;
    (void)buffer;
    (void)offset;
    return static_cast<i64>(size);
}

i64 DevFS::fullRead(DeviceNode* dev, void* buffer, usize size, u64 offset) {
    return zeroRead(dev, buffer, size, offset);
}

i64 DevFS::fullWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset) {
    (void)dev;
    (void)buffer;
    (void)size;
    (void)offset;
    return -1;
}

i64 DevFS::randomRead(DeviceNode* dev, void* buffer, usize size, u64 offset) {
    (void)dev;
    (void)offset;
    
    u8* buf = static_cast<u8*>(buffer);
    static u64 seed = 0x5DEECE66DULL;
    
    for (usize i = 0; i < size; i++) {
        seed = (seed * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
        buf[i] = static_cast<u8>(seed >> 16);
    }
    
    return static_cast<i64>(size);
}

i64 DevFS::urandomRead(DeviceNode* dev, void* buffer, usize size, u64 offset) {
    return randomRead(dev, buffer, size, offset);
}

i64 DevFS::ttyRead(DeviceNode* dev, void* buffer, usize size, u64 offset) {
    (void)dev;
    (void)buffer;
    (void)size;
    (void)offset;
    return 0;
}

i64 DevFS::ttyWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset) {
    (void)dev;
    (void)offset;
    
    const char* buf = static_cast<const char*>(buffer);
    for (usize i = 0; i < size; i++) {
        graphics::Console::putChar(buf[i]);
    }
    
    return static_cast<i64>(size);
}

i64 DevFS::ttyIoctl(DeviceNode* dev, u64 cmd, u64 arg) {
    (void)dev;
    
    switch (cmd) {
        case TIOCGWINSZ: {
            Winsize* ws = reinterpret_cast<Winsize*>(arg);
            if (ws) {
                ws->rows = 25;
                ws->cols = 80;
                ws->xpixel = 640;
                ws->ypixel = 400;
            }
            return 0;
        }
        
        case TIOCSWINSZ:
            return 0;
            
        case TCGETS:
        case TCSETS:
        case TCSETSW:
        case TCSETSF:
            return 0;
            
        case TIOCGPGRP:
        case TIOCSPGRP:
            return 0;
            
        default:
            return -1;
    }
}

i64 DevFS::consoleRead(DeviceNode* dev, void* buffer, usize size, u64 offset) {
    return ttyRead(dev, buffer, size, offset);
}

i64 DevFS::consoleWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset) {
    return ttyWrite(dev, buffer, size, offset);
}

i64 DevFS::fbRead(DeviceNode* dev, void* buffer, usize size, u64 offset) {
    (void)dev;
    
    if (!graphics::Framebuffer::isInitialized()) return -1;
    
    u8* fbBase = reinterpret_cast<u8*>(graphics::Framebuffer::address());
    usize fbSize = graphics::Framebuffer::pitch() * graphics::Framebuffer::height();
    
    if (offset >= fbSize) return 0;
    
    usize toRead = (size < fbSize - offset) ? size : fbSize - offset;
    u8* dest = static_cast<u8*>(buffer);
    
    for (usize i = 0; i < toRead; i++) {
        dest[i] = fbBase[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 DevFS::fbWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset) {
    (void)dev;
    
    if (!graphics::Framebuffer::isInitialized()) return -1;
    
    u8* fbBase = reinterpret_cast<u8*>(graphics::Framebuffer::address());
    usize fbSize = graphics::Framebuffer::pitch() * graphics::Framebuffer::height();
    
    if (offset >= fbSize) return 0;
    
    usize toWrite = (size < fbSize - offset) ? size : fbSize - offset;
    const u8* src = static_cast<const u8*>(buffer);
    
    for (usize i = 0; i < toWrite; i++) {
        fbBase[offset + i] = src[i];
    }
    
    return static_cast<i64>(toWrite);
}

i64 DevFS::fbIoctl(DeviceNode* dev, u64 cmd, u64 arg) {
    (void)dev;
    
    if (!graphics::Framebuffer::isInitialized()) return -1;
    
    switch (cmd) {
        case FBIOGET_VSCREENINFO: {
            FbVarScreeninfo* info = reinterpret_cast<FbVarScreeninfo*>(arg);
            if (info) {
                info->xres = graphics::Framebuffer::width();
                info->yres = graphics::Framebuffer::height();
                info->xresVirtual = graphics::Framebuffer::width();
                info->yresVirtual = graphics::Framebuffer::height();
                info->xoffset = 0;
                info->yoffset = 0;
                info->bitsPerPixel = graphics::Framebuffer::bpp();
                info->grayscale = 0;
                info->redOffset = 16;
                info->redLength = 8;
                info->greenOffset = 8;
                info->greenLength = 8;
                info->blueOffset = 0;
                info->blueLength = 8;
                info->transpOffset = 24;
                info->transpLength = 8;
            }
            return 0;
        }
        
        case FBIOPUT_VSCREENINFO:
            return 0;
            
        case FBIOGET_FSCREENINFO: {
            FbFixScreeninfo* info = reinterpret_cast<FbFixScreeninfo*>(arg);
            if (info) {
                const char* id = "SertOS FB";
                for (usize i = 0; i < 15 && id[i]; i++) {
                    info->id[i] = id[i];
                }
                info->id[15] = '\0';
                info->smemStart = graphics::Framebuffer::address();
                info->smemLen = static_cast<u32>(graphics::Framebuffer::pitch() * graphics::Framebuffer::height());
                info->type = 0;
                info->visual = 2;
                info->lineLength = graphics::Framebuffer::pitch();
            }
            return 0;
        }
        
        case FBIOPAN_DISPLAY:
            return 0;
            
        default:
            return -1;
    }
}

Inode* DevFS::devLookup(Inode* dir, const char* name) {
    (void)dir;
    (void)name;
    return nullptr;
}

i32 DevFS::devGetattr(Inode* inode, Stat* stat) {
    if (!inode || !stat) return -1;
    
    stat->dev = 0;
    stat->ino = inode->ino;
    stat->mode = inode->mode;
    stat->nlink = inode->nlink;
    stat->uid = inode->uid;
    stat->gid = inode->gid;
    stat->rdev = 0;
    stat->size = static_cast<i64>(inode->size);
    stat->blksize = 4096;
    stat->blocks = 0;
    stat->atime = inode->atime;
    stat->mtime = inode->mtime;
    stat->ctime = inode->ctime;
    
    return 0;
}

i64 DevFS::devRead(File* file, void* buffer, usize size, u64 offset) {
    if (!file || !file->privateData) return -1;
    
    DeviceNode* dev = static_cast<DeviceNode*>(file->privateData);
    if (!dev->read) return -1;
    
    return dev->read(dev, buffer, size, offset);
}

i64 DevFS::devWrite(File* file, const void* buffer, usize size, u64 offset) {
    if (!file || !file->privateData) return -1;
    
    DeviceNode* dev = static_cast<DeviceNode*>(file->privateData);
    if (!dev->write) return -1;
    
    return dev->write(dev, buffer, size, offset);
}

i64 DevFS::devIoctl(File* file, u64 cmd, u64 arg) {
    if (!file || !file->privateData) return -1;
    
    DeviceNode* dev = static_cast<DeviceNode*>(file->privateData);
    if (!dev->ioctl) return -1;
    
    return dev->ioctl(dev, cmd, arg);
}

i32 DevFS::devReaddir(File* file, Dirent* dirp, u32 count) {
    (void)file;
    
    if (!dirp || count == 0) return 0;
    
    u32 filled = 0;
    
    for (u32 i = 0; i < DEVFS_MAX_DEVICES && filled < count; i++) {
        if (sDevices[i].valid) {
            dirp[filled].ino = i + 1;
            dirp[filled].off = static_cast<i64>(filled + 1);
            dirp[filled].reclen = sizeof(Dirent);
            dirp[filled].type = (sDevices[i].type == DeviceType::Char) ? 2 : 6;
            
            usize j = 0;
            while (sDevices[i].name[j] && j < MAX_FILENAME_LENGTH - 1) {
                dirp[filled].name[j] = sDevices[i].name[j];
                j++;
            }
            dirp[filled].name[j] = '\0';
            
            filled++;
        }
    }
    
    return static_cast<i32>(filled);
}

}
