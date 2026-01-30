#include "../../include/fs/ramfs.hpp"
#include "../../include/memory/pmm.hpp"

namespace sertos::fs {

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

void RamFsFile::init(RamFsNode* node) {
    stringCopy(mName, node->name, MAX_FILENAME);
    mType = node->type;
    mNode = node;
    mPosition = 0;
    mClosed = false;
}

RamFsFile::~RamFsFile() {
    close();
}

i64 RamFsFile::read(void* buffer, usize size) {
    if (mClosed || !mNode || !buffer) {
        return -1;
    }
    
    if (mPosition >= mNode->size) {
        return 0;
    }
    
    usize bytesToRead = size;
    if (mPosition + bytesToRead > mNode->size) {
        bytesToRead = mNode->size - mPosition;
    }
    
    memoryCopy(buffer, mNode->data + mPosition, bytesToRead);
    mPosition += bytesToRead;
    
    return static_cast<i64>(bytesToRead);
}

i64 RamFsFile::write(const void* buffer, usize size) {
    if (mClosed || !mNode || !buffer) {
        return -1;
    }
    
    usize newSize = mPosition + size;
    
    if (newSize > mNode->capacity) {
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
        
        if (mNode->data && mNode->size > 0) {
            memoryCopy(newData, mNode->data, mNode->size);
        }
        
        if (mNode->data) {
            usize oldPages = (mNode->capacity + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
            memory::PMM::freePages(mNode->data, oldPages);
        }
        
        mNode->data = newData;
        mNode->capacity = newCapacity;
    }
    
    memoryCopy(mNode->data + mPosition, buffer, size);
    mPosition += size;
    
    if (mPosition > mNode->size) {
        mNode->size = mPosition;
    }
    
    return static_cast<i64>(size);
}

i64 RamFsFile::seek(i64 offset, SeekMode mode) {
    if (mClosed || !mNode) {
        return -1;
    }
    
    i64 newPos;
    switch (mode) {
        case SeekMode::Set:
            newPos = offset;
            break;
        case SeekMode::Current:
            newPos = static_cast<i64>(mPosition) + offset;
            break;
        case SeekMode::End:
            newPos = static_cast<i64>(mNode->size) + offset;
            break;
        default:
            return -1;
    }
    
    if (newPos < 0) {
        return -1;
    }
    
    mPosition = static_cast<usize>(newPos);
    return static_cast<i64>(mPosition);
}

i64 RamFsFile::tell() {
    if (mClosed) {
        return -1;
    }
    return static_cast<i64>(mPosition);
}

bool RamFsFile::close() {
    if (mClosed) {
        return false;
    }
    mClosed = true;
    return true;
}

bool RamFsFile::getInfo(FileInfo* info) {
    if (!info || !mNode) {
        return false;
    }
    
    stringCopy(info->name, mNode->name, MAX_FILENAME);
    info->type = mNode->type;
    info->size = mNode->size;
    info->createdTime = mNode->createdTime;
    info->modifiedTime = mNode->modifiedTime;
    info->accessedTime = mNode->modifiedTime;
    info->permissions = 0755;
    info->uid = 0;
    info->gid = 0;
    
    return true;
}

void RamFsDirectory::init(RamFsNode* node) {
    stringCopy(mPath, node->name, MAX_PATH);
    mNode = node;
    mCurrent = node->children;
    mClosed = false;
}

RamFsDirectory::~RamFsDirectory() {
    close();
}

bool RamFsDirectory::readEntry(DirEntry* entry) {
    if (mClosed || !entry || !mCurrent) {
        return false;
    }
    
    stringCopy(entry->name, mCurrent->name, MAX_FILENAME);
    entry->type = mCurrent->type;
    entry->inode = reinterpret_cast<u64>(mCurrent);
    
    mCurrent = mCurrent->next;
    return true;
}

bool RamFsDirectory::rewind() {
    if (mClosed || !mNode) {
        return false;
    }
    mCurrent = mNode->children;
    return true;
}

bool RamFsDirectory::close() {
    if (mClosed) {
        return false;
    }
    mClosed = true;
    return true;
}

usize RamFsDirectory::count() {
    if (!mNode) {
        return 0;
    }
    
    usize cnt = 0;
    RamFsNode* child = mNode->children;
    while (child) {
        cnt++;
        child = child->next;
    }
    return cnt;
}

RamFs::RamFs() : mRoot(nullptr), mNodeCount(0) {
    stringCopy(mName, "ramfs", 32);
    mMounted = false;
}

RamFs::~RamFs() {
    unmount();
}

bool RamFs::mount(const char* mountPoint) {
    if (mMounted || !mountPoint) {
        return false;
    }
    
    mRoot = createNode("/", FileType::Directory, nullptr);
    if (!mRoot) {
        return false;
    }
    
    stringCopy(mMountPoint, mountPoint, MAX_PATH);
    mMounted = true;
    return true;
}

bool RamFs::unmount() {
    if (!mMounted) {
        return false;
    }
    
    if (mRoot) {
        deleteNode(mRoot);
        mRoot = nullptr;
    }
    
    mMounted = false;
    mNodeCount = 0;
    return true;
}

FileNode* RamFs::open(const char* path, u32 flags) {
    if (!mMounted || !path) {
        return nullptr;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    
    if (!node) {
        if (flags & O_CREATE) {
            if (!createFile(path)) {
                return nullptr;
            }
            node = findNode(relativePath);
        }
        if (!node) {
            return nullptr;
        }
    }
    
    if (node->type != FileType::Regular) {
        return nullptr;
    }
    
    if (flags & O_TRUNCATE) {
        node->size = 0;
    }
    
    void* mem = memory::PMM::allocatePage();
    if (!mem) {
        return nullptr;
    }
    
    auto* file = static_cast<RamFsFile*>(mem);
    file->init(node);
    
    if (flags & O_APPEND) {
        file->seek(0, SeekMode::End);
    }
    
    return file;
}

DirectoryNode* RamFs::openDir(const char* path) {
    if (!mMounted || !path) {
        return nullptr;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    
    if (!node || node->type != FileType::Directory) {
        return nullptr;
    }
    
    void* mem = memory::PMM::allocatePage();
    if (!mem) {
        return nullptr;
    }
    
    auto* dir = static_cast<RamFsDirectory*>(mem);
    dir->init(node);
    return dir;
}

bool RamFs::exists(const char* path) {
    if (!mMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    return findNode(relativePath) != nullptr;
}

bool RamFs::isFile(const char* path) {
    if (!mMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    return node && node->type == FileType::Regular;
}

bool RamFs::isDirectory(const char* path) {
    if (!mMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    return node && node->type == FileType::Directory;
}

bool RamFs::createFile(const char* path) {
    if (!mMounted || !path) {
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
    if (!mMounted || !path) {
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
    if (!mMounted || !path) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    
    if (!node || node == mRoot) {
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
    mNodeCount--;
    
    return true;
}

bool RamFs::rename(const char* oldPath, const char* newPath) {
    if (!mMounted || !oldPath || !newPath) {
        return false;
    }
    
    const char* oldRelative = skipMountPoint(oldPath);
    const char* newRelative = skipMountPoint(newPath);
    
    RamFsNode* node = findNode(oldRelative);
    if (!node || node == mRoot) {
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
    
    stringCopy(node->name, newName, MAX_FILENAME);
    node->parent = newParent;
    node->next = newParent->children;
    newParent->children = node;
    
    return true;
}

bool RamFs::getInfo(const char* path, FileInfo* info) {
    if (!mMounted || !path || !info) {
        return false;
    }
    
    const char* relativePath = skipMountPoint(path);
    RamFsNode* node = findNode(relativePath);
    
    if (!node) {
        return false;
    }
    
    stringCopy(info->name, node->name, MAX_FILENAME);
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
    if (!path || !mRoot) {
        return nullptr;
    }
    
    if (path[0] == '/' && path[1] == '\0') {
        return mRoot;
    }
    
    RamFsNode* current = mRoot;
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
            if (stringCompare(child->name, component) == 0) {
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
    if (!path || !childName || !mRoot) {
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
        stringCopy(childName, path[0] == '/' ? path + 1 : path, MAX_FILENAME);
        return mRoot;
    }
    
    char parentPath[MAX_PATH];
    usize parentLen = lastSlash - path;
    usize i;
    for (i = 0; i < parentLen && i < MAX_PATH - 1; i++) {
        parentPath[i] = path[i];
    }
    parentPath[i] = '\0';
    
    stringCopy(childName, lastSlash + 1, MAX_FILENAME);
    
    return findNode(parentPath);
}

RamFsNode* RamFs::createNode(const char* name, FileType type, RamFsNode* parent) {
    if (mNodeCount >= RAMFS_MAX_FILES) {
        return nullptr;
    }
    
    auto* node = static_cast<RamFsNode*>(memory::PMM::allocatePage());
    if (!node) {
        return nullptr;
    }
    
    memorySet(node, 0, sizeof(RamFsNode));
    stringCopy(node->name, name, MAX_FILENAME);
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
    
    mNodeCount++;
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
    mNodeCount--;
}

const char* RamFs::skipMountPoint(const char* path) {
    usize mountLen = stringLength(mMountPoint);
    
    if (mountLen > 0 && path[0] == '/' && mMountPoint[0] == '/') {
        const char* p = path;
        const char* m = mMountPoint;
        
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
