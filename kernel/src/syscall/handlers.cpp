#include "../../include/syscall/syscall.hpp"
#include "../../include/process/process.hpp"
#include "../../include/process/scheduler.hpp"
#include "../../include/memory/vmm.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/graphics/console.hpp"
#include "../../include/ipc/ipc.hpp"
#include "../../include/user/user.hpp"
#include "../../include/security/security.hpp"
#include "../../include/power/acpi.hpp"
#include "../../include/fs/sertfs.hpp"
#include "../../include/loader/elf.hpp"

namespace sertos::syscall {

using sertos::align_up;

namespace {

void strcpy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

usize strlen(const char* s) {
    usize len = 0;
    while (s[len]) len++;
    return len;
}

i32 allocateFd(process::Process* proc) {
    for (u32 i = 3; i < process::MAX_FDS; i++) {
        if (!proc->fds[i].valid) {
            return static_cast<i32>(i);
        }
    }
    return -1;
}

void initStdFds(process::Process* proc) {
    proc->fds[STDIN_FD].type = process::FdType::Console;
    proc->fds[STDIN_FD].flags = fs::O_READ;
    proc->fds[STDIN_FD].offset = 0;
    proc->fds[STDIN_FD].data = nullptr;
    proc->fds[STDIN_FD].valid = true;
    
    proc->fds[STDOUT_FD].type = process::FdType::Console;
    proc->fds[STDOUT_FD].flags = fs::O_WRITE;
    proc->fds[STDOUT_FD].offset = 0;
    proc->fds[STDOUT_FD].data = nullptr;
    proc->fds[STDOUT_FD].valid = true;
    
    proc->fds[STDERR_FD].type = process::FdType::Console;
    proc->fds[STDERR_FD].flags = fs::O_WRITE;
    proc->fds[STDERR_FD].offset = 0;
    proc->fds[STDERR_FD].data = nullptr;
    proc->fds[STDERR_FD].valid = true;
}

i64 sysExit(u64 status, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (current) {
        for (u32 i = 0; i < process::MAX_FDS; i++) {
            if (current->fds[i].valid && current->fds[i].type == process::FdType::File) {
                fs::FileHandle* handle = static_cast<fs::FileHandle*>(current->fds[i].data);
                if (handle) {
                    fs::SertFs::close(handle);
                }
            }
            current->fds[i].valid = false;
        }
        process::PM::terminateProcess(current, static_cast<i32>(status));
        process::Scheduler::schedule();
    }
    return 0;
}

i64 sysWrite(u64 fd, u64 bufAddr, u64 count, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return EFAULT;
    }
    
    if (fd >= process::MAX_FDS || !current->fds[fd].valid) {
        return EBADF;
    }
    
    if (!memory::VMM::isUserAddress(bufAddr)) {
        return EFAULT;
    }
    
    const char* buf = reinterpret_cast<const char*>(bufAddr);
    process::FileDescriptor& fdesc = current->fds[fd];
    
    if (fdesc.type == process::FdType::Console) {
        for (u64 i = 0; i < count; i++) {
            graphics::Console::putChar(buf[i]);
        }
        return static_cast<i64>(count);
    }
    
    if (fdesc.type == process::FdType::File) {
        if (!(fdesc.flags & fs::O_WRITE)) {
            return EBADF;
        }
        
        fs::FileHandle* handle = static_cast<fs::FileHandle*>(fdesc.data);
        if (!handle || !handle->valid) {
            return EBADF;
        }
        
        i64 written = fs::SertFs::write(handle, buf, count);
        if (written > 0) {
            fdesc.offset += static_cast<u64>(written);
        }
        return written;
    }
    
    if (fdesc.type == process::FdType::Pipe) {
        return ipc::PipeManager::write(static_cast<i32>(fd), buf, count);
    }
    
    return EBADF;
}

i64 sysRead(u64 fd, u64 bufAddr, u64 count, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return EFAULT;
    }
    
    if (fd >= process::MAX_FDS || !current->fds[fd].valid) {
        return EBADF;
    }
    
    if (!memory::VMM::isUserAddress(bufAddr)) {
        return EFAULT;
    }
    
    char* buf = reinterpret_cast<char*>(bufAddr);
    process::FileDescriptor& fdesc = current->fds[fd];
    
    if (fdesc.type == process::FdType::Console) {
        return 0;
    }
    
    if (fdesc.type == process::FdType::File) {
        if (!(fdesc.flags & fs::O_READ)) {
            return EBADF;
        }
        
        fs::FileHandle* handle = static_cast<fs::FileHandle*>(fdesc.data);
        if (!handle || !handle->valid) {
            return EBADF;
        }
        
        i64 bytesRead = fs::SertFs::read(handle, buf, count);
        if (bytesRead > 0) {
            fdesc.offset += static_cast<u64>(bytesRead);
        }
        return bytesRead;
    }
    
