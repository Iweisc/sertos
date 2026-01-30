#pragma once

#include "vfs.hpp"

namespace sertos::fs {

constexpr usize RAMFS_MAX_FILES = 64;
constexpr usize RAMFS_MAX_FILE_SIZE = 64 * 1024;

struct RamFsNode {
    char name[MAX_FILENAME];
    FileType type;
    u8* data;
    usize size;
    usize capacity;
    RamFsNode* parent;
    RamFsNode* children;
    RamFsNode* next;
    u64 createdTime;
    u64 modifiedTime;
};

struct RamFsFileHandle {
    RamFsNode* node;
    usize position;
    u32 flags;
    bool valid;
};

struct RamFsDirHandle {
    RamFsNode* node;
    RamFsNode* current;
    bool valid;
};

class RamFs {
public:
    static void initialize();
    static bool mount(const char* mountPoint);
    static bool unmount();
    
    static FileHandle open(const char* path, u32 flags);
    static i64 read(FileHandle* handle, void* buffer, usize size);
    static i64 write(FileHandle* handle, const void* buffer, usize size);
    static i64 seek(FileHandle* handle, i64 offset, SeekMode mode);
    static i64 tell(FileHandle* handle);
    static bool close(FileHandle* handle);
    
    static DirHandle openDir(const char* path);
    static bool readDir(DirHandle* handle, DirEntry* entry);
    static bool closeDir(DirHandle* handle);
    
    static bool exists(const char* path);
    static bool isFile(const char* path);
    static bool isDirectory(const char* path);
    
    static bool createFile(const char* path);
    static bool createDirectory(const char* path);
    static bool remove(const char* path);
    static bool rename(const char* oldPath, const char* newPath);
    
    static bool getInfo(const char* path, FileInfo* info);
    
    static const char* mountPoint() { return sMountPoint; }
    static bool isMounted() { return sMounted; }

private:
    static RamFsNode* findNode(const char* path);
    static RamFsNode* findParent(const char* path, char* childName);
    static RamFsNode* createNode(const char* name, FileType type, RamFsNode* parent);
    static void deleteNode(RamFsNode* node);
    static const char* skipMountPoint(const char* path);
    
    static RamFsNode* sRoot;
    static usize sNodeCount;
    static char sMountPoint[MAX_PATH];
    static bool sMounted;
    
    static constexpr usize MAX_FILE_HANDLES = 32;
    static constexpr usize MAX_DIR_HANDLES = 16;
    static RamFsFileHandle sFileHandles[MAX_FILE_HANDLES];
    static RamFsDirHandle sDirHandles[MAX_DIR_HANDLES];
};

}
