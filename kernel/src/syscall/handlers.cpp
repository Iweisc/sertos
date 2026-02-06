#include "../../include/syscall/syscall.hpp"
#include "../../include/process/process.hpp"
#include "../../include/process/scheduler.hpp"
#include "../../include/process/thread.hpp"
#include "../../include/memory/vmm.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/graphics/console.hpp"
#include "../../include/ipc/ipc.hpp"
#include "../../include/user/user.hpp"
#include "../../include/security/security.hpp"
#include "../../include/power/acpi.hpp"
#include "../../include/fs/sertfs.hpp"
#include "../../include/fs/vfs.hpp"
#include "../../include/fs/procfs.hpp"
#include "../../include/fs/devfs.hpp"
#include "../../include/loader/elf.hpp"
#include "../../include/sync/futex.hpp"
#include "../../include/net/socket.hpp"
#include "../../include/io/epoll.hpp"

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
    proc->fds[STDIN_FD].flags = fs::O_RDONLY;
    proc->fds[STDIN_FD].offset = 0;
    proc->fds[STDIN_FD].data = nullptr;
    proc->fds[STDIN_FD].valid = true;
    
    proc->fds[STDOUT_FD].type = process::FdType::Console;
    proc->fds[STDOUT_FD].flags = fs::O_WRONLY;
    proc->fds[STDOUT_FD].offset = 0;
    proc->fds[STDOUT_FD].data = nullptr;
    proc->fds[STDOUT_FD].valid = true;
    
    proc->fds[STDERR_FD].type = process::FdType::Console;
    proc->fds[STDERR_FD].flags = fs::O_WRONLY;
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
        if (!(fdesc.flags & fs::O_WRONLY) && !(fdesc.flags & fs::O_RDWR)) {
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
        if ((fdesc.flags & 0x3) == fs::O_WRONLY) {
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
    if ((flags & 0x3) == 0x0) fsFlags |= fs::SERTFS_O_READ;
    if ((flags & 0x3) == 0x1) fsFlags |= fs::SERTFS_O_WRITE;
    if ((flags & 0x3) == 0x2) fsFlags |= fs::SERTFS_O_READ | fs::SERTFS_O_WRITE;
    if (flags & 0x40) fsFlags |= fs::SERTFS_O_CREATE;
    if (flags & 0x200) fsFlags |= fs::SERTFS_O_TRUNCATE;
    if (flags & 0x400) fsFlags |= fs::SERTFS_O_APPEND;
    
    if (fsFlags == 0) fsFlags = fs::SERTFS_O_READ;
    
    fs::FileHandle handle = fs::SertFs::open(path, fsFlags);
    if (!handle.valid) {
        if (fsFlags & fs::SERTFS_O_CREATE) {
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
    
    fs::FileHandle handle = fs::SertFs::open(path, fs::SERTFS_O_READ);
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

i64 sysClone(u64 flags, u64 stackAddr, u64 parentTidAddr, u64 childTidAddr, u64 tlsAddr, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    u32 cloneFlags = static_cast<u32>(flags);
    
    u32* parentTid = nullptr;
    u32* childTid = nullptr;
    
    if ((flags & process::CLONE_PARENT_SETTID) && parentTidAddr) {
        if (!memory::VMM::isUserAddress(parentTidAddr)) return EFAULT;
        parentTid = reinterpret_cast<u32*>(parentTidAddr);
    }
    
    if ((flags & process::CLONE_CHILD_SETTID) && childTidAddr) {
        if (!memory::VMM::isUserAddress(childTidAddr)) return EFAULT;
        childTid = reinterpret_cast<u32*>(childTidAddr);
    }
    
    process::Thread* newThread = process::ThreadManager::createThread(
        current, cloneFlags, stackAddr, tlsAddr, parentTid, childTid);
    
    if (!newThread) return ENOMEM;
    
    return static_cast<i64>(newThread->tid);
}

i64 sysFutex(u64 uaddrVal, u64 op, u64 val, u64 timeoutAddr, u64 uaddr2Val, u64 val3) {
    if (!memory::VMM::isUserAddress(uaddrVal)) return EFAULT;
    
    u32* uaddr = reinterpret_cast<u32*>(uaddrVal);
    u32* uaddr2 = nullptr;
    
    if (uaddr2Val && !memory::VMM::isUserAddress(uaddr2Val)) return EFAULT;
    if (uaddr2Val) uaddr2 = reinterpret_cast<u32*>(uaddr2Val);
    
    i32 futexOp = static_cast<i32>(op & 0x7F);
    u64 timeout = 0;
    
    if (timeoutAddr && memory::VMM::isUserAddress(timeoutAddr)) {
        struct timespec {
            i64 tv_sec;
            i64 tv_nsec;
        };
        const timespec* ts = reinterpret_cast<const timespec*>(timeoutAddr);
        timeout = static_cast<u64>(ts->tv_sec * 1000 + ts->tv_nsec / 1000000);
    }
    
    return sync::Futex::futex(uaddr, futexOp, static_cast<u32>(val),
                              &timeout, uaddr2, static_cast<u32>(val3));
}

i64 sysSetTidAddress(u64 tidptrAddr, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    process::Thread* currentThread = process::ThreadManager::currentThread();
    if (!currentThread) return EFAULT;
    
    if (tidptrAddr && !memory::VMM::isUserAddress(tidptrAddr)) return EFAULT;
    
    currentThread->clearChildTid = reinterpret_cast<u32*>(tidptrAddr);
    
    return static_cast<i64>(currentThread->tid);
}

i64 sysGettid(u64, u64, u64, u64, u64, u64) {
    process::Thread* currentThread = process::ThreadManager::currentThread();
    if (!currentThread) {
        process::Process* current = process::PM::currentProcess();
        if (!current) return -1;
        return static_cast<i64>(current->pid);
    }
    return static_cast<i64>(currentThread->tid);
}

i64 sysTkill(u64 tid, u64 sig, u64, u64, u64, u64) {
    process::Thread* target = process::ThreadManager::getThread(static_cast<u32>(tid));
    if (!target) return ESRCH;
    
    ipc::SignalManager::send(target->process->pid, static_cast<i32>(sig));
    return 0;
}

i64 sysTgkill(u64 tgid, u64 tid, u64 sig, u64, u64, u64) {
    process::Thread* target = process::ThreadManager::getThread(static_cast<u32>(tid));
    if (!target) return ESRCH;
    if (target->process->pid != static_cast<u32>(tgid)) return ESRCH;
    
    ipc::SignalManager::send(target->process->pid, static_cast<i32>(sig));
    return 0;
}

i64 sysExitGroup(u64 status, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return 0;
    
    process::ThreadManager::exitThreadGroup(current, static_cast<i32>(status));
    process::PM::terminateProcess(current, static_cast<i32>(status));
    process::Scheduler::schedule();
    return 0;
}

i64 sysSetRobustList(u64 headAddr, u64 len, u64, u64, u64, u64) {
    if (len != sizeof(void*) * 3) return EINVAL;
    
    process::Thread* currentThread = process::ThreadManager::currentThread();
    if (!currentThread) return EFAULT;
    
    currentThread->robustListHead = headAddr;
    return 0;
}

i64 sysGetRobustList(u64 pid, u64 headPtrAddr, u64 lenPtrAddr, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(headPtrAddr) || !memory::VMM::isUserAddress(lenPtrAddr)) {
        return EFAULT;
    }
    
    process::Thread* target;
    if (pid == 0) {
        target = process::ThreadManager::currentThread();
    } else {
        target = process::ThreadManager::getThread(static_cast<u32>(pid));
    }
    
    if (!target) return ESRCH;
    
    *reinterpret_cast<u64*>(headPtrAddr) = target->robustListHead;
    *reinterpret_cast<usize*>(lenPtrAddr) = sizeof(void*) * 3;
    return 0;
}

i64 sysEpollCreate(u64 size, u64, u64, u64, u64, u64) {
    if (static_cast<i32>(size) <= 0) return EINVAL;
    return io::Epoll::create(0);
}

i64 sysEpollCreate1(u64 flags, u64, u64, u64, u64, u64) {
    return io::Epoll::create(static_cast<i32>(flags));
}

i64 sysEpollCtl(u64 epfd, u64 op, u64 fd, u64 eventAddr, u64, u64) {
    io::EpollEvent* event = nullptr;
    if (eventAddr && memory::VMM::isUserAddress(eventAddr)) {
        event = reinterpret_cast<io::EpollEvent*>(eventAddr);
    }
    
    return io::Epoll::ctl(static_cast<i32>(epfd), static_cast<i32>(op),
                          static_cast<i32>(fd), event);
}

i64 sysEpollWait(u64 epfd, u64 eventsAddr, u64 maxevents, u64 timeout, u64, u64) {
    if (!memory::VMM::isUserAddress(eventsAddr)) return EFAULT;
    
    io::EpollEvent* events = reinterpret_cast<io::EpollEvent*>(eventsAddr);
    return io::Epoll::wait(static_cast<i32>(epfd), events,
                           static_cast<i32>(maxevents), static_cast<i32>(timeout));
}

i64 sysEpollPwait(u64 epfd, u64 eventsAddr, u64 maxevents, u64 timeout, u64 sigmaskAddr, u64) {
    (void)sigmaskAddr;
    return sysEpollWait(epfd, eventsAddr, maxevents, timeout, 0, 0);
}

i64 sysPoll(u64 fdsAddr, u64 nfds, u64 timeout, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(fdsAddr)) return EFAULT;
    
    io::PollFd* fds = reinterpret_cast<io::PollFd*>(fdsAddr);
    return io::Poll::poll(fds, static_cast<u32>(nfds), static_cast<i32>(timeout));
}

i64 sysPpoll(u64 fdsAddr, u64 nfds, u64 tmoAddr, u64 sigmaskAddr, u64, u64) {
    (void)sigmaskAddr;
    
    i32 timeout = -1;
    if (tmoAddr && memory::VMM::isUserAddress(tmoAddr)) {
        struct timespec {
            i64 tv_sec;
            i64 tv_nsec;
        };
        const timespec* ts = reinterpret_cast<const timespec*>(tmoAddr);
        timeout = static_cast<i32>(ts->tv_sec * 1000 + ts->tv_nsec / 1000000);
    }
    
    return sysPoll(fdsAddr, nfds, static_cast<u64>(timeout), 0, 0, 0);
}

i64 sysSelect(u64 nfds, u64 readfdsAddr, u64 writefdsAddr, u64 exceptfdsAddr, u64 timeoutAddr, u64) {
    io::FdSet* readfds = nullptr;
    io::FdSet* writefds = nullptr;
    io::FdSet* exceptfds = nullptr;
    
    if (readfdsAddr && memory::VMM::isUserAddress(readfdsAddr)) {
        readfds = reinterpret_cast<io::FdSet*>(readfdsAddr);
    }
    if (writefdsAddr && memory::VMM::isUserAddress(writefdsAddr)) {
        writefds = reinterpret_cast<io::FdSet*>(writefdsAddr);
    }
    if (exceptfdsAddr && memory::VMM::isUserAddress(exceptfdsAddr)) {
        exceptfds = reinterpret_cast<io::FdSet*>(exceptfdsAddr);
    }
    
    i64 timeout = -1;
    if (timeoutAddr && memory::VMM::isUserAddress(timeoutAddr)) {
        struct timeval {
            i64 tv_sec;
            i64 tv_usec;
        };
        const timeval* tv = reinterpret_cast<const timeval*>(timeoutAddr);
        timeout = tv->tv_sec * 1000 + tv->tv_usec / 1000;
    }
    
    return io::Select::select(static_cast<i32>(nfds), readfds, writefds, exceptfds, timeout);
}

i64 sysPselect6(u64 nfds, u64 readfdsAddr, u64 writefdsAddr, u64 exceptfdsAddr, u64 timeoutAddr, u64 sigmaskAddr) {
    (void)sigmaskAddr;
    
    i64 timeout = -1;
    if (timeoutAddr && memory::VMM::isUserAddress(timeoutAddr)) {
        struct timespec {
            i64 tv_sec;
            i64 tv_nsec;
        };
        const timespec* ts = reinterpret_cast<const timespec*>(timeoutAddr);
        timeout = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
    }
    
    io::FdSet* readfds = readfdsAddr ? reinterpret_cast<io::FdSet*>(readfdsAddr) : nullptr;
    io::FdSet* writefds = writefdsAddr ? reinterpret_cast<io::FdSet*>(writefdsAddr) : nullptr;
    io::FdSet* exceptfds = exceptfdsAddr ? reinterpret_cast<io::FdSet*>(exceptfdsAddr) : nullptr;
    
    return io::Select::select(static_cast<i32>(nfds), readfds, writefds, exceptfds, timeout);
}

i64 sysEventfd(u64 initval, u64, u64, u64, u64, u64) {
    return io::EventFdManager::create(static_cast<u32>(initval), 0);
}

i64 sysEventfd2(u64 initval, u64 flags, u64, u64, u64, u64) {
    return io::EventFdManager::create(static_cast<u32>(initval), static_cast<i32>(flags));
}

i64 sysTimerfdCreate(u64 clockid, u64 flags, u64, u64, u64, u64) {
    return io::TimerFdManager::create(static_cast<i32>(clockid), static_cast<i32>(flags));
}

i64 sysTimerfdSettime(u64 fd, u64 flags, u64 newValueAddr, u64 oldValueAddr, u64, u64) {
    if (!memory::VMM::isUserAddress(newValueAddr)) return EFAULT;
    
    io::ITimerSpec* newValue = reinterpret_cast<io::ITimerSpec*>(newValueAddr);
    io::ITimerSpec* oldValue = nullptr;
    if (oldValueAddr && memory::VMM::isUserAddress(oldValueAddr)) {
        oldValue = reinterpret_cast<io::ITimerSpec*>(oldValueAddr);
    }
    
    return io::TimerFdManager::settime(static_cast<i32>(fd), static_cast<i32>(flags),
                                        newValue, oldValue);
}

i64 sysTimerfdGettime(u64 fd, u64 currValueAddr, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(currValueAddr)) return EFAULT;
    
    io::ITimerSpec* currValue = reinterpret_cast<io::ITimerSpec*>(currValueAddr);
    return io::TimerFdManager::gettime(static_cast<i32>(fd), currValue);
}

i64 sysSignalfd(u64 fd, u64 maskAddr, u64, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(maskAddr)) return EFAULT;
    const u64* mask = reinterpret_cast<const u64*>(maskAddr);
    return io::SignalFdManager::create(static_cast<i32>(fd), mask, 0);
}

