#include "../../include/fs/ramfs.hpp"
#include "../../include/memory/pmm.hpp"

namespace sertos::fs {

namespace {

void memoryCopy(void* dest, const void* src, usize size) {
    auto* d = static_cast<u8*>(dest);
    auto* s = static_cast<const u8*>(src);
    while (size--) {
        *d++ = *s++;
    }
}

void memorySet(void* dest, u8 value, usize size) {
    auto* d = static_cast<u8*>(dest);
    while (size--) {
        *d++ = value;
    }
}

}

RamFsNode* RamFs::sRoot = nullptr;
usize RamFs::sNodeCount = 0;
char RamFs::sMountPoint[MAX_PATH] = "";
bool RamFs::sMounted = false;
RamFsFileHandle RamFs::sFileHandles[MAX_FILE_HANDLES];
RamFsDirHandle RamFs::sDirHandles[MAX_DIR_HANDLES];

void RamFs::initialize() {
    sRoot = nullptr;
    sNodeCount = 0;
    sMounted = false;
    sMountPoint[0] = '\0';
    
    for (usize i = 0; i < MAX_FILE_HANDLES; i++) {
        sFileHandles[i].valid = false;
    }
    for (usize i = 0; i < MAX_DIR_HANDLES; i++) {
        sDirHandles[i].valid = false;
    }
}

bool RamFs::mount(const char* mountPoint) {
    if (sMounted || !mountPoint) {
        return false;
    }
    
    initialize();
    
    sRoot = createNode("/", FileType::Directory, nullptr);
    if (!sRoot) {
        return false;
    }
    
    Path::copy(sMountPoint, mountPoint, MAX_PATH);
    sMounted = true;
    return true;
}

bool RamFs::unmount() {
    if (!sMounted) {
        return false;
    }
    
    if (sRoot) {
        deleteNode(sRoot);
        sRoot = nullptr;
    }
    
    sMounted = false;
    sNodeCount = 0;
    return true;
}

FileHandle RamFs::open(const char* path, u32 flags) {
    FileHandle handle = {nullptr, 0, false};
    
    if (!sMounted || !path) {
        return handle;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    
    if (!node) {
        if (flags & O_CREATE) {
            if (!createFile(path)) {
                return handle;
            }
            node = findNode(relativePath);
        }
        if (!node) {
            return handle;
        }
    }
    
    if (node->type != FileType::Regular) {
        return handle;
    }
    
    if (flags & O_TRUNCATE) {
        node->size = 0;
    }
    
    for (usize i = 0; i < MAX_FILE_HANDLES; i++) {
        if (!sFileHandles[i].valid) {
            sFileHandles[i].node = node;
            sFileHandles[i].position = (flags & O_APPEND) ? node->size : 0;
            sFileHandles[i].flags = flags;
            sFileHandles[i].valid = true;
            
            handle.fsData = &sFileHandles[i];
            handle.flags = flags;
            handle.valid = true;
            return handle;
        }
    }
    
    return handle;
}

i64 RamFs::read(FileHandle* handle, void* buffer, usize size) {
    if (!handle || !handle->valid || !buffer) {
        return -1;
    }
    
    auto* fh = static_cast<RamFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid || !fh->node) {
        return -1;
    }
    
    if (fh->position >= fh->node->size) {
        return 0;
    }
    
    usize bytesToRead = size;
    if (fh->position + bytesToRead > fh->node->size) {
        bytesToRead = fh->node->size - fh->position;
    }
    
    memoryCopy(buffer, fh->node->data + fh->position, bytesToRead);
    fh->position += bytesToRead;
    
    return static_cast<i64>(bytesToRead);
}

i64 RamFs::write(FileHandle* handle, const void* buffer, usize size) {
    if (!handle || !handle->valid || !buffer) {
        return -1;
    }
    
    auto* fh = static_cast<RamFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid || !fh->node) {
        return -1;
    }
    
    usize newSize = fh->position + size;
    
    if (newSize > fh->node->capacity) {
        usize newCapacity = newSize * 2;
        if (newCapacity > RAMFS_MAX_FILE_SIZE) {
            newCapacity = RAMFS_MAX_FILE_SIZE;
        }
        if (newSize > newCapacity) {
            return -1;
        }
        
        usize pages = (newCapacity + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
        u8* newData = static_cast<u8*>(memory::PMM::allocatePages(pages));
        if (!newData) {
            return -1;
        }
        
        if (fh->node->data && fh->node->size > 0) {
            memoryCopy(newData, fh->node->data, fh->node->size);
        }
        
        if (fh->node->data) {
            usize oldPages = (fh->node->capacity + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
            memory::PMM::freePages(fh->node->data, oldPages);
        }
        
        fh->node->data = newData;
        fh->node->capacity = newCapacity;
    }
    
    memoryCopy(fh->node->data + fh->position, buffer, size);
    fh->position += size;
    
    if (fh->position > fh->node->size) {
        fh->node->size = fh->position;
    }
    
    return static_cast<i64>(size);
}

i64 RamFs::seek(FileHandle* handle, i64 offset, SeekMode mode) {
    if (!handle || !handle->valid) {
        return -1;
    }
    
    auto* fh = static_cast<RamFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid || !fh->node) {
        return -1;
    }
    
    i64 newPos;
    switch (mode) {
        case SeekMode::Set:
            newPos = offset;
            break;
        case SeekMode::Current:
            newPos = static_cast<i64>(fh->position) + offset;
            break;
        case SeekMode::End:
            newPos = static_cast<i64>(fh->node->size) + offset;
            break;
        default:
            return -1;
    }
    
    if (newPos < 0) {
        return -1;
    }
    
    fh->position = static_cast<usize>(newPos);
    return static_cast<i64>(fh->position);
}

i64 RamFs::tell(FileHandle* handle) {
    if (!handle || !handle->valid) {
        return -1;
    }
    
    auto* fh = static_cast<RamFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid) {
        return -1;
    }
    
