#include "../../include/fs/vfs.hpp"
#include "../../include/fs/sertfs.hpp"
#include "../../include/process/process.hpp"

namespace sertos::fs {

MountPoint VFS::sMountPoints[MAX_MOUNT_POINTS];
File VFS::sFiles[MAX_OPEN_FILES];
Inode VFS::sInodes[MAX_INODES];
Dentry VFS::sDentries[MAX_INODES];
u32 VFS::sMountCount = 0;
u32 VFS::sFileCount = 0;
u32 VFS::sInodeCount = 0;
bool VFS::sInitialized = false;

void VFS::initialize() {
    for (u32 i = 0; i < MAX_MOUNT_POINTS; i++) {
        sMountPoints[i].valid = false;
    }
    for (u32 i = 0; i < MAX_OPEN_FILES; i++) {
        sFiles[i].valid = false;
    }
    for (u32 i = 0; i < MAX_INODES; i++) {
        sInodes[i].valid = false;
        sDentries[i].valid = false;
    }
    
    sMountCount = 0;
    sFileCount = 0;
    sInodeCount = 0;
    sInitialized = true;
}

i32 VFS::mount(const char* source, const char* target, FileSystemType type, u32 flags) {
    if (sMountCount >= MAX_MOUNT_POINTS) return -1;
    
    for (u32 i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (!sMountPoints[i].valid) {
            usize j = 0;
            while (target && target[j] && j < MAX_PATH_LENGTH - 1) {
                sMountPoints[i].path[j] = target[j];
                j++;
            }
            sMountPoints[i].path[j] = '\0';
            
            sMountPoints[i].type = type;
            sMountPoints[i].flags = flags;
            sMountPoints[i].sb = nullptr;
            sMountPoints[i].mountPoint = nullptr;
            sMountPoints[i].valid = true;
            sMountCount++;
            
            (void)source;
            return 0;
        }
    }
    
    return -1;
}

i32 VFS::umount(const char* target) {
    for (u32 i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (sMountPoints[i].valid) {
            bool match = true;
            for (usize j = 0; target[j] || sMountPoints[i].path[j]; j++) {
                if (target[j] != sMountPoints[i].path[j]) {
                    match = false;
                    break;
                }
            }
            
            if (match) {
                sMountPoints[i].valid = false;
                sMountCount--;
                return 0;
            }
        }
    }
    
    return -1;
}

i32 VFS::open(const char* path, i32 flags, u32 mode) {
    (void)mode;
    
    for (u32 i = 0; i < MAX_OPEN_FILES; i++) {
        if (!sFiles[i].valid) {
            sFiles[i].valid = true;
            sFiles[i].flags = static_cast<u32>(flags);
            sFiles[i].offset = 0;
            sFiles[i].refCount = 1;
            sFiles[i].inode = lookupPath(path);
            sFiles[i].fops = nullptr;
            sFiles[i].privateData = nullptr;
            
            sFileCount++;
            return static_cast<i32>(i);
        }
    }
    
    return -1;
}

i32 VFS::openat(i32 dirfd, const char* path, i32 flags, u32 mode) {
    if (dirfd == AT_FDCWD || (path && path[0] == '/')) {
        return open(path, flags, mode);
    }
    
    return open(path, flags, mode);
}

i32 VFS::close(i32 fd) {
    if (fd < 0 || fd >= static_cast<i32>(MAX_OPEN_FILES)) return -1;
    if (!sFiles[fd].valid) return -1;
    
    sFiles[fd].refCount--;
    if (sFiles[fd].refCount == 0) {
        sFiles[fd].valid = false;
        sFileCount--;
    }
    
    return 0;
}

i64 VFS::read(i32 fd, void* buffer, usize size) {
    if (fd < 0 || fd >= static_cast<i32>(MAX_OPEN_FILES)) return -1;
    if (!sFiles[fd].valid || !buffer) return -1;
    
    File* file = &sFiles[fd];
    
    if (file->fops && file->fops->read) {
        return file->fops->read(file, buffer, size, file->offset);
    }
    
    return 0;
}

i64 VFS::write(i32 fd, const void* buffer, usize size) {
    if (fd < 0 || fd >= static_cast<i32>(MAX_OPEN_FILES)) return -1;
    if (!sFiles[fd].valid || !buffer) return -1;
    
    File* file = &sFiles[fd];
    
    if (file->fops && file->fops->write) {
        return file->fops->write(file, buffer, size, file->offset);
    }
    
    return 0;
}

i64 VFS::lseek(i32 fd, i64 offset, i32 whence) {
    if (fd < 0 || fd >= static_cast<i32>(MAX_OPEN_FILES)) return -1;
    if (!sFiles[fd].valid) return -1;
    
    File* file = &sFiles[fd];
    i64 newOffset;
    
    switch (whence) {
        case SEEK_SET:
            newOffset = offset;
            break;
        case SEEK_CUR:
            newOffset = static_cast<i64>(file->offset) + offset;
            break;
        case SEEK_END:
            if (file->inode) {
                newOffset = static_cast<i64>(file->inode->size) + offset;
            } else {
                newOffset = offset;
            }
            break;
        default:
            return -1;
    }
    
    if (newOffset < 0) return -1;
    
    file->offset = static_cast<u64>(newOffset);
    return newOffset;
}

i32 VFS::fstat(i32 fd, Stat* stat) {
    if (fd < 0 || fd >= static_cast<i32>(MAX_OPEN_FILES)) return -1;
    if (!sFiles[fd].valid || !stat) return -1;
    
    File* file = &sFiles[fd];
    if (!file->inode) return -1;
    
    stat->dev = 0;
    stat->ino = file->inode->ino;
    stat->mode = file->inode->mode;
    stat->nlink = file->inode->nlink;
    stat->uid = file->inode->uid;
    stat->gid = file->inode->gid;
    stat->rdev = 0;
    stat->size = static_cast<i64>(file->inode->size);
    stat->blksize = 4096;
    stat->blocks = static_cast<i64>((file->inode->size + 511) / 512);
    stat->atime = file->inode->atime;
    stat->mtime = file->inode->mtime;
    stat->ctime = file->inode->ctime;
    
    return 0;
}

i32 VFS::stat(const char* path, Stat* stat) {
    if (!path || !stat) return -1;
    
    Inode* inode = lookupPath(path);
    if (!inode) return -1;
    
    stat->dev = 0;
    stat->ino = inode->ino;
    stat->mode = inode->mode;
    stat->nlink = inode->nlink;
    stat->uid = inode->uid;
    stat->gid = inode->gid;
    stat->rdev = 0;
    stat->size = static_cast<i64>(inode->size);
    stat->blksize = 4096;
    stat->blocks = static_cast<i64>((inode->size + 511) / 512);
    stat->atime = inode->atime;
    stat->mtime = inode->mtime;
    stat->ctime = inode->ctime;
    
    return 0;
}

i32 VFS::lstat(const char* path, Stat* stat) {
    return VFS::stat(path, stat);
}

i32 VFS::mkdir(const char* path, u32 mode) {
    if (!path) return -1;
    
    for (u32 i = 0; i < MAX_INODES; i++) {
        if (!sInodes[i].valid) {
            sInodes[i].valid = true;
            sInodes[i].ino = i;
            sInodes[i].mode = mode | S_IFDIR;
            sInodes[i].nlink = 2;
            sInodes[i].uid = 0;
            sInodes[i].gid = 0;
            sInodes[i].size = 0;
            sInodes[i].atime = 0;
            sInodes[i].mtime = 0;
            sInodes[i].ctime = 0;
            sInodes[i].blocks = 0;
            sInodes[i].sb = nullptr;
            sInodes[i].iops = nullptr;
            sInodes[i].fops = nullptr;
            sInodes[i].privateData = nullptr;
            
            for (u32 j = 0; j < MAX_INODES; j++) {
                if (!sDentries[j].valid) {
                    sDentries[j].valid = true;
                    sDentries[j].inode = &sInodes[i];
                    sDentries[j].parent = nullptr;
                    sDentries[j].children = nullptr;
                    sDentries[j].next = nullptr;
                    sDentries[j].refCount = 1;
                    
                    usize k = 0;
                    while (path[k] && k < MAX_FILENAME_LENGTH - 1) {
                        sDentries[j].name[k] = path[k];
                        k++;
                    }
                    sDentries[j].name[k] = '\0';
                    break;
                }
            }
            
            sInodeCount++;
            return 0;
        }
    }
    
    return -1;
}

i32 VFS::rmdir(const char* path) {
    if (!path) return -1;
    
    Inode* inode = lookupPath(path);
    if (!inode) return -1;
    if ((inode->mode & S_IFMT) != S_IFDIR) return -1;
    
    inode->valid = false;
    sInodeCount--;
    return 0;
}

i32 VFS::unlink(const char* path) {
    if (!path) return -1;
    
    Inode* inode = lookupPath(path);
    if (!inode) return -1;
    if ((inode->mode & S_IFMT) == S_IFDIR) return -1;
    
    inode->valid = false;
    sInodeCount--;
    return 0;
}

i32 VFS::rename(const char* oldPath, const char* newPath) {
    if (!oldPath || !newPath) return -1;
    
    for (u32 i = 0; i < MAX_INODES; i++) {
        if (sDentries[i].valid) {
            bool match = true;
            for (usize j = 0; oldPath[j] || sDentries[i].name[j]; j++) {
                if (oldPath[j] != sDentries[i].name[j]) {
                    match = false;
                    break;
                }
            }
            
            if (match) {
                usize k = 0;
                while (newPath[k] && k < MAX_FILENAME_LENGTH - 1) {
                    sDentries[i].name[k] = newPath[k];
                    k++;
                }
                sDentries[i].name[k] = '\0';
                return 0;
            }
        }
    }
    
    return -1;
}

i32 VFS::link(const char* oldPath, const char* newPath) {
    (void)oldPath;
    (void)newPath;
    return -1;
}

i32 VFS::symlink(const char* target, const char* linkPath) {
    (void)target;
    (void)linkPath;
    return -1;
}

i64 VFS::readlink(const char* path, char* buffer, usize size) {
    (void)path;
    (void)buffer;
    (void)size;
    return -1;
}

i32 VFS::chdir(const char* path) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return -1;
    