i64 sysSignalfd4(u64 fd, u64 maskAddr, u64 flags, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(maskAddr)) return EFAULT;
    const u64* mask = reinterpret_cast<const u64*>(maskAddr);
    return io::SignalFdManager::create(static_cast<i32>(fd), mask, static_cast<i32>(flags));
}

i64 sysOpenat(u64 dirfd, u64 pathAddr, u64 flags, u64 mode, u64, u64) {
    (void)mode;
    
    if (!memory::VMM::isUserAddress(pathAddr)) return EFAULT;
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (dirfd == static_cast<u64>(-100)) {
        return sysOpen(pathAddr, flags, 0, 0, 0, 0);
    }
    
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (path[0] == '/') {
        return sysOpen(pathAddr, flags, 0, 0, 0, 0);
    }
    
    return sysOpen(pathAddr, flags, 0, 0, 0, 0);
}

i64 sysMkdirat(u64 dirfd, u64 pathAddr, u64 mode, u64, u64, u64) {
    (void)dirfd;
    (void)mode;
    return sysMkdir(pathAddr, 0, 0, 0, 0, 0);
}

i64 sysUnlinkat(u64 dirfd, u64 pathAddr, u64 flags, u64, u64, u64) {
    (void)dirfd;
    if (flags & 0x200) {
        return sysRmdir(pathAddr, 0, 0, 0, 0, 0);
    }
    return sysUnlink(pathAddr, 0, 0, 0, 0, 0);
}

