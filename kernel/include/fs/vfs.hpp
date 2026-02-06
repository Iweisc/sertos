#pragma once

#include "../types.hpp"

namespace sertos::fs {

constexpr u32 MAX_PATH_LENGTH = 4096;
constexpr u32 MAX_FILENAME_LENGTH = 256;
constexpr u32 MAX_MOUNT_POINTS = 32;
constexpr u32 MAX_OPEN_FILES = 1024;
constexpr u32 MAX_INODES = 4096;

constexpr u32 S_IFMT = 0170000;
constexpr u32 S_IFSOCK = 0140000;
constexpr u32 S_IFLNK = 0120000;
constexpr u32 S_IFREG = 0100000;
constexpr u32 S_IFBLK = 0060000;
constexpr u32 S_IFDIR = 0040000;
constexpr u32 S_IFCHR = 0020000;
constexpr u32 S_IFIFO = 0010000;

constexpr u32 S_ISUID = 04000;
constexpr u32 S_ISGID = 02000;
constexpr u32 S_ISVTX = 01000;

constexpr u32 S_IRWXU = 00700;
constexpr u32 S_IRUSR = 00400;
constexpr u32 S_IWUSR = 00200;
constexpr u32 S_IXUSR = 00100;

constexpr u32 S_IRWXG = 00070;
constexpr u32 S_IRGRP = 00040;
constexpr u32 S_IWGRP = 00020;
constexpr u32 S_IXGRP = 00010;

constexpr u32 S_IRWXO = 00007;
constexpr u32 S_IROTH = 00004;
constexpr u32 S_IWOTH = 00002;
constexpr u32 S_IXOTH = 00001;

constexpr i32 O_RDONLY = 0;
constexpr i32 O_WRONLY = 1;
constexpr i32 O_RDWR = 2;
constexpr i32 O_CREAT = 0100;
constexpr i32 O_EXCL = 0200;
constexpr i32 O_NOCTTY = 0400;
constexpr i32 O_TRUNC = 01000;
constexpr i32 O_APPEND = 02000;
constexpr i32 O_NONBLOCK = 04000;
constexpr i32 O_DIRECTORY = 0200000;
constexpr i32 O_CLOEXEC = 02000000;

constexpr i32 SEEK_SET = 0;
constexpr i32 SEEK_CUR = 1;
constexpr i32 SEEK_END = 2;

constexpr i32 AT_FDCWD = -100;
constexpr i32 AT_SYMLINK_NOFOLLOW = 0x100;
constexpr i32 AT_REMOVEDIR = 0x200;
constexpr i32 AT_SYMLINK_FOLLOW = 0x400;
constexpr i32 AT_EMPTY_PATH = 0x1000;

struct Stat {
    u64 dev;
    u64 ino;
    u32 mode;
    u32 nlink;
    u32 uid;
    u32 gid;
    u64 rdev;
    i64 size;
    i64 blksize;
    i64 blocks;
    u64 atime;
    u64 mtime;
    u64 ctime;
};

struct Dirent {
    u64 ino;
    i64 off;
    u16 reclen;
    u8 type;
    char name[MAX_FILENAME_LENGTH];
};

enum class FileSystemType : u8 {
    Unknown = 0,
    SertFS,
    ProcFS,
    DevFS,
    SysFS,
    TmpFS,
    RamFS
};

struct Inode;
struct File;
struct Dentry;
struct SuperBlock;

struct FileOperations {
    i64 (*read)(File* file, void* buffer, usize size, u64 offset);
    i64 (*write)(File* file, const void* buffer, usize size, u64 offset);
    i64 (*ioctl)(File* file, u64 cmd, u64 arg);
    i32 (*open)(Inode* inode, File* file);
    i32 (*release)(Inode* inode, File* file);
    i64 (*lseek)(File* file, i64 offset, i32 whence);
    i32 (*readdir)(File* file, Dirent* dirp, u32 count);
    i32 (*mmap)(File* file, u64 addr, usize length, i32 prot, i32 flags);
};

struct InodeOperations {
    Inode* (*lookup)(Inode* dir, const char* name);
    i32 (*create)(Inode* dir, const char* name, u32 mode);
    i32 (*mkdir)(Inode* dir, const char* name, u32 mode);
    i32 (*rmdir)(Inode* dir, const char* name);
    i32 (*unlink)(Inode* dir, const char* name);
    i32 (*rename)(Inode* oldDir, const char* oldName, Inode* newDir, const char* newName);
    i32 (*link)(Inode* dir, const char* name, Inode* target);
    i32 (*symlink)(Inode* dir, const char* name, const char* target);
    i64 (*readlink)(Inode* inode, char* buffer, usize size);
    i32 (*getattr)(Inode* inode, Stat* stat);
    i32 (*setattr)(Inode* inode, const Stat* stat);
};

struct SuperBlockOperations {
    Inode* (*allocInode)(SuperBlock* sb);
    void (*freeInode)(SuperBlock* sb, Inode* inode);
    i32 (*writeInode)(SuperBlock* sb, Inode* inode);
    i32 (*sync)(SuperBlock* sb);
    i32 (*statfs)(SuperBlock* sb, void* buf);
};

struct Inode {
    u64 ino;
    u32 mode;
    u32 nlink;
    u32 uid;
    u32 gid;
    u64 size;
    u64 atime;
    u64 mtime;
    u64 ctime;
    u64 blocks;
    
