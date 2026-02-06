#pragma once

#include "../types.hpp"
#include "../disk/ata.hpp"

namespace sertos::fs {

constexpr usize MAX_PATH = 256;
constexpr usize MAX_FILENAME = 128;

constexpr u32 SERTFS_MAGIC = 0x53455254;
constexpr u32 SERTFS_VERSION = 1;
constexpr u32 SERTFS_BLOCK_SIZE = 4096;
constexpr u32 SERTFS_INODE_SIZE = 256;
constexpr u32 SERTFS_ROOT_INODE = 1;
constexpr u32 SERTFS_INVALID_BLOCK = 0xFFFFFFFF;
constexpr u32 SERTFS_DIRECT_BLOCKS = 12;
constexpr u32 SERTFS_NAME_MAX = 255;

constexpr u32 SERTFS_O_READ = 0x01;
constexpr u32 SERTFS_O_WRITE = 0x02;
constexpr u32 SERTFS_O_APPEND = 0x04;
constexpr u32 SERTFS_O_CREATE = 0x08;
constexpr u32 SERTFS_O_TRUNCATE = 0x10;

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

struct SertFsSuperblock {
    u32 magic;
    u32 version;
    u32 blockSize;
    u32 totalBlocks;
    u32 freeBlocks;
    u32 totalInodes;
    u32 freeInodes;
    u32 inodeBitmapBlock;
    u32 blockBitmapBlock;
    u32 inodeTableBlock;
    u32 dataBlockStart;
    u32 inodeBitmapBlocks;
    u32 blockBitmapBlocks;
    u32 inodeTableBlocks;
    u64 createdTime;
    u64 lastMountTime;
    u32 mountCount;
    u32 maxMountCount;
    u8 volumeLabel[32];
    u8 reserved[420];
};

enum class SertFsInodeType : u16 {
    Free = 0,
    Regular = 1,
    Directory = 2,
    Symlink = 3
};

struct SertFsInode {
    u16 type;
    u16 permissions;
    u32 uid;
    u32 gid;
    u64 size;
    u64 createdTime;
    u64 modifiedTime;
    u64 accessedTime;
    u32 linkCount;
    u32 flags;
    u32 directBlocks[SERTFS_DIRECT_BLOCKS];
    u32 indirectBlock;
    u32 doubleIndirectBlock;
    u32 tripleIndirectBlock;
    u8 reserved[128];
};

struct SertFsDirEntry {
    u32 inode;
    u16 recordLength;
    u8 nameLength;
    u8 fileType;
    char name[SERTFS_NAME_MAX + 1];
};

struct FileHandle {
    void* fsData;
    u32 flags;
    bool valid;
};

struct DirHandle {
    void* fsData;
    bool valid;
};

struct SertFsFileHandle {
    u32 inode;
    u64 position;
    u32 flags;
    bool valid;
    SertFsInode inodeData;
};

struct SertFsDirHandle {
    u32 inode;
    u64 position;
    bool valid;
    SertFsInode inodeData;
};

class SertFs {
public:
    static bool initialize(u8 driveIndex);
    static bool format(u8 driveIndex, const char* volumeLabel);
    static bool mount(const char* mountPoint);
    static bool unmount();
    static bool isMounted();
    static const char* mountPoint();
    
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
    
    static u64 freeSpace();
    static u64 totalSpace();
    
    static const char* currentDirectory();
    static bool changeDirectory(const char* path);

private:
    static bool readSuperblock();
    static bool writeSuperblock();
    static bool readInode(u32 inodeNum, SertFsInode* inode);
    static bool writeInode(u32 inodeNum, const SertFsInode* inode);
    static bool readBlock(u32 blockNum, void* buffer);
    static bool writeBlock(u32 blockNum, const void* buffer);
    
    static u32 allocateInode();
    static void freeInode(u32 inodeNum);
    static u32 allocateBlock();
    static void freeBlock(u32 blockNum);
    
    static u32 findInode(const char* path);
    static u32 findInodeInDir(u32 dirInode, const char* name);
    static bool addDirEntry(u32 dirInode, u32 entryInode, const char* name, u8 type);
    static bool removeDirEntry(u32 dirInode, const char* name);
    
    static u32 getBlockForOffset(SertFsInode* inode, u64 offset);
    static u32 allocateBlockForOffset(SertFsInode* inode, u64 offset);
    static void freeInodeBlocks(SertFsInode* inode);
    
    static const char* skipMountPoint(const char* path);
    static void splitPath(const char* path, char* parent, char* name);
    static void absolutePath(const char* path, char* result);
    
    static u8 sDriveIndex;
    static bool sMounted;
    static char sMountPoint[MAX_PATH];
    static char sCurrentDirectory[MAX_PATH];
    static SertFsSuperblock sSuperblock;
    static u8 sBlockBuffer[SERTFS_BLOCK_SIZE];
    
    static constexpr usize MAX_FILE_HANDLES = 32;
    static constexpr usize MAX_DIR_HANDLES = 16;
    static SertFsFileHandle sFileHandles[MAX_FILE_HANDLES];
    static SertFsDirHandle sDirHandles[MAX_DIR_HANDLES];
};

}
