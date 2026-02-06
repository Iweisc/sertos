#pragma once

#include "vfs.hpp"
#include "../process/process.hpp"

namespace sertos::fs {

constexpr u32 PROCFS_MAX_ENTRIES = 256;

enum class ProcEntryType : u8 {
    Directory = 0,
    File,
    Link
};

struct ProcEntry {
    char name[64];
    ProcEntryType type;
    u32 mode;
    
    i64 (*read)(ProcEntry* entry, void* buffer, usize size, u64 offset);
    i64 (*write)(ProcEntry* entry, const void* buffer, usize size, u64 offset);
    
    void* privateData;
    ProcEntry* parent;
    ProcEntry* children;
    ProcEntry* next;
    bool valid;
};

class ProcFS {
public:
    static void initialize();
    
    static SuperBlock* mount();
    static void umount(SuperBlock* sb);
    
    static ProcEntry* createEntry(ProcEntry* parent, const char* name, ProcEntryType type, u32 mode);
    static void removeEntry(ProcEntry* entry);
    static ProcEntry* findEntry(const char* path);
    
    static void createProcessEntries(u32 pid);
    static void removeProcessEntries(u32 pid);
    
    static bool isInitialized();

private:
    static void createRootEntries();
    static void createSelfLink();
    
    static i64 readMeminfo(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readCpuinfo(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readUptime(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readVersion(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readStat(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readLoadavg(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readMounts(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readFilesystems(ProcEntry* entry, void* buffer, usize size, u64 offset);
    
    static i64 readProcStatus(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcMaps(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcCmdline(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcEnviron(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcStat(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcStatm(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcComm(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcExe(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcCwd(ProcEntry* entry, void* buffer, usize size, u64 offset);
    static i64 readProcFdLink(ProcEntry* entry, void* buffer, usize size, u64 offset);
    
    static Inode* procLookup(Inode* dir, const char* name);
    static i32 procGetattr(Inode* inode, Stat* stat);
    static i64 procRead(File* file, void* buffer, usize size, u64 offset);
    static i32 procReaddir(File* file, Dirent* dirp, u32 count);
    static i64 procReadlink(Inode* inode, char* buffer, usize size);
    
    static ProcEntry sEntries[PROCFS_MAX_ENTRIES];
    static ProcEntry* sRoot;
    static SuperBlock sSuperBlock;
    static u32 sEntryCount;
    static bool sInitialized;
    
    static const InodeOperations sProcInodeOps;
    static const FileOperations sProcFileOps;
    static const FileOperations sProcDirOps;
    static const SuperBlockOperations sProcSbOps;
};

}