i64 sysRenameat(u64 olddirfd, u64 oldpathAddr, u64 newdirfd, u64 newpathAddr, u64, u64) {
    (void)olddirfd;
    (void)newdirfd;
    return sysRename(oldpathAddr, newpathAddr, 0, 0, 0, 0);
}

i64 sysFstatat(u64 dirfd, u64 pathAddr, u64 statbufAddr, u64 flags, u64, u64) {
    (void)dirfd;
    (void)flags;
    return sysStat(pathAddr, statbufAddr, 0, 0, 0, 0);
}

i64 sysReadlinkat(u64 dirfd, u64 pathAddr, u64 bufAddr, u64 bufsiz, u64, u64) {
    (void)dirfd;
    if (!memory::VMM::isUserAddress(pathAddr) || !memory::VMM::isUserAddress(bufAddr)) {
        return EFAULT;
    }
    return EINVAL;
}

i64 sysFaccessat(u64 dirfd, u64 pathAddr, u64 mode, u64 flags, u64, u64) {
    (void)dirfd;
    (void)flags;
    
    if (!memory::VMM::isUserAddress(pathAddr)) return EFAULT;
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (!fs::SertFs::exists(path)) return ENOENT;
    
    return 0;
}

i64 sysFchmodat(u64 dirfd, u64 pathAddr, u64 mode, u64 flags, u64, u64) {
    (void)dirfd;
    (void)flags;
    (void)mode;
    
    if (!memory::VMM::isUserAddress(pathAddr)) return EFAULT;
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (!fs::SertFs::exists(path)) return ENOENT;
    
    return 0;
}

