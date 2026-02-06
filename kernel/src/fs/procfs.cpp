#include "../../include/fs/procfs.hpp"
#include "../../include/process/scheduler.hpp"
#include "../../include/memory/pmm.hpp"

namespace sertos::fs {

ProcEntry ProcFS::sEntries[PROCFS_MAX_ENTRIES];
ProcEntry* ProcFS::sRoot = nullptr;
SuperBlock ProcFS::sSuperBlock;
u32 ProcFS::sEntryCount = 0;
bool ProcFS::sInitialized = false;

const InodeOperations ProcFS::sProcInodeOps = {
    procLookup,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    procReadlink,
    procGetattr,
    nullptr
};

const FileOperations ProcFS::sProcFileOps = {
    procRead,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

const FileOperations ProcFS::sProcDirOps = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    procReaddir,
    nullptr
};

const SuperBlockOperations ProcFS::sProcSbOps = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

void ProcFS::initialize() {
    for (u32 i = 0; i < PROCFS_MAX_ENTRIES; i++) {
        sEntries[i].valid = false;
    }
    
    sEntryCount = 0;
    sRoot = nullptr;
    
    sSuperBlock.type = FileSystemType::ProcFS;
    sSuperBlock.blockSize = 4096;
    sSuperBlock.totalBlocks = 0;
    sSuperBlock.freeBlocks = 0;
    sSuperBlock.totalInodes = PROCFS_MAX_ENTRIES;
    sSuperBlock.freeInodes = PROCFS_MAX_ENTRIES;
    sSuperBlock.root = nullptr;
    sSuperBlock.sops = &sProcSbOps;
    sSuperBlock.privateData = nullptr;
    sSuperBlock.valid = true;
    
    createRootEntries();
    
    sInitialized = true;
}

SuperBlock* ProcFS::mount() {
    return &sSuperBlock;
}

void ProcFS::umount(SuperBlock* sb) {
    (void)sb;
}

ProcEntry* ProcFS::createEntry(ProcEntry* parent, const char* name, ProcEntryType type, u32 mode) {
    for (u32 i = 0; i < PROCFS_MAX_ENTRIES; i++) {
        if (!sEntries[i].valid) {
            sEntries[i].valid = true;
            sEntries[i].type = type;
            sEntries[i].mode = mode;
            sEntries[i].read = nullptr;
            sEntries[i].write = nullptr;
            sEntries[i].privateData = nullptr;
            sEntries[i].parent = parent;
            sEntries[i].children = nullptr;
            sEntries[i].next = nullptr;
            
            usize j = 0;
            while (name[j] && j < 63) {
                sEntries[i].name[j] = name[j];
                j++;
            }
            sEntries[i].name[j] = '\0';
            
            if (parent) {
                sEntries[i].next = parent->children;
                parent->children = &sEntries[i];
            }
            
            sEntryCount++;
            return &sEntries[i];
        }
    }
    
    return nullptr;
}

void ProcFS::removeEntry(ProcEntry* entry) {
    if (!entry) return;
    
    while (entry->children) {
        removeEntry(entry->children);
    }
    
    if (entry->parent) {
        ProcEntry** pp = &entry->parent->children;
        while (*pp && *pp != entry) {
            pp = &(*pp)->next;
        }
        if (*pp) {
            *pp = entry->next;
        }
    }
    
    entry->valid = false;
    sEntryCount--;
}

ProcEntry* ProcFS::findEntry(const char* path) {
    if (!path || path[0] != '/') return nullptr;
    
    if (path[1] == '\0') return sRoot;
    
    const char* p = path + 1;
    ProcEntry* current = sRoot;
    
    while (*p && current) {
        char component[64];
        usize i = 0;
        
        while (*p && *p != '/' && i < 63) {
            component[i++] = *p++;
        }
        component[i] = '\0';
        
        if (*p == '/') p++;
        
        ProcEntry* child = current->children;
        current = nullptr;
        
        while (child) {
            bool match = true;
            for (usize j = 0; component[j] || child->name[j]; j++) {
                if (component[j] != child->name[j]) {
                    match = false;
                    break;
                }
            }
            
            if (match) {
                current = child;
                break;
            }
            
            child = child->next;
        }
    }
    
    return current;
}

void ProcFS::createProcessEntries(u32 pid) {
    char pidStr[16];
    usize i = 0;
    u32 temp = pid;
    
    if (temp == 0) {
        pidStr[i++] = '0';
    } else {
        char buf[16];
        usize j = 0;
        while (temp > 0) {
            buf[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            pidStr[i++] = buf[--j];
        }
    }
    pidStr[i] = '\0';
    
    ProcEntry* procDir = createEntry(sRoot, pidStr, ProcEntryType::Directory, 0555);
    if (!procDir) return;
    
    procDir->privateData = reinterpret_cast<void*>(static_cast<usize>(pid));
    
    ProcEntry* status = createEntry(procDir, "status", ProcEntryType::File, 0444);
    if (status) status->read = readProcStatus;
    
    ProcEntry* maps = createEntry(procDir, "maps", ProcEntryType::File, 0444);
    if (maps) maps->read = readProcMaps;
    
    ProcEntry* cmdline = createEntry(procDir, "cmdline", ProcEntryType::File, 0444);
    if (cmdline) cmdline->read = readProcCmdline;
    
    ProcEntry* environ = createEntry(procDir, "environ", ProcEntryType::File, 0400);
    if (environ) environ->read = readProcEnviron;
    
    ProcEntry* stat = createEntry(procDir, "stat", ProcEntryType::File, 0444);
    if (stat) stat->read = readProcStat;
    
    ProcEntry* statm = createEntry(procDir, "statm", ProcEntryType::File, 0444);
    if (statm) statm->read = readProcStatm;
    
    ProcEntry* comm = createEntry(procDir, "comm", ProcEntryType::File, 0644);
    if (comm) comm->read = readProcComm;
    
    ProcEntry* exe = createEntry(procDir, "exe", ProcEntryType::Link, 0777);
    if (exe) exe->read = readProcExe;
    
    ProcEntry* cwd = createEntry(procDir, "cwd", ProcEntryType::Link, 0777);
    if (cwd) cwd->read = readProcCwd;
    
    createEntry(procDir, "fd", ProcEntryType::Directory, 0500);
}

void ProcFS::removeProcessEntries(u32 pid) {
    char pidStr[16];
    usize i = 0;
    u32 temp = pid;
    
    if (temp == 0) {
        pidStr[i++] = '0';
    } else {
        char buf[16];
        usize j = 0;
        while (temp > 0) {
            buf[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            pidStr[i++] = buf[--j];
        }
    }
    pidStr[i] = '\0';
    
    ProcEntry* child = sRoot->children;
    while (child) {
        bool match = true;
        for (usize j = 0; pidStr[j] || child->name[j]; j++) {
            if (pidStr[j] != child->name[j]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            removeEntry(child);
            return;
        }
        
        child = child->next;
    }
}

bool ProcFS::isInitialized() {
    return sInitialized;
}

void ProcFS::createRootEntries() {
    sRoot = createEntry(nullptr, "", ProcEntryType::Directory, 0555);
    if (!sRoot) return;
    
    ProcEntry* meminfo = createEntry(sRoot, "meminfo", ProcEntryType::File, 0444);
    if (meminfo) meminfo->read = readMeminfo;
    
    ProcEntry* cpuinfo = createEntry(sRoot, "cpuinfo", ProcEntryType::File, 0444);
    if (cpuinfo) cpuinfo->read = readCpuinfo;
    
    ProcEntry* uptime = createEntry(sRoot, "uptime", ProcEntryType::File, 0444);
    if (uptime) uptime->read = readUptime;
    
    ProcEntry* version = createEntry(sRoot, "version", ProcEntryType::File, 0444);
    if (version) version->read = readVersion;
    
    ProcEntry* stat = createEntry(sRoot, "stat", ProcEntryType::File, 0444);
    if (stat) stat->read = readStat;
    
    ProcEntry* loadavg = createEntry(sRoot, "loadavg", ProcEntryType::File, 0444);
    if (loadavg) loadavg->read = readLoadavg;
    
    ProcEntry* mounts = createEntry(sRoot, "mounts", ProcEntryType::File, 0444);
    if (mounts) mounts->read = readMounts;
    
    ProcEntry* filesystems = createEntry(sRoot, "filesystems", ProcEntryType::File, 0444);
    if (filesystems) filesystems->read = readFilesystems;
    
    createSelfLink();
}

void ProcFS::createSelfLink() {
    createEntry(sRoot, "self", ProcEntryType::Link, 0777);
}

i64 ProcFS::readMeminfo(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    char data[512];
    usize len = 0;
    
    u64 totalMem = memory::PMM::totalMemory();
    u64 freeMem = memory::PMM::freeMemory();
    u64 usedMem = totalMem - freeMem;
    
    const char* header = "MemTotal:       ";
    for (usize i = 0; header[i]; i++) data[len++] = header[i];
    
    char numBuf[32];
    usize numLen = 0;
    u64 kb = totalMem / 1024;
    if (kb == 0) {
        numBuf[numLen++] = '0';
    } else {
        char temp[32];
        usize tempLen = 0;
        while (kb > 0) {
            temp[tempLen++] = '0' + (kb % 10);
            kb /= 10;
        }
        while (tempLen > 0) {
            numBuf[numLen++] = temp[--tempLen];
        }
    }
    for (usize i = 0; i < numLen; i++) data[len++] = numBuf[i];
    
    const char* suffix = " kB\nMemFree:        ";
    for (usize i = 0; suffix[i]; i++) data[len++] = suffix[i];
    
    numLen = 0;
    kb = freeMem / 1024;
    if (kb == 0) {
        numBuf[numLen++] = '0';
    } else {
        char temp[32];
        usize tempLen = 0;
        while (kb > 0) {
            temp[tempLen++] = '0' + (kb % 10);
            kb /= 10;
        }
        while (tempLen > 0) {
            numBuf[numLen++] = temp[--tempLen];
        }
    }
    for (usize i = 0; i < numLen; i++) data[len++] = numBuf[i];
    
    const char* suffix2 = " kB\nMemUsed:        ";
    for (usize i = 0; suffix2[i]; i++) data[len++] = suffix2[i];
    
    numLen = 0;
    kb = usedMem / 1024;
    if (kb == 0) {
        numBuf[numLen++] = '0';
    } else {
        char temp[32];
        usize tempLen = 0;
        while (kb > 0) {
            temp[tempLen++] = '0' + (kb % 10);
            kb /= 10;
        }
        while (tempLen > 0) {
            numBuf[numLen++] = temp[--tempLen];
        }
    }
    for (usize i = 0; i < numLen; i++) data[len++] = numBuf[i];
    
    const char* end = " kB\n";
    for (usize i = 0; end[i]; i++) data[len++] = end[i];
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readCpuinfo(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    const char* data = "processor\t: 0\nvendor_id\t: SertOS\nmodel name\t: SertOS Virtual CPU\ncpu MHz\t\t: 1000.000\n";
    
    usize len = 0;
    while (data[len]) len++;
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readUptime(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    u64 uptime = process::Scheduler::systemTime() / 1000;
    
    char data[64];
    usize len = 0;
    
    char numBuf[32];
    usize numLen = 0;
    if (uptime == 0) {
        numBuf[numLen++] = '0';
    } else {
        char temp[32];
        usize tempLen = 0;
        while (uptime > 0) {
            temp[tempLen++] = '0' + (uptime % 10);
            uptime /= 10;
        }
        while (tempLen > 0) {
            numBuf[numLen++] = temp[--tempLen];
        }
    }
    
    for (usize i = 0; i < numLen; i++) data[len++] = numBuf[i];
    data[len++] = '.';
    data[len++] = '0';
    data[len++] = '0';
    data[len++] = ' ';
    data[len++] = '0';
    data[len++] = '.';
    data[len++] = '0';
    data[len++] = '0';
    data[len++] = '\n';
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readVersion(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    const char* data = "SertOS version 1.0.0 (sertos@sertos) (gcc 12.0.0) #1 SMP\n";
    
    usize len = 0;
    while (data[len]) len++;
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readStat(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    const char* data = "cpu  0 0 0 0 0 0 0 0 0 0\ncpu0 0 0 0 0 0 0 0 0 0 0\n";
    
    usize len = 0;
    while (data[len]) len++;
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readLoadavg(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    const char* data = "0.00 0.00 0.00 1/1 1\n";
    
    usize len = 0;
    while (data[len]) len++;
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readMounts(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    const char* data = "sertfs / sertfs rw 0 0\nproc /proc proc rw 0 0\ndevfs /dev devfs rw 0 0\n";
    
    usize len = 0;
    while (data[len]) len++;
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readFilesystems(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    const char* data = "\tsertfs\n\tprocfs\n\tdevfs\n";
    
    usize len = 0;
    while (data[len]) len++;
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readProcStatus(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    if (!entry || !entry->parent) return -1;
    
    u32 pid = static_cast<u32>(reinterpret_cast<usize>(entry->parent->privateData));
    process::Process* proc = process::PM::getProcess(pid);
    if (!proc) return -1;
    
    char data[256];
    usize len = 0;
    
    const char* name = "Name:\t";
    for (usize i = 0; name[i]; i++) data[len++] = name[i];
    for (usize i = 0; proc->name[i] && i < 32; i++) data[len++] = proc->name[i];
    data[len++] = '\n';
    
    const char* state = "State:\tR (running)\n";
    for (usize i = 0; state[i]; i++) data[len++] = state[i];
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readProcMaps(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    (void)buffer;
    (void)size;
    (void)offset;
    return 0;
}

i64 ProcFS::readProcCmdline(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    if (!entry || !entry->parent) return -1;
    
    u32 pid = static_cast<u32>(reinterpret_cast<usize>(entry->parent->privateData));
    process::Process* proc = process::PM::getProcess(pid);
    if (!proc) return -1;
    
    usize len = 0;
    while (proc->name[len]) len++;
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = proc->name[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readProcEnviron(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    (void)buffer;
    (void)size;
    (void)offset;
    return 0;
}

i64 ProcFS::readProcStat(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    if (!entry || !entry->parent) return -1;
    
    u32 pid = static_cast<u32>(reinterpret_cast<usize>(entry->parent->privateData));
    
    char data[128];
    usize len = 0;
    
    char numBuf[16];
    usize numLen = 0;
    u32 temp = pid;
    if (temp == 0) {
        numBuf[numLen++] = '0';
    } else {
        char buf[16];
        usize j = 0;
        while (temp > 0) {
            buf[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            numBuf[numLen++] = buf[--j];
        }
    }
    
    for (usize i = 0; i < numLen; i++) data[len++] = numBuf[i];
    
    const char* rest = " (init) R 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
    for (usize i = 0; rest[i]; i++) data[len++] = rest[i];
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readProcStatm(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    
    const char* data = "0 0 0 0 0 0 0\n";
    
    usize len = 0;
    while (data[len]) len++;
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = data[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readProcComm(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    return readProcCmdline(entry, buffer, size, offset);
}

i64 ProcFS::readProcExe(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    return readProcCmdline(entry, buffer, size, offset);
}

i64 ProcFS::readProcCwd(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    if (!entry || !entry->parent) return -1;
    
    u32 pid = static_cast<u32>(reinterpret_cast<usize>(entry->parent->privateData));
    process::Process* proc = process::PM::getProcess(pid);
    if (!proc) return -1;
    
    usize len = 0;
    while (proc->cwd[len]) len++;
    
    if (len == 0) {
        if (size > 0) {
            static_cast<char*>(buffer)[0] = '/';
            return 1;
        }
        return 0;
    }
    
    if (offset >= len) return 0;
    
    usize toRead = (size < len - offset) ? size : len - offset;
    char* dest = static_cast<char*>(buffer);
    for (usize i = 0; i < toRead; i++) {
        dest[i] = proc->cwd[offset + i];
    }
    
    return static_cast<i64>(toRead);
}

i64 ProcFS::readProcFdLink(ProcEntry* entry, void* buffer, usize size, u64 offset) {
    (void)entry;
    (void)buffer;
    (void)size;
    (void)offset;
    return 0;
}

Inode* ProcFS::procLookup(Inode* dir, const char* name) {
    (void)dir;
    (void)name;
    return nullptr;
}

i32 ProcFS::procGetattr(Inode* inode, Stat* stat) {
    if (!inode || !stat) return -1;
    
    stat->dev = 0;
    stat->ino = inode->ino;
    stat->mode = inode->mode;
    stat->nlink = inode->nlink;
    stat->uid = inode->uid;
    stat->gid = inode->gid;
    stat->rdev = 0;
    stat->size = static_cast<i64>(inode->size);
    stat->blksize = 4096;
    stat->blocks = 0;
    stat->atime = inode->atime;
    stat->mtime = inode->mtime;
    stat->ctime = inode->ctime;
    
    return 0;
}

i64 ProcFS::procRead(File* file, void* buffer, usize size, u64 offset) {
    if (!file || !file->privateData) return -1;
    
    ProcEntry* entry = static_cast<ProcEntry*>(file->privateData);
    if (!entry->read) return 0;
    
    return entry->read(entry, buffer, size, offset);
}

i32 ProcFS::procReaddir(File* file, Dirent* dirp, u32 count) {
    (void)file;
    (void)dirp;
    (void)count;
    return 0;
}

i64 ProcFS::procReadlink(Inode* inode, char* buffer, usize size) {
    (void)inode;
    (void)buffer;
    (void)size;
    return -1;
}

}