    if (fdesc.type == process::FdType::Pipe) {
        return ipc::PipeManager::read(static_cast<i32>(fd), buf, count);
    }
    
    return EBADF;
}

i64 sysOpen(u64 pathAddr, u64 flags, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return EFAULT;
    }
    
    if (!memory::VMM::isUserAddress(pathAddr)) {
        return EFAULT;
    }
    
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    i32 fdNum = allocateFd(current);
    if (fdNum < 0) {
        return EMFILE;
    }
    
    u32 fsFlags = 0;
    if (flags & 0x0) fsFlags |= fs::O_READ;
    if (flags & 0x1) fsFlags |= fs::O_WRITE;
    if (flags & 0x2) fsFlags |= fs::O_READ | fs::O_WRITE;
    if (flags & 0x40) fsFlags |= fs::O_CREATE;
    if (flags & 0x200) fsFlags |= fs::O_TRUNCATE;
    if (flags & 0x400) fsFlags |= fs::O_APPEND;
    
    if (fsFlags == 0) fsFlags = fs::O_READ;
    
    fs::FileHandle handle = fs::SertFs::open(path, fsFlags);
    if (!handle.valid) {
        if (fsFlags & fs::O_CREATE) {
            if (!fs::SertFs::createFile(path)) {
                return ENOENT;
            }
            handle = fs::SertFs::open(path, fsFlags);
            if (!handle.valid) {
                return ENOENT;
            }
        } else {
            return ENOENT;
        }
    }
    
    fs::FileHandle* handlePtr = new fs::FileHandle;
    *handlePtr = handle;
    
    current->fds[fdNum].type = process::FdType::File;
    current->fds[fdNum].flags = fsFlags;
    current->fds[fdNum].offset = 0;
    current->fds[fdNum].data = handlePtr;
    current->fds[fdNum].valid = true;
    
    return fdNum;
}

i64 sysClose(u64 fd, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return EFAULT;
    }
    
    if (fd >= process::MAX_FDS || !current->fds[fd].valid) {
        return EBADF;
    }
    
    process::FileDescriptor& fdesc = current->fds[fd];
    
    if (fdesc.type == process::FdType::File) {
        fs::FileHandle* handle = static_cast<fs::FileHandle*>(fdesc.data);
        if (handle) {
            fs::SertFs::close(handle);
            delete handle;
        }
    } else if (fdesc.type == process::FdType::Pipe) {
        ipc::PipeManager::close(static_cast<i32>(fd));
    }
    
    fdesc.valid = false;
    fdesc.data = nullptr;
    
    return 0;
}

i64 sysMmap(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysMunmap(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysBrk(u64 addr, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return EFAULT;
    }
    
    if (addr == 0) {
        return static_cast<i64>(current->programBreak);
    }
    
    if (addr < current->heapStart) {
        return EINVAL;
    }
    
    u64 currentBreak = current->programBreak;
    u64 newBreak = align_up(addr, static_cast<u64>(memory::PAGE_SIZE));
    
    if (newBreak > currentBreak) {
        u64 pagesToAlloc = (newBreak - currentBreak) / memory::PAGE_SIZE;
        
        for (u64 i = 0; i < pagesToAlloc; i++) {
            u64 pageAddr = currentBreak + i * memory::PAGE_SIZE;
            void* physPage = memory::PMM::allocatePage();
            
            if (!physPage) {
                return ENOMEM;
            }
            
            u8* pagePtr = reinterpret_cast<u8*>(physPage);
            for (usize j = 0; j < memory::PAGE_SIZE; j++) {
                pagePtr[j] = 0;
            }
            
            if (!memory::VMM::mapPageIn(current->pageTable, pageAddr, 
                    reinterpret_cast<u64>(physPage),
                    memory::PAGE_PRESENT | memory::PAGE_WRITABLE | memory::PAGE_USER)) {
                memory::PMM::freePage(physPage);
                return ENOMEM;
            }
        }
    } else if (newBreak < currentBreak) {
        u64 pagesToFree = (currentBreak - newBreak) / memory::PAGE_SIZE;
        
        for (u64 i = 0; i < pagesToFree; i++) {
            u64 pageAddr = newBreak + i * memory::PAGE_SIZE;
            u64 physAddr = memory::VMM::getPhysicalAddressIn(current->pageTable, pageAddr);
            
            if (physAddr) {
                memory::PMM::freePage(reinterpret_cast<void*>(physAddr));
                memory::VMM::unmapPageIn(current->pageTable, pageAddr);
            }
        }
    }
    
    current->programBreak = newBreak;
    current->heapEnd = newBreak;
    
    return static_cast<i64>(newBreak);
}

i64 sysGetpid(u64, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return -1;
    }
    return static_cast<i64>(current->pid);
}