i64 sysFchownat(u64 dirfd, u64 pathAddr, u64 uid, u64 gid, u64 flags, u64) {
    (void)dirfd;
    (void)flags;
    (void)uid;
    (void)gid;
    
    if (!memory::VMM::isUserAddress(pathAddr)) return EFAULT;
    const char* path = reinterpret_cast<const char*>(pathAddr);
    
    if (!fs::SertFs::exists(path)) return ENOENT;
    
    return 0;
}

i64 sysMremap(u64 oldAddr, u64 oldSize, u64 newSize, u64 flags, u64 newAddr, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (!memory::VMM::isUserAddress(oldAddr)) return EINVAL;
    
    if (newSize <= oldSize) {
        return static_cast<i64>(oldAddr);
    }
    
    u64 additionalPages = (newSize - oldSize + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
    u64 newEnd = oldAddr + oldSize;
    
    for (u64 i = 0; i < additionalPages; i++) {
        void* physPage = memory::PMM::allocatePage();
        if (!physPage) return ENOMEM;
        
        u8* pagePtr = reinterpret_cast<u8*>(physPage);
        for (usize j = 0; j < memory::PAGE_SIZE; j++) {
            pagePtr[j] = 0;
        }
        
        if (!memory::VMM::mapPageIn(current->pageTable, newEnd + i * memory::PAGE_SIZE,
                reinterpret_cast<u64>(physPage),
                memory::PAGE_PRESENT | memory::PAGE_WRITABLE | memory::PAGE_USER)) {
            memory::PMM::freePage(physPage);
            return ENOMEM;
        }
    }
    
    (void)flags;
    (void)newAddr;
    return static_cast<i64>(oldAddr);
}

i64 sysMadvise(u64 addr, u64 length, u64 advice, u64, u64, u64) {
    (void)addr;
    (void)length;
    (void)advice;
    return 0;
}

i64 sysMincore(u64 addr, u64 length, u64 vecAddr, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(addr) || !memory::VMM::isUserAddress(vecAddr)) {
        return EFAULT;
    }
    
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    u8* vec = reinterpret_cast<u8*>(vecAddr);
    u64 pages = (length + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
    
    for (u64 i = 0; i < pages; i++) {
        u64 pageAddr = addr + i * memory::PAGE_SIZE;
        u64 physAddr = memory::VMM::getPhysicalAddressIn(current->pageTable, pageAddr);
        vec[i] = physAddr ? 1 : 0;
    }
    
    return 0;
}

i64 sysMemfdCreate(u64 nameAddr, u64 flags, u64, u64, u64, u64) {
    (void)nameAddr;
    (void)flags;
    
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    i32 fd = allocateFd(current);
    if (fd < 0) return EMFILE;
    
    current->fds[fd].type = process::FdType::File;
    current->fds[fd].flags = fs::O_RDWR;
    current->fds[fd].offset = 0;
    current->fds[fd].data = nullptr;
    current->fds[fd].valid = true;
    
    return fd;
}

i64 sysGetrandom(u64 bufAddr, u64 buflen, u64 flags, u64, u64, u64) {
    (void)flags;
    
    if (!memory::VMM::isUserAddress(bufAddr)) return EFAULT;
    
    u8* buf = reinterpret_cast<u8*>(bufAddr);
    
    static u64 seed = 0x5DEECE66DULL;
    for (u64 i = 0; i < buflen; i++) {
        seed = (seed * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
        buf[i] = static_cast<u8>(seed >> 16);
    }
    
    return static_cast<i64>(buflen);
}

i64 sysPrlimit64(u64 pid, u64 resource, u64 newLimitAddr, u64 oldLimitAddr, u64, u64) {
    (void)pid;
    (void)resource;
    (void)newLimitAddr;
    (void)oldLimitAddr;
    return 0;
}

i64 sysWaitid(u64 idtype, u64 id, u64 infopAddr, u64 options, u64, u64) {
    (void)idtype;
    (void)id;
    (void)infopAddr;
    (void)options;
    return ENOSYS;
}

i64 sysSocketImpl(u64 domain, u64 type, u64 protocol, u64, u64, u64) {
    return net::SocketManager::create(static_cast<i32>(domain), static_cast<i32>(type),
                                       static_cast<i32>(protocol));
}

i64 sysBindImpl(u64 sockfd, u64 addrAddr, u64 addrlen, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(addrAddr)) return EFAULT;
    const net::SockAddr* addr = reinterpret_cast<const net::SockAddr*>(addrAddr);
    return net::SocketManager::bind(static_cast<i32>(sockfd), addr, static_cast<u32>(addrlen));
}

i64 sysListenImpl(u64 sockfd, u64 backlog, u64, u64, u64, u64) {
    return net::SocketManager::listen(static_cast<i32>(sockfd), static_cast<i32>(backlog));
}

i64 sysAcceptImpl(u64 sockfd, u64 addrAddr, u64 addrlenAddr, u64, u64, u64) {
    net::SockAddr* addr = nullptr;
    u32* addrlen = nullptr;
    
    if (addrAddr && memory::VMM::isUserAddress(addrAddr)) {
        addr = reinterpret_cast<net::SockAddr*>(addrAddr);
    }
    if (addrlenAddr && memory::VMM::isUserAddress(addrlenAddr)) {
        addrlen = reinterpret_cast<u32*>(addrlenAddr);
    }
    
    return net::SocketManager::accept(static_cast<i32>(sockfd), addr, addrlen);
}

i64 sysConnectImpl(u64 sockfd, u64 addrAddr, u64 addrlen, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(addrAddr)) return EFAULT;
    const net::SockAddr* addr = reinterpret_cast<const net::SockAddr*>(addrAddr);
    return net::SocketManager::connect(static_cast<i32>(sockfd), addr, static_cast<u32>(addrlen));
}