    return static_cast<i64>(fh->position);
}

bool RamFs::close(FileHandle* handle) {
    if (!handle || !handle->valid) {
        return false;
    }
    
    auto* fh = static_cast<RamFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid) {
        return false;
    }
    
    fh->valid = false;
    fh->node = nullptr;
    handle->valid = false;
    handle->fsData = nullptr;
    
    return true;
}

DirHandle RamFs::openDir(const char* path) {
    DirHandle handle = {nullptr, false};
    
    if (!sMounted || !path) {
        return handle;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    
    if (!node || node->type != FileType::Directory) {
        return handle;
    }
    
    for (usize i = 0; i < MAX_DIR_HANDLES; i++) {
        if (!sDirHandles[i].valid) {
            sDirHandles[i].node = node;
            sDirHandles[i].current = node->children;
            sDirHandles[i].valid = true;
            
            handle.fsData = &sDirHandles[i];
            handle.valid = true;
            return handle;
        }
    }
    
    return handle;
}

bool RamFs::readDir(DirHandle* handle, DirEntry* entry) {
    if (!handle || !handle->valid || !entry) {
        return false;
    }
    
    auto* dh = static_cast<RamFsDirHandle*>(handle->fsData);
    if (!dh || !dh->valid || !dh->current) {
        return false;
    }
    
    Path::copy(entry->name, dh->current->name, MAX_FILENAME);
    entry->type = dh->current->type;
    entry->inode = reinterpret_cast<u64>(dh->current);
    
    dh->current = dh->current->next;
    return true;
}

bool RamFs::closeDir(DirHandle* handle) {
    if (!handle || !handle->valid) {
        return false;
    }
    
    auto* dh = static_cast<RamFsDirHandle*>(handle->fsData);
    if (!dh || !dh->valid) {
        return false;
    }
    
    dh->valid = false;
    dh->node = nullptr;
    dh->current = nullptr;
    handle->valid = false;
    handle->fsData = nullptr;
    
    return true;
}

bool RamFs::exists(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    return findNode(relativePath) != nullptr;
}

bool RamFs::isFile(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    return node && node->type == FileType::Regular;
}

bool RamFs::isDirectory(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    return node && node->type == FileType::Directory;
}

bool RamFs::createFile(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    
    if (findNode(relativePath)) {
        return false;
    }
    
    char childName[MAX_FILENAME];
    RamFsNode* parent = findParent(relativePath, childName);
    
    if (!parent || parent->type != FileType::Directory) {
        return false;
    }
    
    RamFsNode* node = createNode(childName, FileType::Regular, parent);
    return node != nullptr;
}

bool RamFs::createDirectory(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    
    if (findNode(relativePath)) {
        return false;
    }
    
    char childName[MAX_FILENAME];
    RamFsNode* parent = findParent(relativePath, childName);
    
    if (!parent || parent->type != FileType::Directory) {
        return false;
    }
    
    RamFsNode* node = createNode(childName, FileType::Directory, parent);
    return node != nullptr;
}

bool RamFs::remove(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    
    if (!node || node == sRoot) {
        return false;
    }
    
    if (node->type == FileType::Directory && node->children) {
        return false;
    }
    
    RamFsNode* parent = node->parent;
    if (parent) {
        if (parent->children == node) {
            parent->children = node->next;
        } else {
            RamFsNode* prev = parent->children;
            while (prev && prev->next != node) {
                prev = prev->next;
            }
            if (prev) {
                prev->next = node->next;
            }
        }
    }
    
    if (node->data) {
        usize pages = (node->capacity + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
        memory::PMM::freePages(node->data, pages);
    }
    
    memory::PMM::freePage(node);
    sNodeCount--;
    
    return true;
}

bool RamFs::rename(const char* oldPath, const char* newPath) {
    if (!sMounted || !oldPath || !newPath) {
        return false;
    }
    
    const char* oldRelative = skipMountPoint(oldPath);
    const char* newRelative = skipMountPoint(newPath);
    
    RamFsNode* node = findNode(oldRelative);
    if (!node || node == sRoot) {
        return false;
    }
    
    if (findNode(newRelative)) {
        return false;
    }
    
    char newName[MAX_FILENAME];
    RamFsNode* newParent = findParent(newRelative, newName);
    
    if (!newParent || newParent->type != FileType::Directory) {
        return false;
    }
    
    RamFsNode* oldParent = node->parent;
    if (oldParent) {
        if (oldParent->children == node) {
            oldParent->children = node->next;
        } else {
            RamFsNode* prev = oldParent->children;
            while (prev && prev->next != node) {
                prev = prev->next;
            }
            if (prev) {
                prev->next = node->next;
            }
        }
    }
    
    Path::copy(node->name, newName, MAX_FILENAME);
    node->parent = newParent;
    node->next = newParent->children;
    newParent->children = node;
    
    return true;
}

bool RamFs::getInfo(const char* path, FileInfo* info) {
    if (!sMounted || !path || !info) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    
    if (!node) {
        return false;
    }
    
    Path::copy(info->name, node->name, MAX_FILENAME);
    info->type = node->type;
    info->size = node->size;
    info->createdTime = node->createdTime;
    info->modifiedTime = node->modifiedTime;
    info->accessedTime = node->modifiedTime;
    info->permissions = 0755;
    info->uid = 0;
    info->gid = 0;
    
    return true;
}

RamFsNode* RamFs::findNode(const char* path) {
    if (!path || !sRoot) {
        return nullptr;
    }
    
    if (path[0] == '/' && path[1] == '\0') {
        return sRoot;
    }
    
    RamFsNode* current = sRoot;
    const char* p = path;
    
    if (*p == '/') p++;
    
    while (*p) {
        if (current->type != FileType::Directory) {
            return nullptr;
        }
        
        char component[MAX_FILENAME];
        usize i = 0;
        while (*p && *p != '/' && i < MAX_FILENAME - 1) {
            component[i++] = *p++;
        }
        component[i] = '\0';
        
        if (*p == '/') p++;
        
        if (i == 0) continue;
        
        RamFsNode* child = current->children;
        while (child) {
            if (Path::compare(child->name, component) == 0) {
                break;
            }
            child = child->next;
        }
        
        if (!child) {
            return nullptr;
        }
        
        current = child;
    }
    
    return current;
}

RamFsNode* RamFs::findParent(const char* path, char* childName) {
    if (!path || !childName || !sRoot) {
        return nullptr;
    }
    
    const char* lastSlash = nullptr;
    const char* p = path;
    while (*p) {
        if (*p == '/') {
            lastSlash = p;
        }
        p++;
    }
    
    if (!lastSlash || lastSlash == path) {
        Path::copy(childName, path[0] == '/' ? path + 1 : path, MAX_FILENAME);
        return sRoot;
    }
    
    char parentPath[MAX_PATH];
    usize parentLen = lastSlash - path;
    usize i;
    for (i = 0; i < parentLen && i < MAX_PATH - 1; i++) {
        parentPath[i] = path[i];
    }
    parentPath[i] = '\0';
    
    Path::copy(childName, lastSlash + 1, MAX_FILENAME);
    
    return findNode(parentPath);
}

RamFsNode* RamFs::createNode(const char* name, FileType type, RamFsNode* parent) {
    if (sNodeCount >= RAMFS_MAX_FILES) {
        return nullptr;
    }
    
    auto* node = static_cast<RamFsNode*>(memory::PMM::allocatePage());
    if (!node) {
        return nullptr;
    }
    
    memorySet(node, 0, sizeof(RamFsNode));
    Path::copy(node->name, name, MAX_FILENAME);
    node->type = type;
    node->data = nullptr;
    node->size = 0;
    node->capacity = 0;
    node->parent = parent;
    node->children = nullptr;
    node->next = nullptr;
    node->createdTime = 0;
    node->modifiedTime = 0;
    
    if (parent) {
        node->next = parent->children;
        parent->children = node;
    }
    
    sNodeCount++;
    return node;
}

void RamFs::deleteNode(RamFsNode* node) {
    if (!node) {
        return;
    }
    
    RamFsNode* child = node->children;
    while (child) {
        RamFsNode* next = child->next;
        deleteNode(child);
        child = next;
    }
    
    if (node->data) {
        usize pages = (node->capacity + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
        memory::PMM::freePages(node->data, pages);
    }
    
    memory::PMM::freePage(node);
    sNodeCount--;
}

const char* RamFs::skipMountPoint(const char* path) {
    usize mountLen = Path::length(sMountPoint);
    
    if (mountLen > 0 && path[0] == '/' && sMountPoint[0] == '/') {
        const char* p = path;
        const char* m = sMountPoint;
        
        while (*m && *p == *m) {
            p++;
            m++;
        }
        
        if (*m == '\0') {
            if (*p == '\0') return "/";
            if (*p == '/') return p;
        }
    }
    
    return path;
}

}