i64 sysFork(u64, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return -1;
    }
    
    process::Process* child = process::PM::forkProcess(current);
    if (!child) {
        return ENOMEM;
    }
    
    process::Scheduler::addProcess(child);
    
    return static_cast<i64>(child->pid);
}

i64 sysExec(u64 pathAddr, u64 argvAddr, u64 envpAddr, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return EFAULT;
    }
    
    if (!memory::VMM::isUserAddress(pathAddr)) {
        return EFAULT;
    }
    
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (!fs::SertFs::exists(path)) {
        return ENOENT;
    }
    
    if (!fs::SertFs::isFile(path)) {
        return ENOEXEC;
    }
    
    fs::FileInfo info;
    if (!fs::SertFs::getInfo(path, &info)) {
        return ENOENT;
    }
    
    if (info.size == 0 || info.size > 16 * 1024 * 1024) {
        return ENOEXEC;
    }
    
    u8* fileData = new u8[info.size];
    if (!fileData) {
        return ENOMEM;
    }
    
    fs::FileHandle handle = fs::SertFs::open(path, fs::O_READ);
    if (!handle.valid) {
        delete[] fileData;
        return ENOENT;
    }
    
    i64 bytesRead = fs::SertFs::read(&handle, fileData, info.size);
    fs::SertFs::close(&handle);
    
    if (bytesRead != static_cast<i64>(info.size)) {
        delete[] fileData;
        return EIO;
    }
    
    if (!loader::ElfLoader::validate(fileData, info.size)) {
        delete[] fileData;
        return ENOEXEC;
    }
    
    for (u32 i = 3; i < process::MAX_FDS; i++) {
        if (current->fds[i].valid && current->fds[i].type == process::FdType::File) {
            fs::FileHandle* fh = static_cast<fs::FileHandle*>(current->fds[i].data);
            if (fh) {
                fs::SertFs::close(fh);
                delete fh;
            }
            current->fds[i].valid = false;
        }
    }
    
    loader::ElfLoadResult result = loader::ElfLoader::load(current, fileData, info.size);
    delete[] fileData;
    
    if (result.error != loader::ElfError::Success) {
        return ENOEXEC;
    }
    
    strcpy(current->name, path, process::PROCESS_NAME_MAX);
    
    current->context.rip = result.entryPoint;
    current->context.rsp = current->userStack;
    current->context.rflags = 0x202;
    current->context.cs = 0x23;
    current->context.ss = 0x1b;
    
    current->heapStart = align_up(result.endAddress, static_cast<u64>(memory::PAGE_SIZE));
    current->heapEnd = current->heapStart;
    current->programBreak = current->heapStart;
    
    initStdFds(current);
    
    (void)argvAddr;
    (void)envpAddr;
    
    return 0;
}

i64 sysWait(u64 statusPtr, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return -1;
    }
    
    for (u32 i = 0; i < process::MAX_PROCESSES; i++) {
        process::Process* child = process::PM::getProcess(i);
        if (child && child->parentPid == current->pid && 
            child->state == process::ProcessState::Zombie) {
            
            if (statusPtr && memory::VMM::isUserAddress(statusPtr)) {
                *reinterpret_cast<i32*>(statusPtr) = child->exitCode;
            }
            
            u32 childPid = child->pid;
            process::PM::destroyProcess(child);
            
            return static_cast<i64>(childPid);
        }
    }
    
    return ECHILD;
}

i64 sysYield(u64, u64, u64, u64, u64, u64) {
    process::Scheduler::yield();
    return 0;
}

i64 sysSleep(u64 milliseconds, u64, u64, u64, u64, u64) {
    process::Scheduler::sleep(milliseconds);
    return 0;
}

i64 sysGettime(u64, u64, u64, u64, u64, u64) {
    return static_cast<i64>(process::Scheduler::systemTime());
}

i64 sysGetuid(u64, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return -1;
    return static_cast<i64>(current->uid);
}

i64 sysGetgid(u64, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return -1;
    return static_cast<i64>(current->gid);
}

i64 sysSetuid(u64 uid, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (current->euid != 0 && current->uid != static_cast<u32>(uid)) {
        return EPERM;
    }
    
    current->uid = static_cast<u32>(uid);
    current->euid = static_cast<u32>(uid);
    return 0;
}

i64 sysSetgid(u64 gid, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (current->euid != 0 && current->gid != static_cast<u32>(gid)) {
        return EPERM;
    }
    
    current->gid = static_cast<u32>(gid);
    current->egid = static_cast<u32>(gid);
    return 0;
}