i64 sysSendImpl(u64 sockfd, u64 bufAddr, u64 len, u64 flags, u64, u64) {
    if (!memory::VMM::isUserAddress(bufAddr)) return EFAULT;
    const void* buf = reinterpret_cast<const void*>(bufAddr);
    return net::SocketManager::send(static_cast<i32>(sockfd), buf, len, static_cast<i32>(flags));
}

i64 sysRecvImpl(u64 sockfd, u64 bufAddr, u64 len, u64 flags, u64, u64) {
    if (!memory::VMM::isUserAddress(bufAddr)) return EFAULT;
    void* buf = reinterpret_cast<void*>(bufAddr);
    return net::SocketManager::recv(static_cast<i32>(sockfd), buf, len, static_cast<i32>(flags));
}

i64 sysShutdownImpl(u64 sockfd, u64 how, u64, u64, u64, u64) {
    return net::SocketManager::shutdown(static_cast<i32>(sockfd), static_cast<i32>(how));
}

i64 sysSendto(u64 sockfd, u64 bufAddr, u64 len, u64 flags, u64 destAddrAddr, u64 addrlen) {
    if (!memory::VMM::isUserAddress(bufAddr)) return EFAULT;
    
    const void* buf = reinterpret_cast<const void*>(bufAddr);
    const net::SockAddr* destAddr = nullptr;
    
    if (destAddrAddr && memory::VMM::isUserAddress(destAddrAddr)) {
        destAddr = reinterpret_cast<const net::SockAddr*>(destAddrAddr);
    }
    
    return net::SocketManager::sendto(static_cast<i32>(sockfd), buf, len,
                                       static_cast<i32>(flags), destAddr, static_cast<u32>(addrlen));
}

i64 sysRecvfrom(u64 sockfd, u64 bufAddr, u64 len, u64 flags, u64 srcAddrAddr, u64 addrlenAddr) {
    if (!memory::VMM::isUserAddress(bufAddr)) return EFAULT;
    
    void* buf = reinterpret_cast<void*>(bufAddr);
    net::SockAddr* srcAddr = nullptr;
    u32* addrlen = nullptr;
    
    if (srcAddrAddr && memory::VMM::isUserAddress(srcAddrAddr)) {
        srcAddr = reinterpret_cast<net::SockAddr*>(srcAddrAddr);
    }
    if (addrlenAddr && memory::VMM::isUserAddress(addrlenAddr)) {
        addrlen = reinterpret_cast<u32*>(addrlenAddr);
    }
    
    return net::SocketManager::recvfrom(static_cast<i32>(sockfd), buf, len,
                                         static_cast<i32>(flags), srcAddr, addrlen);
}

i64 sysSetsockopt(u64 sockfd, u64 level, u64 optname, u64 optvalAddr, u64 optlen, u64) {
    const void* optval = nullptr;
    if (optvalAddr && memory::VMM::isUserAddress(optvalAddr)) {
        optval = reinterpret_cast<const void*>(optvalAddr);
    }
    
    return net::SocketManager::setsockopt(static_cast<i32>(sockfd), static_cast<i32>(level),
                                           static_cast<i32>(optname), optval, static_cast<u32>(optlen));
}

i64 sysGetsockopt(u64 sockfd, u64 level, u64 optname, u64 optvalAddr, u64 optlenAddr, u64) {
    void* optval = nullptr;
    u32* optlen = nullptr;
    
    if (optvalAddr && memory::VMM::isUserAddress(optvalAddr)) {
        optval = reinterpret_cast<void*>(optvalAddr);
    }
    if (optlenAddr && memory::VMM::isUserAddress(optlenAddr)) {
        optlen = reinterpret_cast<u32*>(optlenAddr);
    }
    
    return net::SocketManager::getsockopt(static_cast<i32>(sockfd), static_cast<i32>(level),
                                           static_cast<i32>(optname), optval, optlen);
}

