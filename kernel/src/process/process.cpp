#include "../../include/process/process.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/cpu/gdt.hpp"

namespace sertos::process {

Process ProcessManager::sProcesses[MAX_PROCESSES];
Process* ProcessManager::sCurrentProcess = nullptr;
u32 ProcessManager::sNextPid = 1;
u32 ProcessManager::sProcessCount = 0;
bool ProcessManager::sInitialized = false;

constexpr u64 KERNEL_STACK_SIZE = 4 * memory::PAGE_SIZE;
constexpr u64 USER_STACK_PAGES = 16;

static void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

static void strcpy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void ProcessManager::initialize() {
    if (sInitialized) {
        return;
    }
    
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        sProcesses[i].pid = 0;
        sProcesses[i].state = ProcessState::Invalid;
        sProcesses[i].pageTable = nullptr;
        sProcesses[i].next = nullptr;
        sProcesses[i].prev = nullptr;
    }
    
    sCurrentProcess = nullptr;
    sNextPid = 1;
    sProcessCount = 0;
    sInitialized = true;
}

Process* ProcessManager::createProcess(const char* name) {
    if (!sInitialized) {
        return nullptr;
    }
    
    u32 pid = allocatePid();
    if (pid == 0) {
        return nullptr;
    }
    
    Process* proc = &sProcesses[pid % MAX_PROCESSES];
    
    memset(proc, 0, sizeof(Process));
    
    proc->pid = pid;
    proc->parentPid = sCurrentProcess ? sCurrentProcess->pid : 0;
    proc->state = ProcessState::Ready;
    proc->exitCode = 0;
    
    strcpy(proc->name, name, PROCESS_NAME_MAX);
    
    proc->pageTable = memory::VMM::createAddressSpace();
    if (!proc->pageTable) {
        freePid(pid);
        proc->state = ProcessState::Invalid;
        return nullptr;
    }
    
    setupKernelStack(proc);
    setupUserStack(proc);
    
    proc->heapStart = memory::USER_HEAP_START;
    proc->heapEnd = memory::USER_HEAP_START;
    proc->programBreak = memory::USER_HEAP_START;
    
    proc->context.cs = cpu::USER_CODE_SELECTOR;
    proc->context.ss = cpu::USER_DATA_SELECTOR;
    proc->context.rflags = 0x202;
    proc->context.rsp = proc->userStack;
    
    sProcessCount++;
    
    return proc;
}

Process* ProcessManager::createKernelProcess(const char* name, void (*entry)()) {
    if (!sInitialized) {
        return nullptr;
    }
    
    u32 pid = allocatePid();
    if (pid == 0) {
        return nullptr;
    }
    
    Process* proc = &sProcesses[pid % MAX_PROCESSES];
    
    memset(proc, 0, sizeof(Process));
    
    proc->pid = pid;
    proc->parentPid = 0;
    proc->state = ProcessState::Ready;
    proc->exitCode = 0;
    
    strcpy(proc->name, name, PROCESS_NAME_MAX);
    
    proc->pageTable = memory::VMM::kernelPageTable();
    
    setupKernelStack(proc);
    
    proc->context.cs = cpu::KERNEL_CODE_SELECTOR;
    proc->context.ss = cpu::KERNEL_DATA_SELECTOR;
    proc->context.rflags = 0x202;
    proc->context.rsp = proc->kernelStack;
    proc->context.rip = reinterpret_cast<u64>(entry);
    
    sProcessCount++;
    
    return proc;
}

Process* ProcessManager::forkProcess(Process* parent) {
    if (!sInitialized || !parent) {
        return nullptr;
    }
    
    u32 pid = allocatePid();
    if (pid == 0) {
        return nullptr;
    }
    
    Process* child = &sProcesses[pid % MAX_PROCESSES];
    
    *child = *parent;
    
    child->pid = pid;
    child->parentPid = parent->pid;
    child->state = ProcessState::Ready;
    child->exitCode = 0;
    child->cpuTime = 0;
    child->next = nullptr;
    child->prev = nullptr;
    
    child->pageTable = memory::VMM::cloneAddressSpace(parent->pageTable);
    if (!child->pageTable) {
        freePid(pid);
        child->state = ProcessState::Invalid;
        return nullptr;
    }
    
    setupKernelStack(child);
    
    child->context.rax = 0;
    
    sProcessCount++;
    
    return child;
}

void ProcessManager::terminateProcess(Process* proc, i32 exitCode) {
    if (!proc || proc->state == ProcessState::Invalid) {
        return;
    }
    
    proc->exitCode = exitCode;
    proc->state = ProcessState::Zombie;
    
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (sProcesses[i].state != ProcessState::Invalid &&
            sProcesses[i].parentPid == proc->pid) {
            sProcesses[i].parentPid = 1;
        }
    }
}

void ProcessManager::destroyProcess(Process* proc) {
    if (!proc || proc->state == ProcessState::Invalid) {
        return;
    }
    
    if (proc->pageTable && proc->pageTable != memory::VMM::kernelPageTable()) {
        memory::VMM::destroyAddressSpace(proc->pageTable);
    }
    
    if (proc->kernelStack) {
        memory::PMM::freePages(
            reinterpret_cast<void*>(proc->kernelStack - KERNEL_STACK_SIZE + memory::PAGE_SIZE),
            KERNEL_STACK_SIZE / memory::PAGE_SIZE
        );
    }
    
    freePid(proc->pid);
    
    proc->state = ProcessState::Invalid;
    proc->pid = 0;
    proc->pageTable = nullptr;
    proc->kernelStack = 0;
    
    sProcessCount--;
}

Process* ProcessManager::getProcess(u32 pid) {
    if (pid == 0 || pid >= MAX_PROCESSES) {
        return nullptr;
    }
    
    Process* proc = &sProcesses[pid % MAX_PROCESSES];
    if (proc->pid == pid && proc->state != ProcessState::Invalid) {
        return proc;
    }
    
    return nullptr;
}

Process* ProcessManager::currentProcess() {
    return sCurrentProcess;
}

void ProcessManager::setCurrentProcess(Process* proc) {
    sCurrentProcess = proc;
}

u32 ProcessManager::allocatePid() {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        u32 pid = sNextPid;
        sNextPid = (sNextPid % (MAX_PROCESSES - 1)) + 1;
        
        if (sProcesses[pid].state == ProcessState::Invalid) {
            return pid;
        }
    }
    
    return 0;
}

void ProcessManager::freePid(u32) {
}

u32 ProcessManager::processCount() {
    return sProcessCount;
}

bool ProcessManager::isInitialized() {
    return sInitialized;
}

void ProcessManager::setupKernelStack(Process* proc) {
    void* stack = memory::PMM::allocatePages(KERNEL_STACK_SIZE / memory::PAGE_SIZE);
    if (stack) {
        proc->kernelStack = reinterpret_cast<u64>(stack) + KERNEL_STACK_SIZE - 8;
    }
}

void ProcessManager::setupUserStack(Process* proc) {
    u64 stackBottom = memory::USER_STACK_TOP - memory::USER_STACK_SIZE;
    
    memory::VMM::allocateUserPages(
        proc->pageTable,
        stackBottom,
        USER_STACK_PAGES,
        memory::PAGE_PRESENT | memory::PAGE_WRITABLE | memory::PAGE_USER
    );
    
    proc->userStack = memory::USER_STACK_TOP - 8;
}

}