i64 sysGeteuid(u64, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return -1;
    return static_cast<i64>(current->euid);
}

i64 sysGetegid(u64, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return -1;
    return static_cast<i64>(current->egid);
}

i64 sysSeteuid(u64 euid, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (current->euid != 0 && current->uid != static_cast<u32>(euid) &&
        current->euid != static_cast<u32>(euid)) {
        return EPERM;
    }
    
    current->euid = static_cast<u32>(euid);
    return 0;
}

i64 sysSetegid(u64 egid, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (current->euid != 0 && current->gid != static_cast<u32>(egid) &&
        current->egid != static_cast<u32>(egid)) {
        return EPERM;
    }
    
    current->egid = static_cast<u32>(egid);
    return 0;
}

i64 sysPipe(u64 pipefdAddr, u64, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(pipefdAddr)) {
        return EFAULT;
    }
    
    i32* pipefd = reinterpret_cast<i32*>(pipefdAddr);
    if (!ipc::PipeManager::createPipe(&pipefd[0], &pipefd[1])) {
        return ENOMEM;
    }
    
    return 0;
}

i64 sysDup(u64 oldfd, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (oldfd >= process::MAX_FDS || !current->fds[oldfd].valid) {
        return EBADF;
    }
    
    i32 newfd = allocateFd(current);
    if (newfd < 0) {
        return EMFILE;
    }
    
    current->fds[newfd] = current->fds[oldfd];
    
    if (current->fds[oldfd].type == process::FdType::File && current->fds[oldfd].data) {
        fs::FileHandle* oldHandle = static_cast<fs::FileHandle*>(current->fds[oldfd].data);
        fs::FileHandle* newHandle = new fs::FileHandle;
        *newHandle = *oldHandle;
        current->fds[newfd].data = newHandle;
    }
    
    return newfd;
}

i64 sysDup2(u64 oldfd, u64 newfd, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (oldfd >= process::MAX_FDS || !current->fds[oldfd].valid) {
        return EBADF;
    }
    
    if (newfd >= process::MAX_FDS) {
        return EBADF;
    }
    
    if (oldfd == newfd) {
        return static_cast<i64>(newfd);
    }
    
    if (current->fds[newfd].valid) {
        if (current->fds[newfd].type == process::FdType::File && current->fds[newfd].data) {
            fs::FileHandle* handle = static_cast<fs::FileHandle*>(current->fds[newfd].data);
            fs::SertFs::close(handle);
            delete handle;
        }
        current->fds[newfd].valid = false;
    }
    
    current->fds[newfd] = current->fds[oldfd];
    
    if (current->fds[oldfd].type == process::FdType::File && current->fds[oldfd].data) {
        fs::FileHandle* oldHandle = static_cast<fs::FileHandle*>(current->fds[oldfd].data);
        fs::FileHandle* newHandle = new fs::FileHandle;
        *newHandle = *oldHandle;
        current->fds[newfd].data = newHandle;
    }
    
    return static_cast<i64>(newfd);
}

i64 sysLseek(u64 fd, u64 offset, u64 whence, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (fd >= process::MAX_FDS || !current->fds[fd].valid) {
        return EBADF;
    }
    
    process::FileDescriptor& fdesc = current->fds[fd];
    
    if (fdesc.type != process::FdType::File) {
        return ESPIPE;
    }
    
    fs::FileHandle* handle = static_cast<fs::FileHandle*>(fdesc.data);
    if (!handle || !handle->valid) {
        return EBADF;
    }
    
    fs::SeekMode mode;
    switch (whence) {
        case 0: mode = fs::SeekMode::Set; break;
        case 1: mode = fs::SeekMode::Current; break;
        case 2: mode = fs::SeekMode::End; break;
        default: return EINVAL;
    }
    
    i64 newPos = fs::SertFs::seek(handle, static_cast<i64>(offset), mode);
    if (newPos >= 0) {
        fdesc.offset = static_cast<u64>(newPos);
    }
    
    return newPos;
}

struct StatBuf {
    u64 st_dev;
    u64 st_ino;
    u32 st_mode;
    u32 st_nlink;
    u32 st_uid;
    u32 st_gid;
    u64 st_rdev;
    u64 st_size;
    u64 st_blksize;
    u64 st_blocks;
    u64 st_atime;
    u64 st_mtime;
    u64 st_ctime;
};