i64 sysGetsockname(u64 sockfd, u64 addrAddr, u64 addrlenAddr, u64, u64, u64) {
    net::SockAddr* addr = nullptr;
    u32* addrlen = nullptr;
    
    if (addrAddr && memory::VMM::isUserAddress(addrAddr)) {
        addr = reinterpret_cast<net::SockAddr*>(addrAddr);
    }
    if (addrlenAddr && memory::VMM::isUserAddress(addrlenAddr)) {
        addrlen = reinterpret_cast<u32*>(addrlenAddr);
    }
    
    return net::SocketManager::getsockname(static_cast<i32>(sockfd), addr, addrlen);
}

i64 sysGetpeername(u64 sockfd, u64 addrAddr, u64 addrlenAddr, u64, u64, u64) {
    net::SockAddr* addr = nullptr;
    u32* addrlen = nullptr;
    
    if (addrAddr && memory::VMM::isUserAddress(addrAddr)) {
        addr = reinterpret_cast<net::SockAddr*>(addrAddr);
    }
    if (addrlenAddr && memory::VMM::isUserAddress(addrlenAddr)) {
        addrlen = reinterpret_cast<u32*>(addrlenAddr);
    }
    
    return net::SocketManager::getpeername(static_cast<i32>(sockfd), addr, addrlen);
}

i64 sysSocketpair(u64 domain, u64 type, u64 protocol, u64 svAddr, u64, u64) {
    if (!memory::VMM::isUserAddress(svAddr)) return EFAULT;
    
    i32* sv = reinterpret_cast<i32*>(svAddr);
    return net::SocketManager::socketpair(static_cast<i32>(domain), static_cast<i32>(type),
                                           static_cast<i32>(protocol), sv);
}

i64 sysArchPrctl(u64 code, u64 addr, u64, u64, u64, u64) {
    process::Thread* currentThread = process::ThreadManager::currentThread();
    
    constexpr u64 ARCH_SET_GS = 0x1001;
    constexpr u64 ARCH_SET_FS = 0x1002;
    constexpr u64 ARCH_GET_FS = 0x1003;
    constexpr u64 ARCH_GET_GS = 0x1004;
    
    switch (code) {
        case ARCH_SET_FS:
            if (currentThread) {
                currentThread->tlsBase = addr;
            }
            asm volatile("wrfsbase %0" :: "r"(addr));
            return 0;
            
        case ARCH_GET_FS:
            if (!memory::VMM::isUserAddress(addr)) return EFAULT;
            if (currentThread) {
                *reinterpret_cast<u64*>(addr) = currentThread->tlsBase;
            } else {
                u64 fsbase;
                asm volatile("rdfsbase %0" : "=r"(fsbase));
                *reinterpret_cast<u64*>(addr) = fsbase;
            }
            return 0;
            
        case ARCH_SET_GS:
            asm volatile("wrgsbase %0" :: "r"(addr));
            return 0;
            
        case ARCH_GET_GS:
            if (!memory::VMM::isUserAddress(addr)) return EFAULT;
            {
                u64 gsbase;
                asm volatile("rdgsbase %0" : "=r"(gsbase));
                *reinterpret_cast<u64*>(addr) = gsbase;
            }
            return 0;
            
        default:
            return EINVAL;
    }
}

struct IoVec {
    void* base;
    usize len;
};

i64 sysReadv(u64 fd, u64 iovAddr, u64 iovcnt, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(iovAddr)) return EFAULT;
    
    const IoVec* iov = reinterpret_cast<const IoVec*>(iovAddr);
    i64 totalRead = 0;
    
    for (u64 i = 0; i < iovcnt; i++) {
        if (!memory::VMM::isUserAddress(reinterpret_cast<u64>(iov[i].base))) {
            return EFAULT;
        }
        
        i64 result = sysRead(fd, reinterpret_cast<u64>(iov[i].base), iov[i].len, 0, 0, 0);
        if (result < 0) {
            return totalRead > 0 ? totalRead : result;
        }
        
        totalRead += result;
        if (static_cast<usize>(result) < iov[i].len) break;
    }
    
    return totalRead;
}

i64 sysWritev(u64 fd, u64 iovAddr, u64 iovcnt, u64, u64, u64) {
    if (!memory::VMM::isUserAddress(iovAddr)) return EFAULT;
    
    const IoVec* iov = reinterpret_cast<const IoVec*>(iovAddr);
    i64 totalWritten = 0;
    
    for (u64 i = 0; i < iovcnt; i++) {
        if (!memory::VMM::isUserAddress(reinterpret_cast<u64>(iov[i].base))) {
            return EFAULT;
        }
        
        i64 result = sysWrite(fd, reinterpret_cast<u64>(iov[i].base), iov[i].len, 0, 0, 0);
        if (result < 0) {
            return totalWritten > 0 ? totalWritten : result;
        }
        
        totalWritten += result;
        if (static_cast<usize>(result) < iov[i].len) break;
    }
    
    return totalWritten;
}