    Inode* inode = lookupPath(path);
    if (!inode) return -1;
    if ((inode->mode & S_IFMT) != S_IFDIR) return -1;
    
    usize i = 0;
    while (path[i] && i < process::MAX_CWD_LEN - 1) {
        current->cwd[i] = path[i];
        i++;
    }
    current->cwd[i] = '\0';
    
    return 0;
}

i32 VFS::getcwd(char* buffer, usize size) {
    process::Process* current = process::PM::currentProcess();
    if (!current || !buffer) return -1;
    
    usize i = 0;
    while (current->cwd[i] && i < size - 1) {
        buffer[i] = current->cwd[i];
        i++;
    }
    buffer[i] = '\0';
    
    return 0;
}

i32 VFS::getdents(i32 fd, Dirent* dirp, u32 count) {
    (void)fd;
    (void)dirp;
    (void)count;
    return 0;
}

i32 VFS::ioctl(i32 fd, u64 cmd, u64 arg) {
    if (fd < 0 || fd >= static_cast<i32>(MAX_OPEN_FILES)) return -1;
    if (!sFiles[fd].valid) return -1;
    
    File* file = &sFiles[fd];
    
    if (file->fops && file->fops->ioctl) {
        return static_cast<i32>(file->fops->ioctl(file, cmd, arg));
    }
    
    return -1;
}