i64 sysStat(u64 pathAddr, u64 statbufAddr, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(pathAddr) || !memory::VMM::isUserAddress(statbufAddr)) {
        return EFAULT;
    }
    
    const char* path = reinterpret_cast<const char*>(pathAddr);
    StatBuf* statbuf = reinterpret_cast<StatBuf*>(statbufAddr);
    
    fs::FileInfo info;
    if (!fs::SertFs::getInfo(path, &info)) {
        return ENOENT;
    }
    
    statbuf->st_dev = 0;
    statbuf->st_ino = 0;
    statbuf->st_mode = info.permissions;
    if (info.type == fs::FileType::Directory) {
        statbuf->st_mode |= 0040000;
    } else if (info.type == fs::FileType::Regular) {
        statbuf->st_mode |= 0100000;
    }
    statbuf->st_nlink = 1;
    statbuf->st_uid = info.uid;
    statbuf->st_gid = info.gid;
    statbuf->st_rdev = 0;
    statbuf->st_size = info.size;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = (info.size + 511) / 512;
    statbuf->st_atime = info.accessedTime;
    statbuf->st_mtime = info.modifiedTime;
    statbuf->st_ctime = info.createdTime;
    
    return 0;
}

i64 sysFstat(u64 fd, u64 statbufAddr, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (!memory::VMM::isUserAddress(statbufAddr)) {
        return EFAULT;
    }
    
    if (fd >= process::MAX_FDS || !current->fds[fd].valid) {
        return EBADF;
    }
    
    StatBuf* statbuf = reinterpret_cast<StatBuf*>(statbufAddr);
    process::FileDescriptor& fdesc = current->fds[fd];
    
    if (fdesc.type == process::FdType::Console) {
        statbuf->st_dev = 0;
        statbuf->st_ino = 0;
        statbuf->st_mode = 0020666;
        statbuf->st_nlink = 1;
        statbuf->st_uid = 0;
        statbuf->st_gid = 0;
        statbuf->st_rdev = 0;
        statbuf->st_size = 0;
        statbuf->st_blksize = 1;
        statbuf->st_blocks = 0;
        statbuf->st_atime = 0;
        statbuf->st_mtime = 0;
        statbuf->st_ctime = 0;
        return 0;
    }
    
    if (fdesc.type == process::FdType::Pipe) {
        statbuf->st_dev = 0;
        statbuf->st_ino = 0;
        statbuf->st_mode = 0010666;
        statbuf->st_nlink = 1;
        statbuf->st_uid = 0;
        statbuf->st_gid = 0;
        statbuf->st_rdev = 0;
        statbuf->st_size = 0;
        statbuf->st_blksize = 4096;
        statbuf->st_blocks = 0;
        statbuf->st_atime = 0;
        statbuf->st_mtime = 0;
        statbuf->st_ctime = 0;
        return 0;
    }
    
    return EBADF;
}

i64 sysMkdir(u64 pathAddr, u64, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(pathAddr)) {
        return EFAULT;
    }
    
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (fs::SertFs::exists(path)) {
        return EEXIST;
    }
    
    if (!fs::SertFs::createDirectory(path)) {
        return EIO;
    }
    
    return 0;
}

i64 sysRmdir(u64 pathAddr, u64, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(pathAddr)) {
        return EFAULT;
    }
    
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (!fs::SertFs::exists(path)) {
        return ENOENT;
    }
    
    if (!fs::SertFs::isDirectory(path)) {
        return ENOTDIR;
    }
    
    if (!fs::SertFs::remove(path)) {
        return ENOTEMPTY;
    }
    
    return 0;
}

i64 sysUnlink(u64 pathAddr, u64, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(pathAddr)) {
        return EFAULT;
    }
    
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (!fs::SertFs::exists(path)) {
        return ENOENT;
    }
    
    if (fs::SertFs::isDirectory(path)) {
        return EISDIR;
    }
    
    if (!fs::SertFs::remove(path)) {
        return EIO;
    }
    
    return 0;
}

i64 sysRename(u64 oldpathAddr, u64 newpathAddr, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(oldpathAddr) || !memory::VMM::isUserAddress(newpathAddr)) {
        return EFAULT;
    }
    
    const char* oldpath = reinterpret_cast<const char*>(oldpathAddr);
    const char* newpath = reinterpret_cast<const char*>(newpathAddr);
    
    if (!fs::SertFs::exists(oldpath)) {
        return ENOENT;
    }
    
    if (!fs::SertFs::rename(oldpath, newpath)) {
        return EIO;
    }
    
    return 0;
}

i64 sysChdir(u64 pathAddr, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (!memory::VMM::isUserAddress(pathAddr)) {
        return EFAULT;
    }
    
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (!fs::SertFs::exists(path)) {
        return ENOENT;
    }
    
    if (!fs::SertFs::isDirectory(path)) {
        return ENOTDIR;
    }
    
    strcpy(current->cwd, path, process::MAX_CWD_LEN);
    
    return 0;
}

