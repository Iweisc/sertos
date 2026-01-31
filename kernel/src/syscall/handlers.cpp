#include "../../include/syscall/syscall.hpp"
#include "../../include/process/process.hpp"
#include "../../include/process/scheduler.hpp"
#include "../../include/memory/vmm.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/graphics/console.hpp"

namespace sertos::syscall {

using sertos::align_up;

namespace {

i64 sysExit(u64 status, u64, u64, u64, u64, u64) {
    process::Process* current = process::PM::currentProcess();
    if (current) {
        process::PM::terminateProcess(current, static_cast<i32>(status));
        process::Scheduler::schedule();
    }
    return 0;
}

i64 sysWrite(u64 fd, u64 bufAddr, u64 count, u64, u64, u64) {
    if (fd != STDOUT_FD && fd != STDERR_FD) {
        return EBADF;
    }
    
    process::Process* current = process::PM::currentProcess();
    if (!current) {
        return EFAULT;
    }
    
    if (!memory::VMM::isUserAddress(bufAddr)) {
        return EFAULT;
    }
    
    const char* buf = reinterpret_cast<const char*>(bufAddr);
    
    for (u64 i = 0; i < count; i++) {
        graphics::Console::putChar(buf[i]);
    }
    
    return static_cast<i64>(count);
}

i64 sysRead(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysOpen(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
}

i64 sysClose(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
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

i64 sysExec(u64, u64, u64, u64, u64, u64) {
    return ENOSYS;
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
}

}
