#pragma once

#include "../types.hpp"

namespace sertos::fs {

constexpr usize MAX_PATH = 256;
constexpr usize MAX_FILENAME = 128;

enum class FileType : u8 {
    Unknown = 0,
    Regular = 1,
    Directory = 2,
    CharDevice = 3,
    BlockDevice = 4,
    Pipe = 5,
    Socket = 6,
    Symlink = 7
};

enum class SeekMode : u8 {
    Set = 0,
    Current = 1,
    End = 2
};

struct FileInfo {
    char name[MAX_FILENAME];
    FileType type;
    u64 size;
    u64 createdTime;
    u64 modifiedTime;
    u64 accessedTime;
    u32 permissions;
    u32 uid;
    u32 gid;
};

struct DirEntry {
    char name[MAX_FILENAME];
    FileType type;
    u64 inode;
};

constexpr u32 O_READ = 0x01;
constexpr u32 O_WRITE = 0x02;
constexpr u32 O_APPEND = 0x04;
constexpr u32 O_CREATE = 0x08;
constexpr u32 O_TRUNCATE = 0x10;

struct FileHandle {
    void* fsData;
    u32 flags;
    bool valid;
};

struct DirHandle {
    void* fsData;
    bool valid;
};

class Path {
public:
    static void normalize(const char* path, char* result);
    static void join(const char* base, const char* relative, char* result);
    static void dirname(const char* path, char* result);
    static void basename(const char* path, char* result);
    static bool isAbsolute(const char* path);
    static usize length(const char* str);
    static void copy(char* dest, const char* src, usize maxLen);
    static int compare(const char* a, const char* b);
};

class VFS {
public:
    static void initialize();
    
    static bool mount(const char* fsType, const char* mountPoint);
    static bool unmount(const char* mountPoint);
    
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
    
    static const char* currentDirectory();
    static bool changeDirectory(const char* path);
    static void absolutePath(const char* path, char* result);

private:
    static void resolvePath(const char* path, char* resolved);
    
    static char sCurrentDirectory[MAX_PATH];
    static bool sInitialized;
    static bool sRootMounted;
};

}