Inode* VFS::lookupPath(const char* path) {
    if (!path) return nullptr;
    
    for (u32 i = 0; i < MAX_INODES; i++) {
        if (sDentries[i].valid) {
            bool match = true;
            for (usize j = 0; path[j] || sDentries[i].name[j]; j++) {
                if (path[j] != sDentries[i].name[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return sDentries[i].inode;
        }
    }
    
    return nullptr;
}

Inode* VFS::lookupPathAt(i32 dirfd, const char* path) {
    (void)dirfd;
    return lookupPath(path);
}

MountPoint* VFS::findMountPoint(const char* path) {
    if (!path) return nullptr;
    
    MountPoint* bestMatch = nullptr;
    usize bestLen = 0;
    
    for (u32 i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (sMountPoints[i].valid) {
            usize len = 0;
            bool match = true;
            
            for (usize j = 0; sMountPoints[i].path[j]; j++) {
                if (path[j] != sMountPoints[i].path[j]) {
                    match = false;
                    break;
                }
                len++;
            }
            
            if (match && len > bestLen) {
                bestMatch = &sMountPoints[i];
                bestLen = len;
            }
        }
    }
    
    return bestMatch;
}

File* VFS::getFile(i32 fd) {
    if (fd < 0 || fd >= static_cast<i32>(MAX_OPEN_FILES)) return nullptr;
    if (!sFiles[fd].valid) return nullptr;
    return &sFiles[fd];
}

i32 VFS::allocateFd(File* file) {
    if (!file) return -1;
    
    for (u32 i = 0; i < MAX_OPEN_FILES; i++) {
        if (!sFiles[i].valid) {
            sFiles[i] = *file;
            sFiles[i].valid = true;
            sFileCount++;
            return static_cast<i32>(i);
        }
    }
    
    return -1;
}

void VFS::freeFd(i32 fd) {
    if (fd >= 0 && fd < static_cast<i32>(MAX_OPEN_FILES)) {
        if (sFiles[fd].valid) {
            sFiles[fd].valid = false;
            sFileCount--;
        }
    }
}

bool VFS::isInitialized() {
    return sInitialized;
}

Inode* VFS::resolvePath(const char* path, Inode* start) {
    (void)start;
    return lookupPath(path);
}

char* VFS::normalizePath(const char* path, char* buffer, usize size) {
    if (!path || !buffer) return nullptr;
    
    usize i = 0;
    while (path[i] && i < size - 1) {
        buffer[i] = path[i];
        i++;
    }
    buffer[i] = '\0';
    
    return buffer;
}

const char* VFS::getBasename(const char* path) {
    if (!path) return nullptr;
    
    const char* last = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/') last = p + 1;
    }
    
    return last;
}

const char* VFS::getDirname(const char* path, char* buffer, usize size) {
    if (!path || !buffer) return nullptr;
    
    usize lastSlash = 0;
    usize i = 0;
    
    while (path[i]) {
        if (path[i] == '/') lastSlash = i;
        i++;
    }
    
    if (lastSlash == 0 && path[0] == '/') {
        buffer[0] = '/';
        buffer[1] = '\0';
    } else if (lastSlash == 0) {
        buffer[0] = '.';
        buffer[1] = '\0';
    } else {
        for (usize j = 0; j < lastSlash && j < size - 1; j++) {
            buffer[j] = path[j];
        }
        buffer[lastSlash < size - 1 ? lastSlash : size - 1] = '\0';
    }
    
    return buffer;
}

}
