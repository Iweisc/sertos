#include "../../include/fs/vfs.hpp"

namespace sertos::fs {

FileSystem* VFS::sFileSystems[MAX_FILESYSTEMS];
usize VFS::sFileSystemCount = 0;
bool VFS::sInitialized = false;

namespace {

usize stringLength(const char* str) {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

void stringCopy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int stringCompare(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

bool startsWith(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return false;
        str++;
        prefix++;
    }
    return true;
}

}

void VFS::initialize() {
    for (usize i = 0; i < MAX_FILESYSTEMS; i++) {
        sFileSystems[i] = nullptr;
    }
    sFileSystemCount = 0;
    sInitialized = true;
}

bool VFS::mount(FileSystem* fs, const char* mountPoint) {
    if (!sInitialized || !fs || !mountPoint) {
        return false;
    }
    
    if (sFileSystemCount >= MAX_FILESYSTEMS) {
        return false;
    }
    
    for (usize i = 0; i < sFileSystemCount; i++) {
        if (sFileSystems[i] && stringCompare(sFileSystems[i]->mountPoint(), mountPoint) == 0) {
            return false;
        }
    }
    
    if (!fs->mount(mountPoint)) {
        return false;
    }
    
    sFileSystems[sFileSystemCount++] = fs;
    return true;
}

bool VFS::unmount(const char* mountPoint) {
    if (!sInitialized || !mountPoint) {
        return false;
    }
    
    for (usize i = 0; i < sFileSystemCount; i++) {
        if (sFileSystems[i] && stringCompare(sFileSystems[i]->mountPoint(), mountPoint) == 0) {
            if (!sFileSystems[i]->unmount()) {
                return false;
            }
            
            for (usize j = i; j < sFileSystemCount - 1; j++) {
                sFileSystems[j] = sFileSystems[j + 1];
            }
            sFileSystems[--sFileSystemCount] = nullptr;
            return true;
        }
    }
    
    return false;
}

FileNode* VFS::open(const char* path, u32 flags) {
    if (!sInitialized || !path) {
        return nullptr;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return nullptr;
    }
    
    return fs->open(resolved, flags);
}

DirectoryNode* VFS::openDir(const char* path) {
    if (!sInitialized || !path) {
        return nullptr;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return nullptr;
    }
    
    return fs->openDir(resolved);
}

bool VFS::exists(const char* path) {
    if (!sInitialized || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return false;
    }
    
    return fs->exists(resolved);
}

bool VFS::isFile(const char* path) {
    if (!sInitialized || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return false;
    }
    
    return fs->isFile(resolved);
}

bool VFS::isDirectory(const char* path) {
    if (!sInitialized || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return false;
    }
    
    return fs->isDirectory(resolved);
}

bool VFS::createFile(const char* path) {
    if (!sInitialized || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return false;
    }
    
    return fs->createFile(resolved);
}

bool VFS::createDirectory(const char* path) {
    if (!sInitialized || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return false;
    }
    
    return fs->createDirectory(resolved);
}

bool VFS::remove(const char* path) {
    if (!sInitialized || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return false;
    }
    
    return fs->remove(resolved);
}

bool VFS::rename(const char* oldPath, const char* newPath) {
    if (!sInitialized || !oldPath || !newPath) {
        return false;
    }
    
    char resolvedOld[MAX_PATH];
    char resolvedNew[MAX_PATH];
    resolvePath(oldPath, resolvedOld);
    resolvePath(newPath, resolvedNew);
    
    FileSystem* fsOld = findFileSystem(resolvedOld);
    FileSystem* fsNew = findFileSystem(resolvedNew);
    
    if (!fsOld || fsOld != fsNew) {
        return false;
    }
    
    return fsOld->rename(resolvedOld, resolvedNew);
}

bool VFS::getInfo(const char* path, FileInfo* info) {
    if (!sInitialized || !path || !info) {
        return false;
    }
    
    char resolved[MAX_PATH];
    resolvePath(path, resolved);
    
    FileSystem* fs = findFileSystem(resolved);
    if (!fs) {
        return false;
    }
    
    return fs->getInfo(resolved, info);
}

FileSystem* VFS::findFileSystem(const char* path) {
    FileSystem* bestMatch = nullptr;
    usize bestMatchLen = 0;
    
    for (usize i = 0; i < sFileSystemCount; i++) {
        if (!sFileSystems[i] || !sFileSystems[i]->isMounted()) {
            continue;
        }
        
        const char* mountPoint = sFileSystems[i]->mountPoint();
        usize mountLen = stringLength(mountPoint);
        
        if (startsWith(path, mountPoint) && mountLen > bestMatchLen) {
            bestMatch = sFileSystems[i];
            bestMatchLen = mountLen;
        }
    }
    
    return bestMatch;
}

void VFS::resolvePath(const char* path, char* resolved) {
    if (!path || !resolved) {
        return;
    }
    
    if (path[0] != '/') {
        resolved[0] = '/';
        stringCopy(resolved + 1, path, MAX_PATH - 1);
    } else {
        stringCopy(resolved, path, MAX_PATH);
    }
    
    usize len = stringLength(resolved);
    while (len > 1 && resolved[len - 1] == '/') {
        resolved[--len] = '\0';
    }
}

}
