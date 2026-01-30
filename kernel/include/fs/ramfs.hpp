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

class RamFsFile : public FileNode {
public:
    void init(RamFsNode* node);
    ~RamFsFile() override;
    
    i64 read(void* buffer, usize size) override;
    i64 write(const void* buffer, usize size) override;
    i64 seek(i64 offset, SeekMode mode) override;
    i64 tell() override;
    bool close() override;
    bool getInfo(FileInfo* info) override;

    RamFsNode* mNode;
    usize mPosition;
    bool mClosed;
};

class RamFsDirectory : public DirectoryNode {
public:
    void init(RamFsNode* node);
    ~RamFsDirectory() override;
    
    bool readEntry(DirEntry* entry) override;
    bool rewind() override;
    bool close() override;
    usize count() override;

    RamFsNode* mNode;
    RamFsNode* mCurrent;
    bool mClosed;
};

class RamFs : public FileSystem {
public:
    RamFs();
    ~RamFs() override;
    
    bool mount(const char* mountPoint) override;
    bool unmount() override;
    
    FileNode* open(const char* path, u32 flags) override;
    DirectoryNode* openDir(const char* path) override;
    
    bool exists(const char* path) override;
    bool isFile(const char* path) override;
    bool isDirectory(const char* path) override;
    
    bool createFile(const char* path) override;
    bool createDirectory(const char* path) override;
    bool remove(const char* path) override;
    bool rename(const char* oldPath, const char* newPath) override;
    
    bool getInfo(const char* path, FileInfo* info) override;

private:
    RamFsNode* findNode(const char* path);
    RamFsNode* findParent(const char* path, char* childName);
    RamFsNode* createNode(const char* name, FileType type, RamFsNode* parent);
    void deleteNode(RamFsNode* node);
    const char* skipMountPoint(const char* path);
    
    RamFsNode* mRoot;
    usize mNodeCount;
};

}