i64 sysGetcwd(u64 bufAddr, u64 size, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (!memory::VMM::isUserAddress(bufAddr)) {
        return EFAULT;
    }
    
    char* buf = reinterpret_cast<char*>(bufAddr);
    
    if (current->cwd[0] == '\0') {
        strcpy(buf, "/", size);
    } else {
        strcpy(buf, current->cwd, size);
    }
    
    return static_cast<i64>(bufAddr);
}

i64 sysKill(u64 pid, u64 sig, u64, u64, u64, u64) {
    process::Process* target = process::PM::getProcess(static_cast<u32>(pid));
    if (!target) {
        return ESRCH;
    }
    
    ipc::SignalManager::send(static_cast<u32>(pid), static_cast<i32>(sig));
    return 0;
}

i64 sysSignal(u64 signum, u64 handler, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (signum >= 32) return EINVAL;
    
    current->signalHandlers[signum] = handler;
    return 0;
}

i64 sysSigaction(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysSigprocmask(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysSigsuspend(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysShmget(u64 key, u64 size, u64 flags, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    i32 shmId = ipc::SharedMemoryManager::create(
        static_cast<u32>(key), size, static_cast<u32>(flags));
    if (shmId < 0) {
        return ENOMEM;
    }
    
    return static_cast<i64>(shmId);
}

i64 sysShmat(u64 shmid, u64 shmaddr, u64, u64, u64, u64) {
    void* addr = ipc::SharedMemoryManager::attach(
        static_cast<i32>(shmid), shmaddr);
    
    if (!addr) {
        return EINVAL;
    }
    
    return reinterpret_cast<i64>(addr);
}

i64 sysShmdt(u64 shmaddr, u64, u64, u64, u64, u64) {
    if (!ipc::SharedMemoryManager::detach(reinterpret_cast<void*>(shmaddr))) {
        return EINVAL;
    }
    
    return 0;
}

i64 sysShmctl(u64 shmid, u64 cmd, u64, u64, u64, u64) {
    if (cmd == 0) {
        if (!ipc::SharedMemoryManager::remove(static_cast<i32>(shmid))) {
            return EINVAL;
        }
    }
    return 0;
}

i64 sysMsgget(u64 key, u64 flags, u64, u64, u64, u64) {
    i32 mqId = ipc::MessageQueueManager::create(
        static_cast<u32>(key), static_cast<u32>(flags));
    if (mqId < 0) {
        return ENOMEM;
    }
    
    return static_cast<i64>(mqId);
}

i64 sysMsgsnd(u64 msqid, u64 msgpAddr, u64 msgsz, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(msgpAddr)) {
        return EFAULT;
    }
    
    const void* msgp = reinterpret_cast<const void*>(msgpAddr);
    if (!ipc::MessageQueueManager::send(static_cast<u32>(msqid), msgp, msgsz, 0)) {
        return EINVAL;
    }
    
    return 0;
}

i64 sysMsgrcv(u64 msqid, u64 msgpAddr, u64 msgsz, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(msgpAddr)) {
        return EFAULT;
    }
    
    void* msgp = reinterpret_cast<void*>(msgpAddr);
    usize received = ipc::MessageQueueManager::receive(
        static_cast<u32>(msqid), msgp, msgsz, 0);
    
    return static_cast<i64>(received);
}

i64 sysMsgctl(u64 msqid, u64 cmd, u64, u64, u64, u64) {
    if (cmd == 0) {
        if (!ipc::MessageQueueManager::remove(static_cast<i32>(msqid))) {
            return EINVAL;
        }
    }
    return 0;
}

i64 sysSocket(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysBind(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysListen(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysAccept(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysConnect(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysSend(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysRecv(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysShutdown(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysMprotect(u64 addr, u64 len, u64 prot, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (!memory::VMM::isUserAddress(addr)) {
        return EINVAL;
    }
    
    u64 flags = memory::PAGE_PRESENT | memory::PAGE_USER;
    if (prot & 0x2) flags |= memory::PAGE_WRITABLE;
    if (!(prot & 0x4)) flags |= memory::PAGE_NO_EXECUTE;
    
    u64 pages = (len + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
    for (u64 i = 0; i < pages; i++) {
        u64 pageAddr = addr + i * memory::PAGE_SIZE;
        u64 physAddr = memory::VMM::getPhysicalAddressIn(current->pageTable, pageAddr);
        if (physAddr) {
            memory::VMM::mapPageIn(current->pageTable, pageAddr, physAddr, flags);
        }
    }
    
    return 0;
}

i64 sysGetppid(u64, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return -1;
    return static_cast<i64>(current->parentPid);
}

i64 sysGetpgid(u64 pid, u64, u64, u64, u64, u64) {
    process::Process* proc;
    if (pid == 0) {
        proc = process::PM::currentProcess();
    } else {
        proc = process::PM::getProcess(static_cast<u32>(pid));
    }
    
    if (!proc) return ESRCH;
    return static_cast<i64>(proc->pgid);
}

i64 sysSetpgid(u64 pid, u64 pgid, u64, u64, u64, u64) {
    process::Process* proc;
    if (pid == 0) {
        proc = process::PM::currentProcess();
    } else {
        proc = process::PM::getProcess(static_cast<u32>(pid));
    }
    
    if (!proc) return ESRCH;
    
    if (pgid == 0) {
        proc->pgid = proc->pid;
    } else {
        proc->pgid = static_cast<u32>(pgid);
    }
    
    return 0;
}

i64 sysSetsid(u64, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    current->sid = current->pid;
    current->pgid = current->pid;
    
    return static_cast<i64>(current->sid);
}

i64 sysGetsid(u64 pid, u64, u64, u64, u64, u64) {
    process::Process* proc;
    if (pid == 0) {
        proc = process::PM::currentProcess();
    } else {
        proc = process::PM::getProcess(static_cast<u32>(pid));
    }
    
    if (!proc) return ESRCH;
    return static_cast<i64>(proc->sid);
}

i64 sysIoctl(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysFcntl(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysClockGettime(u64, u64 tpAddr, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(tpAddr)) {
        return EFAULT;
    }
    
    struct timespec {
        i64 tv_sec;
        i64 tv_nsec;
    };
    
    timespec* tp = reinterpret_cast<timespec*>(tpAddr);
    u64 ms = process::Scheduler::systemTime();
    tp->tv_sec = static_cast<i64>(ms / 1000);
    tp->tv_nsec = static_cast<i64>((ms % 1000) * 1000000);
    
    return 0;
}

i64 sysClockSettime(u64, u64, u64, u64, u64, u64) {
    return EPERM;
}

i64 sysNanosleep(u64 reqAddr, u64, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(reqAddr)) {
        return EFAULT;
    }
    
    struct timespec {
        i64 tv_sec;
        i64 tv_nsec;
    };
    
    const timespec* req = reinterpret_cast<const timespec*>(reqAddr);
    u64 ms = static_cast<u64>(req->tv_sec * 1000 + req->tv_nsec / 1000000);
    
    process::Scheduler::sleep(ms);
    return 0;
}

i64 sysGetrlimit(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysSetrlimit(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysGetrusage(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysUname(u64 bufAddr, u64, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(bufAddr)) {
        return EFAULT;
    }
    
    struct utsname {
        char sysname[65];
        char nodename[65];
        char release[65];
        char version[65];
        char machine[65];
    };
    
    utsname* buf = reinterpret_cast<utsname*>(bufAddr);
    strcpy(buf->sysname, "SertOS", 65);
    strcpy(buf->nodename, "sertos", 65);
    strcpy(buf->release, "1.0.0", 65);
    strcpy(buf->version, "1.0.0", 65);
    strcpy(buf->machine, "x86_64", 65);
    
    return 0;
}

i64 sysSysinfo(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysPrctl(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysCapget(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysCapset(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysReboot(u64 magic1, u64 magic2, u64 cmd, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current || current->euid != 0) {
        return EPERM;
    }
    
    if (magic1 != 0xfee1dead || magic2 != 0x28121969) {
        return EINVAL;
    }
    
    if (cmd == 0x4321fedc) {
        power::PowerManager::shutdown();
    } else if (cmd == 0x1234567) {
        power::PowerManager::reboot();
    }
    
    return 0;
}

i64 sysSync(u64, u64, u64, u64, u64, u64) {
    return 0;
}

}

void registerAllHandlers() {
    Syscall::registerHandler(SYS_EXIT, sysExit);
    Syscall::registerHandler(SYS_WRITE, sysWrite);
    Syscall::registerHandler(SYS_READ, sysRead);
    Syscall::registerHandler(SYS_OPEN, sysOpen);
    Syscall::registerHandler(SYS_CLOSE, sysClose);
    Syscall::registerHandler(SYS_MMAP, sysMmap);
    Syscall::registerHandler(SYS_MUNMAP, sysMunmap);
    Syscall::registerHandler(SYS_BRK, sysBrk);
    Syscall::registerHandler(SYS_GETPID, sysGetpid);
    Syscall::registerHandler(SYS_FORK, sysFork);
    Syscall::registerHandler(SYS_EXEC, sysExec);
    Syscall::registerHandler(SYS_WAIT, sysWait);
    Syscall::registerHandler(SYS_YIELD, sysYield);
    Syscall::registerHandler(SYS_SLEEP, sysSleep);
    Syscall::registerHandler(SYS_GETTIME, sysGettime);
    
    Syscall::registerHandler(SYS_GETUID, sysGetuid);
    Syscall::registerHandler(SYS_GETGID, sysGetgid);
    Syscall::registerHandler(SYS_SETUID, sysSetuid);
    Syscall::registerHandler(SYS_SETGID, sysSetgid);
    Syscall::registerHandler(SYS_GETEUID, sysGeteuid);
    Syscall::registerHandler(SYS_GETEGID, sysGetegid);
    Syscall::registerHandler(SYS_SETEUID, sysSeteuid);
    Syscall::registerHandler(SYS_SETEGID, sysSetegid);
    
    Syscall::registerHandler(SYS_PIPE, sysPipe);
    Syscall::registerHandler(SYS_DUP, sysDup);
    Syscall::registerHandler(SYS_DUP2, sysDup2);
    Syscall::registerHandler(SYS_LSEEK, sysLseek);
    Syscall::registerHandler(SYS_STAT, sysStat);
    Syscall::registerHandler(SYS_FSTAT, sysFstat);
    Syscall::registerHandler(SYS_MKDIR, sysMkdir);
    Syscall::registerHandler(SYS_RMDIR, sysRmdir);
    Syscall::registerHandler(SYS_UNLINK, sysUnlink);
    Syscall::registerHandler(SYS_RENAME, sysRename);
    Syscall::registerHandler(SYS_CHDIR, sysChdir);
    Syscall::registerHandler(SYS_GETCWD, sysGetcwd);
    
    Syscall::registerHandler(SYS_KILL, sysKill);
    Syscall::registerHandler(SYS_SIGNAL, sysSignal);
    Syscall::registerHandler(SYS_SIGACTION, sysSigaction);
    Syscall::registerHandler(SYS_SIGPROCMASK, sysSigprocmask);
    Syscall::registerHandler(SYS_SIGSUSPEND, sysSigsuspend);
    
    Syscall::registerHandler(SYS_SHMGET, sysShmget);
    Syscall::registerHandler(SYS_SHMAT, sysShmat);
    Syscall::registerHandler(SYS_SHMDT, sysShmdt);
    Syscall::registerHandler(SYS_SHMCTL, sysShmctl);
    
    Syscall::registerHandler(SYS_MSGGET, sysMsgget);
    Syscall::registerHandler(SYS_MSGSND, sysMsgsnd);
    Syscall::registerHandler(SYS_MSGRCV, sysMsgrcv);
    Syscall::registerHandler(SYS_MSGCTL, sysMsgctl);
    
    Syscall::registerHandler(SYS_SOCKET, sysSocket);
    Syscall::registerHandler(SYS_BIND, sysBind);
    Syscall::registerHandler(SYS_LISTEN, sysListen);
    Syscall::registerHandler(SYS_ACCEPT, sysAccept);
    Syscall::registerHandler(SYS_CONNECT, sysConnect);
    Syscall::registerHandler(SYS_SEND, sysSend);
    Syscall::registerHandler(SYS_RECV, sysRecv);
    Syscall::registerHandler(SYS_SHUTDOWN, sysShutdown);
    
    Syscall::registerHandler(SYS_MPROTECT, sysMprotect);
    Syscall::registerHandler(SYS_GETPPID, sysGetppid);
    Syscall::registerHandler(SYS_GETPGID, sysGetpgid);
    Syscall::registerHandler(SYS_SETPGID, sysSetpgid);
    Syscall::registerHandler(SYS_SETSID, sysSetsid);
    Syscall::registerHandler(SYS_GETSID, sysGetsid);
    
    Syscall::registerHandler(SYS_IOCTL, sysIoctl);
    Syscall::registerHandler(SYS_FCNTL, sysFcntl);
    
    Syscall::registerHandler(SYS_CLOCK_GETTIME, sysClockGettime);
    Syscall::registerHandler(SYS_CLOCK_SETTIME, sysClockSettime);
    Syscall::registerHandler(SYS_NANOSLEEP, sysNanosleep);
    
    Syscall::registerHandler(SYS_GETRLIMIT, sysGetrlimit);
    Syscall::registerHandler(SYS_SETRLIMIT, sysSetrlimit);
    Syscall::registerHandler(SYS_GETRUSAGE, sysGetrusage);
    
    Syscall::registerHandler(SYS_UNAME, sysUname);
    Syscall::registerHandler(SYS_SYSINFO, sysSysinfo);
    
    Syscall::registerHandler(SYS_PRCTL, sysPrctl);
    Syscall::registerHandler(SYS_CAPGET, sysCapget);
    Syscall::registerHandler(SYS_CAPSET, sysCapset);
    
    Syscall::registerHandler(SYS_REBOOT, sysReboot);
    Syscall::registerHandler(SYS_SYNC, sysSync);
}

}
