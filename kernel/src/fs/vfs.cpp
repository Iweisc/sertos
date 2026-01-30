#include "../../include/fs/vfs.hpp"
#include "../../include/fs/ramfs.hpp"

namespace sertos::fs {

char VFS::sCurrentDirectory[MAX_PATH] = "/";
bool VFS::sInitialized = false;
bool VFS::sRootMounted = false;

usize Path::length(const char* str) {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

void Path::copy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int Path::compare(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

bool Path::isAbsolute(const char* path) {
    return path && path[0] == '/';
}

void Path::normalize(const char* path, char* result) {
    if (!path || !result) return;
    
    char temp[MAX_PATH];
    usize tempLen = 0;
    
    if (path[0] != '/') {
        temp[tempLen++] = '/';
    }
    
    const char* p = path;
    while (*p && tempLen < MAX_PATH - 1) {
        if (*p == '/') {
            if (tempLen > 0 && temp[tempLen - 1] != '/') {
                temp[tempLen++] = '/';
            }
            p++;
            continue;
        }
        
        if (*p == '.') {
            if (*(p + 1) == '/' || *(p + 1) == '\0') {
                p++;
                if (*p == '/') p++;
                continue;
            }
            
            if (*(p + 1) == '.' && (*(p + 2) == '/' || *(p + 2) == '\0')) {
                if (tempLen > 1) {
                    tempLen--;
                    while (tempLen > 0 && temp[tempLen - 1] != '/') {
                        tempLen--;
                    }
                    if (tempLen > 1) tempLen--;
                }
                p += 2;
                if (*p == '/') p++;
                continue;
            }
        }
        
        while (*p && *p != '/' && tempLen < MAX_PATH - 1) {
            temp[tempLen++] = *p++;
        }
    }
    
    if (tempLen > 1 && temp[tempLen - 1] == '/') {
        tempLen--;
    }
    
    if (tempLen == 0) {
        temp[tempLen++] = '/';
    }
    
    temp[tempLen] = '\0';
    copy(result, temp, MAX_PATH);
}

void Path::join(const char* base, const char* relative, char* result) {
    if (!base || !relative || !result) return;
    
    if (isAbsolute(relative)) {
        normalize(relative, result);
        return;
    }
    
    char temp[MAX_PATH];
    usize baseLen = length(base);
    
    if (baseLen >= MAX_PATH - 1) {
        copy(result, base, MAX_PATH);
        return;
    }
    
    copy(temp, base, MAX_PATH);
    
    if (baseLen > 0 && temp[baseLen - 1] != '/') {
        temp[baseLen++] = '/';
        temp[baseLen] = '\0';
    }
    
    usize relLen = length(relative);
    if (baseLen + relLen < MAX_PATH) {
        copy(temp + baseLen, relative, MAX_PATH - baseLen);
    }
    
    normalize(temp, result);
}

void Path::dirname(const char* path, char* result) {
    if (!path || !result) return;
    
    usize len = length(path);
    if (len == 0) {
        result[0] = '.';
        result[1] = '\0';
        return;
    }
    
    while (len > 0 && path[len - 1] == '/') {
        len--;
    }
    
    while (len > 0 && path[len - 1] != '/') {
        len--;
    }
    
    while (len > 1 && path[len - 1] == '/') {
        len--;
    }
    
    if (len == 0) {
        if (path[0] == '/') {
            result[0] = '/';
            result[1] = '\0';
        } else {
            result[0] = '.';
            result[1] = '\0';
        }
        return;
    }
    
    for (usize i = 0; i < len && i < MAX_PATH - 1; i++) {
        result[i] = path[i];
    }
    result[len < MAX_PATH ? len : MAX_PATH - 1] = '\0';
}

void Path::basename(const char* path, char* result) {
    if (!path || !result) return;
    
    usize len = length(path);
    if (len == 0) {
        result[0] = '\0';
        return;
    }
    
    while (len > 0 && path[len - 1] == '/') {
        len--;
    }
    
    if (len == 0) {
        result[0] = '/';
        result[1] = '\0';
        return;
    }
    
    usize start = len;
    while (start > 0 && path[start - 1] != '/') {
        start--;
    }
    
    usize i = 0;
    while (start < len && i < MAX_FILENAME - 1) {
        result[i++] = path[start++];
    }
    result[i] = '\0';
}

void VFS::initialize() {
    Path::copy(sCurrentDirectory, "/", MAX_PATH);
    sInitialized = true;
    sRootMounted = false;
}

bool VFS::mount(const char* fsType, const char* mountPoint) {
    if (!sInitialized || !fsType || !mountPoint) {
        return false;
    }
    
    if (Path::compare(fsType, "ramfs") == 0) {
        if (RamFs::mount(mountPoint)) {
            if (Path::compare(mountPoint, "/") == 0) {
                sRootMounted = true;
            }
            return true;
        }
    }
    
    return false;
}

bool VFS::unmount(const char* mountPoint) {
    if (!sInitialized || !mountPoint) {
        return false;
    }
    
    if (RamFs::isMounted() && Path::compare(RamFs::mountPoint(), mountPoint) == 0) {
        if (RamFs::unmount()) {
            if (Path::compare(mountPoint, "/") == 0) {
                sRootMounted = false;
            }
            return true;
        }
    }
    
    return false;
}

FileHandle VFS::open(const char* path, u32 flags) {
    FileHandle handle = {nullptr, 0, false};
    
    if (!sInitialized || !path || !sRootMounted) {
        return handle;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::open(resolved, flags);
}

i64 VFS::read(FileHandle* handle, void* buffer, usize size) {
    if (!handle || !handle->valid) {
        return -1;
    }
    return RamFs::read(handle, buffer, size);
}

i64 VFS::write(FileHandle* handle, const void* buffer, usize size) {
    if (!handle || !handle->valid) {
        return -1;
    }
    return RamFs::write(handle, buffer, size);
}

i64 VFS::seek(FileHandle* handle, i64 offset, SeekMode mode) {
    if (!handle || !handle->valid) {
        return -1;
    }
    return RamFs::seek(handle, offset, mode);
}

i64 VFS::tell(FileHandle* handle) {
    if (!handle || !handle->valid) {
        return -1;
    }
    return RamFs::tell(handle);
}

bool VFS::close(FileHandle* handle) {
    if (!handle || !handle->valid) {
        return false;
    }
    return RamFs::close(handle);
}

DirHandle VFS::openDir(const char* path) {
    DirHandle handle = {nullptr, false};
    
    if (!sInitialized || !path || !sRootMounted) {
        return handle;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::openDir(resolved);
}

bool VFS::readDir(DirHandle* handle, DirEntry* entry) {
    if (!handle || !handle->valid || !entry) {
        return false;
    }
    return RamFs::readDir(handle, entry);
}

bool VFS::closeDir(DirHandle* handle) {
    if (!handle || !handle->valid) {
        return false;
    }
    return RamFs::closeDir(handle);
}

bool VFS::exists(const char* path) {
    if (!sInitialized || !path || !sRootMounted) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::exists(resolved);
}

bool VFS::isFile(const char* path) {
    if (!sInitialized || !path || !sRootMounted) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::isFile(resolved);
}

bool VFS::isDirectory(const char* path) {
    if (!sInitialized || !path || !sRootMounted) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::isDirectory(resolved);
}

bool VFS::createFile(const char* path) {
    if (!sInitialized || !path || !sRootMounted) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::createFile(resolved);
}

bool VFS::createDirectory(const char* path) {
    if (!sInitialized || !path || !sRootMounted) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::createDirectory(resolved);
}

bool VFS::remove(const char* path) {
    if (!sInitialized || !path || !sRootMounted) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::remove(resolved);
}

bool VFS::rename(const char* oldPath, const char* newPath) {
    if (!sInitialized || !oldPath || !newPath || !sRootMounted) {
        return false;
    }
    
    char resolvedOld[MAX_PATH];
    char resolvedNew[MAX_PATH];
    absolutePath(oldPath, resolvedOld);
    absolutePath(newPath, resolvedNew);
    
    return RamFs::rename(resolvedOld, resolvedNew);
}

bool VFS::getInfo(const char* path, FileInfo* info) {
    if (!sInitialized || !path || !info || !sRootMounted) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return RamFs::getInfo(resolved, info);
}

const char* VFS::currentDirectory() {
    return sCurrentDirectory;
}

bool VFS::changeDirectory(const char* path) {
    if (!sInitialized || !path || !sRootMounted) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    if (!isDirectory(resolved)) {
        return false;
    }
    
    Path::copy(sCurrentDirectory, resolved, MAX_PATH);
    return true;
}

void VFS::absolutePath(const char* path, char* result) {
    if (!path || !result) return;
    
    if (Path::isAbsolute(path)) {
        Path::normalize(path, result);
    } else {
        Path::join(sCurrentDirectory, path, result);
    }
}

void VFS::resolvePath(const char* path, char* resolved) {
    absolutePath(path, resolved);
}

}