i64 sysMmapImpl(u64 addr, u64 length, u64 prot, u64 flags, u64 fd, u64 offset) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    constexpr u64 MAP_ANONYMOUS = 0x20;
    constexpr u64 MAP_PRIVATE = 0x02;
    constexpr u64 MAP_FIXED = 0x10;
    constexpr u64 MMAP_BASE = 0x7F0000000000ULL;
    
    static u64 mmapOffset = 0;
    
    u64 alignedLength = align_up(length, static_cast<u64>(memory::PAGE_SIZE));
    u64 pages = alignedLength / memory::PAGE_SIZE;
    
    u64 mapAddr = addr;
    if (!(flags & MAP_FIXED) || mapAddr == 0) {
        mapAddr = MMAP_BASE + mmapOffset;
        mmapOffset += alignedLength;
    }
    
    u64 pageFlags = memory::PAGE_PRESENT | memory::PAGE_USER;
    if (prot & 0x2) pageFlags |= memory::PAGE_WRITABLE;
    if (!(prot & 0x4)) pageFlags |= memory::PAGE_NO_EXECUTE;
    
    for (u64 i = 0; i < pages; i++) {
        void* physPage = memory::PMM::allocatePage();
        if (!physPage) return ENOMEM;
        
        u8* pagePtr = reinterpret_cast<u8*>(physPage);
        for (usize j = 0; j < memory::PAGE_SIZE; j++) {
            pagePtr[j] = 0;
        }
        
        if (!memory::VMM::mapPageIn(current->pageTable, mapAddr + i * memory::PAGE_SIZE,
                reinterpret_cast<u64>(physPage), pageFlags)) {
            memory::PMM::freePage(physPage);
            return ENOMEM;
        }
    }
    
    if (!(flags & MAP_ANONYMOUS) && fd < process::MAX_FDS && current->fds[fd].valid) {
        process::FileDescriptor& fdesc = current->fds[fd];
        if (fdesc.type == process::FdType::File) {
            fs::FileHandle* handle = static_cast<fs::FileHandle*>(fdesc.data);
            if (handle && handle->valid) {
                fs::SertFs::seek(handle, static_cast<i64>(offset), fs::SeekMode::Set);
                fs::SertFs::read(handle, reinterpret_cast<char*>(mapAddr), length);
            }
        }
    }
    
    return static_cast<i64>(mapAddr);
}

i64 sysMunmapImpl(u64 addr, u64 length, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (!memory::VMM::isUserAddress(addr)) return EINVAL;
    
    u64 alignedLength = align_up(length, static_cast<u64>(memory::PAGE_SIZE));
    u64 pages = alignedLength / memory::PAGE_SIZE;
    
    for (u64 i = 0; i < pages; i++) {
        u64 pageAddr = addr + i * memory::PAGE_SIZE;
        u64 physAddr = memory::VMM::getPhysicalAddressIn(current->pageTable, pageAddr);
        
        if (physAddr) {
            memory::PMM::freePage(reinterpret_cast<void*>(physAddr));
            memory::VMM::unmapPageIn(current->pageTable, pageAddr);
        }
    }
    
    return 0;
}

i64 sysIoctlImpl(u64 fd, u64 request, u64 arg, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (fd >= process::MAX_FDS || !current->fds[fd].valid) {
        return EBADF;
    }
    
    constexpr u64 TCGETS = 0x5401;
    constexpr u64 TCSETS = 0x5402;
    constexpr u64 TIOCGWINSZ = 0x5413;
    constexpr u64 TIOCSWINSZ = 0x5414;
    constexpr u64 FIONREAD = 0x541B;
    constexpr u64 FIONBIO = 0x5421;
    
    switch (request) {
        case TCGETS:
        case TCSETS:
            return 0;
            
        case TIOCGWINSZ:
            if (arg && memory::VMM::isUserAddress(arg)) {
                struct winsize {
                    u16 ws_row;
                    u16 ws_col;
                    u16 ws_xpixel;
                    u16 ws_ypixel;
                };
                winsize* ws = reinterpret_cast<winsize*>(arg);
                ws->ws_row = 25;
                ws->ws_col = 80;
                ws->ws_xpixel = 640;
                ws->ws_ypixel = 400;
            }
            return 0;
            
        case TIOCSWINSZ:
            return 0;
            
        case FIONREAD:
            if (arg && memory::VMM::isUserAddress(arg)) {
                *reinterpret_cast<i32*>(arg) = 0;
            }
            return 0;
            
        case FIONBIO:
            return 0;
            
        default:
            return ENOTTY;
    }
}

i64 sysFcntlImpl(u64 fd, u64 cmd, u64 arg, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return EFAULT;
    
    if (fd >= process::MAX_FDS || !current->fds[fd].valid) {
        return EBADF;
    }
    
    constexpr u64 F_DUPFD = 0;
    constexpr u64 F_GETFD = 1;
    constexpr u64 F_SETFD = 2;
    constexpr u64 F_GETFL = 3;
    constexpr u64 F_SETFL = 4;
    constexpr u64 F_DUPFD_CLOEXEC = 1030;
    
    switch (cmd) {
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
            return sysDup(fd, 0, 0, 0, 0, 0);
            
        case F_GETFD:
            return 0;
            
        case F_SETFD:
            return 0;
            
        case F_GETFL:
            return static_cast<i64>(current->fds[fd].flags);
            
        case F_SETFL:
            current->fds[fd].flags = static_cast<u32>(arg);
            return 0;
            
        default:
            return EINVAL;
    }
}

}

