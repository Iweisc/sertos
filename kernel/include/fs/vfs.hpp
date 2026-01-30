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

class FileSystem;
class File;
class Directory;

class FileNode {
public:
    virtual ~FileNode() = default;
    
    virtual i64 read(void* buffer, usize size) = 0;
    virtual i64 write(const void* buffer, usize size) = 0;
    virtual i64 seek(i64 offset, SeekMode mode) = 0;
    virtual i64 tell() = 0;
    virtual bool close() = 0;
    virtual bool getInfo(FileInfo* info) = 0;
    
    FileType type() const { return mType; }
    const char* name() const { return mName; }

    char mName[MAX_FILENAME];
    FileType mType;
    FileSystem* mFs;
};

class DirectoryNode {
public:
    virtual ~DirectoryNode() = default;
    
    virtual bool readEntry(DirEntry* entry) = 0;
    virtual bool rewind() = 0;
    virtual bool close() = 0;
    virtual usize count() = 0;
    
    const char* path() const { return mPath; }

protected:
    char mPath[MAX_PATH];
    FileSystem* mFs;
};

class FileSystem {
public:
    virtual ~FileSystem() = default;
    
    virtual bool mount(const char* mountPoint) = 0;
    virtual bool unmount() = 0;
    
    virtual FileNode* open(const char* path, u32 flags) = 0;
    virtual DirectoryNode* openDir(const char* path) = 0;
    
    virtual bool exists(const char* path) = 0;
    virtual bool isFile(const char* path) = 0;
    virtual bool isDirectory(const char* path) = 0;
    
    virtual bool createFile(const char* path) = 0;
    virtual bool createDirectory(const char* path) = 0;
    virtual bool remove(const char* path) = 0;
    virtual bool rename(const char* oldPath, const char* newPath) = 0;
    
    virtual bool getInfo(const char* path, FileInfo* info) = 0;
    
    const char* name() const { return mName; }
    const char* mountPoint() const { return mMountPoint; }
    bool isMounted() const { return mMounted; }

protected:
    char mName[32];
    char mMountPoint[MAX_PATH];
    bool mMounted;
};

constexpr u32 O_READ = 0x01;
constexpr u32 O_WRITE = 0x02;
constexpr u32 O_APPEND = 0x04;
constexpr u32 O_CREATE = 0x08;
constexpr u32 O_TRUNCATE = 0x10;

class VFS {
public:
    static void initialize();
    
    static bool mount(FileSystem* fs, const char* mountPoint);
    static bool unmount(const char* mountPoint);
    
    static FileNode* open(const char* path, u32 flags);
    static DirectoryNode* openDir(const char* path);
    
    static bool exists(const char* path);
    static bool isFile(const char* path);
    static bool isDirectory(const char* path);
    
    static bool createFile(const char* path);
    static bool createDirectory(const char* path);
    static bool remove(const char* path);
    static bool rename(const char* oldPath, const char* newPath);
    
    static bool getInfo(const char* path, FileInfo* info);

private:
    static FileSystem* findFileSystem(const char* path);
    static void resolvePath(const char* path, char* resolved);
    
    static constexpr usize MAX_FILESYSTEMS = 16;
    static FileSystem* sFileSystems[MAX_FILESYSTEMS];
    static usize sFileSystemCount;
    static bool sInitialized;
};

}
