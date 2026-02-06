#pragma once

#include "vfs.hpp"

namespace sertos::fs {

constexpr u32 DEVFS_MAX_DEVICES = 128;

enum class DeviceType : u8 {
    Char = 0,
    Block
};

struct DeviceNode {
    char name[64];
    DeviceType type;
    u32 major;
    u32 minor;
    u32 mode;
    
    i64 (*read)(DeviceNode* dev, void* buffer, usize size, u64 offset);
    i64 (*write)(DeviceNode* dev, const void* buffer, usize size, u64 offset);
    i64 (*ioctl)(DeviceNode* dev, u64 cmd, u64 arg);
    i32 (*open)(DeviceNode* dev);
    i32 (*close)(DeviceNode* dev);
    
    void* privateData;
    DeviceNode* next;
    bool valid;
};

class DevFS {
public:
    static void initialize();
    
    static SuperBlock* mount();
    static void umount(SuperBlock* sb);
    
    static DeviceNode* registerDevice(const char* name, DeviceType type, u32 major, u32 minor, u32 mode);
    static void unregisterDevice(const char* name);
    static DeviceNode* findDevice(const char* name);
    static DeviceNode* findDeviceByNumber(u32 major, u32 minor);
    
    static bool isInitialized();

private:
    static void createStandardDevices();
    
    static i64 nullRead(DeviceNode* dev, void* buffer, usize size, u64 offset);
    static i64 nullWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset);
    static i64 zeroRead(DeviceNode* dev, void* buffer, usize size, u64 offset);
    static i64 zeroWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset);
    static i64 fullRead(DeviceNode* dev, void* buffer, usize size, u64 offset);
    static i64 fullWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset);
    static i64 randomRead(DeviceNode* dev, void* buffer, usize size, u64 offset);
    static i64 urandomRead(DeviceNode* dev, void* buffer, usize size, u64 offset);
    static i64 ttyRead(DeviceNode* dev, void* buffer, usize size, u64 offset);
    static i64 ttyWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset);
    static i64 ttyIoctl(DeviceNode* dev, u64 cmd, u64 arg);
    static i64 consoleRead(DeviceNode* dev, void* buffer, usize size, u64 offset);
    static i64 consoleWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset);
    static i64 fbRead(DeviceNode* dev, void* buffer, usize size, u64 offset);
    static i64 fbWrite(DeviceNode* dev, const void* buffer, usize size, u64 offset);
    static i64 fbIoctl(DeviceNode* dev, u64 cmd, u64 arg);
    
    static Inode* devLookup(Inode* dir, const char* name);
    static i32 devGetattr(Inode* inode, Stat* stat);
    static i64 devRead(File* file, void* buffer, usize size, u64 offset);
    static i64 devWrite(File* file, const void* buffer, usize size, u64 offset);
    static i64 devIoctl(File* file, u64 cmd, u64 arg);
    static i32 devReaddir(File* file, Dirent* dirp, u32 count);
    
    static DeviceNode sDevices[DEVFS_MAX_DEVICES];
    static SuperBlock sSuperBlock;
    static u32 sDeviceCount;
    static bool sInitialized;
    
    static const InodeOperations sDevInodeOps;
    static const FileOperations sDevFileOps;
    static const FileOperations sDevDirOps;
    static const SuperBlockOperations sDevSbOps;
};

constexpr u64 TIOCGWINSZ = 0x5413;
constexpr u64 TIOCSWINSZ = 0x5414;
constexpr u64 TIOCGPGRP = 0x540F;
constexpr u64 TIOCSPGRP = 0x5410;
constexpr u64 TCGETS = 0x5401;
constexpr u64 TCSETS = 0x5402;
constexpr u64 TCSETSW = 0x5403;
constexpr u64 TCSETSF = 0x5404;

constexpr u64 FBIOGET_VSCREENINFO = 0x4600;
constexpr u64 FBIOPUT_VSCREENINFO = 0x4601;
constexpr u64 FBIOGET_FSCREENINFO = 0x4602;
constexpr u64 FBIOPAN_DISPLAY = 0x4606;

struct Winsize {
    u16 rows;
    u16 cols;
    u16 xpixel;
    u16 ypixel;
};

struct FbVarScreeninfo {
    u32 xres;
    u32 yres;
    u32 xresVirtual;
    u32 yresVirtual;
    u32 xoffset;
    u32 yoffset;
    u32 bitsPerPixel;
    u32 grayscale;
    u32 redOffset;
    u32 redLength;
    u32 greenOffset;
    u32 greenLength;
    u32 blueOffset;
    u32 blueLength;
    u32 transpOffset;
    u32 transpLength;
};

struct FbFixScreeninfo {
    char id[16];
    u64 smemStart;
    u32 smemLen;
    u32 type;
    u32 visual;
    u32 lineLength;
};

}