void registerAllHandlers() {
    Syscall::registerHandler(SYS_EXIT, sysExit);
    Syscall::registerHandler(SYS_WRITE, sysWrite);
    Syscall::registerHandler(SYS_READ, sysRead);
    Syscall::registerHandler(SYS_OPEN, sysOpen);
    Syscall::registerHandler(SYS_CLOSE, sysClose);
    Syscall::registerHandler(SYS_MMAP, sysMmapImpl);
    Syscall::registerHandler(SYS_MUNMAP, sysMunmapImpl);
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
    
    Syscall::registerHandler(SYS_SOCKET, sysSocketImpl);
    Syscall::registerHandler(SYS_BIND, sysBindImpl);
    Syscall::registerHandler(SYS_LISTEN, sysListenImpl);
    Syscall::registerHandler(SYS_ACCEPT, sysAcceptImpl);
    Syscall::registerHandler(SYS_CONNECT, sysConnectImpl);
    Syscall::registerHandler(SYS_SEND, sysSendImpl);
    Syscall::registerHandler(SYS_RECV, sysRecvImpl);
    Syscall::registerHandler(SYS_SHUTDOWN, sysShutdownImpl);
    
    Syscall::registerHandler(SYS_MPROTECT, sysMprotect);
    Syscall::registerHandler(SYS_GETPPID, sysGetppid);
    Syscall::registerHandler(SYS_GETPGID, sysGetpgid);
    Syscall::registerHandler(SYS_SETPGID, sysSetpgid);
    Syscall::registerHandler(SYS_SETSID, sysSetsid);
    Syscall::registerHandler(SYS_GETSID, sysGetsid);
    
    Syscall::registerHandler(SYS_IOCTL, sysIoctlImpl);
    Syscall::registerHandler(SYS_FCNTL, sysFcntlImpl);
    
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
    
    Syscall::registerHandler(SYS_CLONE, sysClone);
    Syscall::registerHandler(SYS_FUTEX, sysFutex);
    Syscall::registerHandler(SYS_SET_TID_ADDRESS, sysSetTidAddress);
    Syscall::registerHandler(SYS_GETTID, sysGettid);
    Syscall::registerHandler(SYS_TKILL, sysTkill);
    Syscall::registerHandler(SYS_TGKILL, sysTgkill);
    Syscall::registerHandler(SYS_EXIT_GROUP, sysExitGroup);
    Syscall::registerHandler(SYS_SET_ROBUST_LIST, sysSetRobustList);
    Syscall::registerHandler(SYS_GET_ROBUST_LIST, sysGetRobustList);
    
    Syscall::registerHandler(SYS_EPOLL_CREATE, sysEpollCreate);
    Syscall::registerHandler(SYS_EPOLL_CREATE1, sysEpollCreate1);
    Syscall::registerHandler(SYS_EPOLL_CTL, sysEpollCtl);
    Syscall::registerHandler(SYS_EPOLL_WAIT, sysEpollWait);
    Syscall::registerHandler(SYS_EPOLL_PWAIT, sysEpollPwait);
    Syscall::registerHandler(SYS_POLL, sysPoll);
    Syscall::registerHandler(SYS_PPOLL, sysPpoll);
    Syscall::registerHandler(SYS_SELECT, sysSelect);
    Syscall::registerHandler(SYS_PSELECT6, sysPselect6);
    
    Syscall::registerHandler(SYS_EVENTFD, sysEventfd);
    Syscall::registerHandler(SYS_EVENTFD2, sysEventfd2);
    Syscall::registerHandler(SYS_TIMERFD_CREATE, sysTimerfdCreate);
    Syscall::registerHandler(SYS_TIMERFD_SETTIME, sysTimerfdSettime);
    Syscall::registerHandler(SYS_TIMERFD_GETTIME, sysTimerfdGettime);
    Syscall::registerHandler(SYS_SIGNALFD, sysSignalfd);
    Syscall::registerHandler(SYS_SIGNALFD4, sysSignalfd4);
    
    Syscall::registerHandler(SYS_OPENAT, sysOpenat);
    Syscall::registerHandler(SYS_MKDIRAT, sysMkdirat);
    Syscall::registerHandler(SYS_UNLINKAT, sysUnlinkat);
    Syscall::registerHandler(SYS_RENAMEAT, sysRenameat);
    Syscall::registerHandler(SYS_FSTATAT, sysFstatat);
    Syscall::registerHandler(SYS_READLINKAT, sysReadlinkat);
    Syscall::registerHandler(SYS_FACCESSAT, sysFaccessat);
    Syscall::registerHandler(SYS_FCHMODAT, sysFchmodat);
    Syscall::registerHandler(SYS_FCHOWNAT, sysFchownat);
    
    Syscall::registerHandler(SYS_MREMAP, sysMremap);
    Syscall::registerHandler(SYS_MADVISE, sysMadvise);
    Syscall::registerHandler(SYS_MINCORE, sysMincore);
    Syscall::registerHandler(SYS_MEMFD_CREATE, sysMemfdCreate);
    
    Syscall::registerHandler(SYS_GETRANDOM, sysGetrandom);
    Syscall::registerHandler(SYS_PRLIMIT64, sysPrlimit64);
    Syscall::registerHandler(SYS_WAITID, sysWaitid);
    
    Syscall::registerHandler(SYS_SENDTO, sysSendto);
    Syscall::registerHandler(SYS_RECVFROM, sysRecvfrom);
    Syscall::registerHandler(SYS_SETSOCKOPT, sysSetsockopt);
    Syscall::registerHandler(SYS_GETSOCKOPT, sysGetsockopt);
    Syscall::registerHandler(SYS_GETSOCKNAME, sysGetsockname);
    Syscall::registerHandler(SYS_GETPEERNAME, sysGetpeername);
    Syscall::registerHandler(SYS_SOCKETPAIR, sysSocketpair);
    
    Syscall::registerHandler(SYS_ARCH_PRCTL, sysArchPrctl);
    Syscall::registerHandler(SYS_READV, sysReadv);
    Syscall::registerHandler(SYS_WRITEV, sysWritev);
}

}