    SuperBlock* sb;
    const InodeOperations* iops;
    const FileOperations* fops;
    
    void* privateData;
    bool valid;
};

struct File {
    Inode* inode;
    u64 offset;
    u32 flags;
    u32 refCount;
    const FileOperations* fops;
    void* privateData;
    bool valid;
};

struct Dentry {
    char name[MAX_FILENAME_LENGTH];
    Inode* inode;
    Dentry* parent;
    Dentry* children;
    Dentry* next;
    u32 refCount;
    bool valid;
};

struct SuperBlock {
    FileSystemType type;
    u64 blockSize;
    u64 totalBlocks;
    u64 freeBlocks;
    u64 totalInodes;
    u64 freeInodes;
    
    Inode* root;
    const SuperBlockOperations* sops;
    
    void* privateData;
    bool valid;
};

struct MountPoint {
    char path[MAX_PATH_LENGTH];
    SuperBlock* sb;
    Dentry* mountPoint;
    FileSystemType type;
    u32 flags;
    bool valid;
};

class VFS {
public:
    static void initialize();
    
    static i32 mount(const char* source, const char* target, FileSystemType type, u32 flags);
    static i32 umount(const char* target);
    
    static i32 open(const char* path, i32 flags, u32 mode);
    static i32 openat(i32 dirfd, const char* path, i32 flags, u32 mode);
    static i32 close(i32 fd);
    static i64 read(i32 fd, void* buffer, usize size);
    static i64 write(i32 fd, const void* buffer, usize size);
    static i64 lseek(i32 fd, i64 offset, i32 whence);
    static i32 fstat(i32 fd, Stat* stat);
    static i32 stat(const char* path, Stat* stat);
    static i32 lstat(const char* path, Stat* stat);
    
    static i32 mkdir(const char* path, u32 mode);
    static i32 rmdir(const char* path);
    static i32 unlink(const char* path);
    static i32 rename(const char* oldPath, const char* newPath);
    static i32 link(const char* oldPath, const char* newPath);
    static i32 symlink(const char* target, const char* linkPath);
    static i64 readlink(const char* path, char* buffer, usize size);
    
    static i32 chdir(const char* path);
    static i32 getcwd(char* buffer, usize size);
    
    static i32 getdents(i32 fd, Dirent* dirp, u32 count);
    static i32 ioctl(i32 fd, u64 cmd, u64 arg);
    
    static Inode* lookupPath(const char* path);
    static Inode* lookupPathAt(i32 dirfd, const char* path);
    static MountPoint* findMountPoint(const char* path);
    
    static File* getFile(i32 fd);
    static i32 allocateFd(File* file);
    static void freeFd(i32 fd);
    
    static bool isInitialized();

private:
    static Inode* resolvePath(const char* path, Inode* start);
    static char* normalizePath(const char* path, char* buffer, usize size);
    static const char* getBasename(const char* path);
    static const char* getDirname(const char* path, char* buffer, usize size);
    
    static MountPoint sMountPoints[MAX_MOUNT_POINTS];
    static File sFiles[MAX_OPEN_FILES];
    static Inode sInodes[MAX_INODES];
    static Dentry sDentries[MAX_INODES];
    static u32 sMountCount;
    static u32 sFileCount;
    static u32 sInodeCount;
    static bool sInitialized;
};

}
