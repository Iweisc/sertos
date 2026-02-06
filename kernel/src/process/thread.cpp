#include "../../include/process/thread.hpp"
#include "../../include/process/process.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/cpu/gdt.hpp"

namespace sertos::process {

Thread ThreadManager::sThreads[MAX_TOTAL_THREADS];
ThreadGroup ThreadManager::sThreadGroups[256];
Thread* ThreadManager::sCurrentThread = nullptr;
u32 ThreadManager::sNextTid = 1;
u32 ThreadManager::sThreadCount = 0;
bool ThreadManager::sInitialized = false;

constexpr u64 KERNEL_STACK_SIZE = 4 * memory::PAGE_SIZE;

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

void ThreadManager::initialize() {
    if (sInitialized) {
        return;
    }
    
    for (u32 i = 0; i < MAX_TOTAL_THREADS; i++) {
        sThreads[i].tid = 0;
        sThreads[i].state = ThreadState::Invalid;
        sThreads[i].active = false;
        sThreads[i].next = nullptr;
        sThreads[i].prev = nullptr;
        sThreads[i].threadGroupNext = nullptr;
    }
    
    for (u32 i = 0; i < 256; i++) {
        sThreadGroups[i].tgid = 0;
        sThreadGroups[i].leader = nullptr;
        sThreadGroups[i].threads = nullptr;
        sThreadGroups[i].threadCount = 0;
        sThreadGroups[i].activeCount = 0;
        sThreadGroups[i].active = false;
    }
    
    sCurrentThread = nullptr;
    sNextTid = 1;
    sThreadCount = 0;
    sInitialized = true;
}

Thread* ThreadManager::createThread(u32 pid, void* entry, void* arg, u32 flags) {
    if (!sInitialized) {
        return nullptr;
    }
    
    Process* proc = PM::getProcess(pid);
    if (!proc) {
        return nullptr;
    }
    
    u32 tid = allocateTid();
    if (tid == 0) {
        return nullptr;
    }
    
    Thread* thread = &sThreads[tid % MAX_TOTAL_THREADS];
    
    memset(thread, 0, sizeof(Thread));
    
    thread->tid = tid;
    thread->pid = pid;
    thread->state = ThreadState::Ready;
    thread->exitCode = 0;
    thread->active = true;
    thread->detached = false;
    thread->inKernel = false;
    
    strcpy(thread->name, "thread", THREAD_NAME_MAX);
    
    setupKernelStack(thread);
    setupThreadStack(thread, entry, arg);
    
    thread->context.cs = cpu::USER_CODE_SELECTOR;
    thread->context.ss = cpu::USER_DATA_SELECTOR;
    thread->context.rflags = 0x202;
    
    ThreadGroup* group = getThreadGroup(pid);
    if (group) {
        addToThreadGroup(thread, group);
    }
    
    sThreadCount++;
    
    return thread;
}

Thread* ThreadManager::createMainThread(u32 pid, void* entry) {
    if (!sInitialized) {
        return nullptr;
    }
    
    Process* proc = PM::getProcess(pid);
    if (!proc) {
        return nullptr;
    }
    
    u32 tid = allocateTid();
    if (tid == 0) {
        return nullptr;
    }
    
    Thread* thread = &sThreads[tid % MAX_TOTAL_THREADS];
    
    memset(thread, 0, sizeof(Thread));
    
    thread->tid = tid;
    thread->pid = pid;
    thread->state = ThreadState::Ready;
    thread->exitCode = 0;
    thread->active = true;
    thread->detached = false;
    thread->inKernel = false;
    
    strcpy(thread->name, proc->name, THREAD_NAME_MAX);
    
    thread->kernelStack = proc->kernelStack;
    thread->userStack = proc->userStack;
    thread->stackBase = memory::USER_STACK_TOP - memory::USER_STACK_SIZE;
    thread->stackSize = memory::USER_STACK_SIZE;
    
    thread->context.r15 = proc->context.r15;
    thread->context.r14 = proc->context.r14;
    thread->context.r13 = proc->context.r13;
    thread->context.r12 = proc->context.r12;
    thread->context.r11 = proc->context.r11;
    thread->context.r10 = proc->context.r10;
    thread->context.r9 = proc->context.r9;
    thread->context.r8 = proc->context.r8;
    thread->context.rbp = proc->context.rbp;
    thread->context.rdi = proc->context.rdi;
    thread->context.rsi = proc->context.rsi;
    thread->context.rdx = proc->context.rdx;
    thread->context.rcx = proc->context.rcx;
    thread->context.rbx = proc->context.rbx;
    thread->context.rax = proc->context.rax;
    thread->context.rip = reinterpret_cast<u64>(entry);
    thread->context.cs = proc->context.cs;
    thread->context.rflags = proc->context.rflags;
    thread->context.rsp = proc->context.rsp;
    thread->context.ss = proc->context.ss;
    
    ThreadGroup* group = createThreadGroup(pid);
    if (group) {
        group->leader = thread;
        group->leaderTid = tid;
        addToThreadGroup(thread, group);
    }
    
    sThreadCount++;
    
    return thread;
}

Thread* ThreadManager::cloneThread(Thread* parent, u32 flags, void* stack, u32* parentTid, u32* childTid, u64 tls) {
    if (!sInitialized || !parent) {
        return nullptr;
    }
    
    u32 tid = allocateTid();
    if (tid == 0) {
        return nullptr;
    }
    
    Thread* child = &sThreads[tid % MAX_TOTAL_THREADS];
    
    *child = *parent;
    
    child->tid = tid;
    child->state = ThreadState::Ready;
    child->exitCode = 0;
    child->cpuTime = 0;
    child->next = nullptr;
    child->prev = nullptr;
    child->threadGroupNext = nullptr;
    child->active = true;
    
    setupKernelStack(child);
    
    if (stack) {
        child->userStack = reinterpret_cast<u64>(stack);
        child->context.rsp = child->userStack;
    }
    
    if (flags & CLONE_SETTLS) {
        child->tls.fsBase = tls;
    }
    
    if (flags & CLONE_PARENT_SETTID && parentTid) {
        *parentTid = tid;
    }
    
    if (flags & CLONE_CHILD_SETTID && childTid) {
        child->setChildTid = childTid;
    }
    
    if (flags & CLONE_CHILD_CLEARTID && childTid) {
        child->clearChildTid = childTid;
    }
    
    child->context.rax = 0;
    
    if (flags & CLONE_THREAD) {
        ThreadGroup* group = getThreadGroup(parent->pid);
        if (group) {
            addToThreadGroup(child, group);
        }
    } else {
        Process* proc = PM::getProcess(child->pid);
        if (proc) {
            ThreadGroup* group = createThreadGroup(child->pid);
            if (group) {
                group->leader = child;
                group->leaderTid = tid;
                addToThreadGroup(child, group);
            }
        }
    }
    
    sThreadCount++;
    
    return child;
}

void ThreadManager::terminateThread(Thread* thread, i32 exitCode) {
    if (!thread || thread->state == ThreadState::Invalid) {
        return;
    }
    
    thread->exitCode = exitCode;
    thread->state = ThreadState::Zombie;
    
    if (thread->clearChildTid) {
        *thread->clearChildTid = 0;
    }
    
    ThreadGroup* group = getThreadGroup(thread->pid);
    if (group) {
        group->activeCount--;
        
        if (group->activeCount == 0) {
            Process* proc = PM::getProcess(thread->pid);
            if (proc) {
                PM::terminateProcess(proc, exitCode);
            }
        }
    }
}

void ThreadManager::destroyThread(Thread* thread) {
    if (!thread || thread->state == ThreadState::Invalid) {
        return;
    }
    
    removeFromThreadGroup(thread);
    
    if (thread->kernelStack) {
        memory::PMM::freePages(
            reinterpret_cast<void*>(thread->kernelStack - KERNEL_STACK_SIZE + memory::PAGE_SIZE),
            KERNEL_STACK_SIZE / memory::PAGE_SIZE
        );
    }
    
    freeTid(thread->tid);
    
    thread->state = ThreadState::Invalid;
    thread->tid = 0;
    thread->active = false;
    thread->kernelStack = 0;
    
    sThreadCount--;
}

void ThreadManager::exitThread(i32 exitCode) {
    Thread* current = currentThread();
    if (current) {
        terminateThread(current, exitCode);
    }
}

Thread* ThreadManager::getThread(u32 tid) {
    if (tid == 0 || tid >= MAX_TOTAL_THREADS) {
        return nullptr;
    }
    
    Thread* thread = &sThreads[tid % MAX_TOTAL_THREADS];
    if (thread->tid == tid && thread->state != ThreadState::Invalid) {
        return thread;
    }
    
    return nullptr;
}

Thread* ThreadManager::currentThread() {
    return sCurrentThread;
}

void ThreadManager::setCurrentThread(Thread* thread) {
    sCurrentThread = thread;
}

ThreadGroup* ThreadManager::getThreadGroup(u32 tgid) {
    if (tgid == 0 || tgid >= 256) {
        return nullptr;
    }
    
    ThreadGroup* group = &sThreadGroups[tgid % 256];
    if (group->tgid == tgid && group->active) {
        return group;
    }
    
    return nullptr;
}

ThreadGroup* ThreadManager::createThreadGroup(u32 pid) {
    if (pid == 0 || pid >= 256) {
        return nullptr;
    }
    
    ThreadGroup* group = &sThreadGroups[pid % 256];
    
    if (group->active) {
        return group;
    }
    
    memset(group, 0, sizeof(ThreadGroup));
    
    group->tgid = pid;
    group->leader = nullptr;
    group->threads = nullptr;
    group->threadCount = 0;
    group->activeCount = 0;
    group->active = true;
    
    return group;
}

void ThreadManager::destroyThreadGroup(ThreadGroup* group) {
    if (!group || !group->active) {
        return;
    }
    
    Thread* thread = group->threads;
    while (thread) {
        Thread* next = thread->threadGroupNext;
        destroyThread(thread);
        thread = next;
    }
    
    group->tgid = 0;
    group->leader = nullptr;
    group->threads = nullptr;
    group->threadCount = 0;
    group->activeCount = 0;
    group->active = false;
}

void ThreadManager::addToThreadGroup(Thread* thread, ThreadGroup* group) {
    if (!thread || !group) {
        return;
    }
    
    thread->threadGroupNext = group->threads;
    group->threads = thread;
    group->threadCount++;
    group->activeCount++;
}

void ThreadManager::removeFromThreadGroup(Thread* thread) {
    if (!thread) {
        return;
    }
    
    ThreadGroup* group = getThreadGroup(thread->pid);
    if (!group) {
        return;
    }
    
    Thread* prev = nullptr;
    Thread* curr = group->threads;
    
    while (curr) {
        if (curr == thread) {
            if (prev) {
                prev->threadGroupNext = curr->threadGroupNext;
            } else {
                group->threads = curr->threadGroupNext;
            }
            group->threadCount--;
            break;
        }
        prev = curr;
        curr = curr->threadGroupNext;
    }
    
    thread->threadGroupNext = nullptr;
}

u32 ThreadManager::allocateTid() {
    for (u32 i = 0; i < MAX_TOTAL_THREADS; i++) {
        u32 tid = sNextTid;
        sNextTid = (sNextTid % (MAX_TOTAL_THREADS - 1)) + 1;
        
        if (sThreads[tid].state == ThreadState::Invalid) {
            return tid;
        }
    }
    
    return 0;
}

void ThreadManager::freeTid(u32) {
}

u32 ThreadManager::threadCount() {
    return sThreadCount;
}

u32 ThreadManager::threadCountForProcess(u32 pid) {
    ThreadGroup* group = getThreadGroup(pid);
    if (group) {
        return group->threadCount;
    }
    return 0;
}

void ThreadManager::setTLS(Thread* thread, u64 tlsBase) {
    if (!thread) {
        return;
    }
    
    thread->tls.fsBase = tlsBase;
    
    asm volatile(
        "wrfsbase %0"
        :
        : "r"(tlsBase)
    );
}

u64 ThreadManager::getTLS(Thread* thread) {
    if (!thread) {
        return 0;
    }
    return thread->tls.fsBase;
}

bool ThreadManager::isInitialized() {
    return sInitialized;
}

void ThreadManager::setupKernelStack(Thread* thread) {
    void* stack = memory::PMM::allocatePages(KERNEL_STACK_SIZE / memory::PAGE_SIZE);
    if (stack) {
        thread->kernelStack = reinterpret_cast<u64>(stack) + KERNEL_STACK_SIZE - 8;
    }
}

void ThreadManager::setupThreadStack(Thread* thread, void* entry, void* arg) {
    Process* proc = PM::getProcess(thread->pid);
    if (!proc) {
        return;
    }
    
    u64 stackBottom = memory::USER_STACK_TOP - (threadCountForProcess(thread->pid) + 1) * THREAD_STACK_SIZE;
    
    memory::VMM::allocateUserPages(
        proc->pageTable,
        stackBottom,
        THREAD_STACK_SIZE / memory::PAGE_SIZE,
        memory::PAGE_PRESENT | memory::PAGE_WRITABLE | memory::PAGE_USER
    );
    
    thread->stackBase = stackBottom;
    thread->stackSize = THREAD_STACK_SIZE;
    thread->userStack = stackBottom + THREAD_STACK_SIZE - 8;
    
    thread->context.rsp = thread->userStack;
    thread->context.rip = reinterpret_cast<u64>(entry);
    thread->context.rdi = reinterpret_cast<u64>(arg);
}

Thread* ThreadManager::createThread(Process* proc, u32 flags, u64 stackAddr, u64 tlsAddr, u32* parentTid, u32* childTid) {
    if (!sInitialized || !proc) {
        return nullptr;
    }
    
    Thread* current = currentThread();
    if (!current) {
        return nullptr;
    }
    
    u32 tid = allocateTid();
    if (tid == 0) {
        return nullptr;
    }
    
    Thread* thread = &sThreads[tid % MAX_TOTAL_THREADS];
    
    *thread = *current;
    
    thread->tid = tid;
    thread->pid = proc->pid;
    thread->state = ThreadState::Ready;
    thread->exitCode = 0;
    thread->cpuTime = 0;
    thread->next = nullptr;
    thread->prev = nullptr;
    thread->threadGroupNext = nullptr;
    thread->active = true;
    
    setupKernelStack(thread);
    
    if (stackAddr) {
        thread->userStack = stackAddr;
        thread->context.rsp = stackAddr;
    }
    
    if (flags & CLONE_SETTLS) {
        thread->tls.fsBase = tlsAddr;
        thread->tlsBase = tlsAddr;
    }
    
    if ((flags & CLONE_PARENT_SETTID) && parentTid) {
        *parentTid = tid;
    }
    
    if ((flags & CLONE_CHILD_SETTID) && childTid) {
        thread->setChildTid = childTid;
        *childTid = tid;
    }
    
    if ((flags & CLONE_CHILD_CLEARTID) && childTid) {
        thread->clearChildTid = childTid;
    }
    
    thread->context.rax = 0;
    thread->process = proc;
    
    if (flags & CLONE_THREAD) {
        ThreadGroup* group = getThreadGroup(proc->pid);
        if (group) {
            addToThreadGroup(thread, group);
        }
    }
    
    sThreadCount++;
    
    return thread;
}

void ThreadManager::exitThreadGroup(Process* proc, i32 exitCode) {
    if (!proc) {
        return;
    }
    
    ThreadGroup* group = getThreadGroup(proc->pid);
    if (!group) {
        return;
    }
    
    Thread* thread = group->threads;
    while (thread) {
        Thread* next = thread->threadGroupNext;
        if (thread->state != ThreadState::Zombie && thread->state != ThreadState::Invalid) {
            terminateThread(thread, exitCode);
        }
        thread = next;
    }
}

}
