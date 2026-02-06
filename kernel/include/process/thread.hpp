#pragma once

#include "../types.hpp"
#include "../memory/vmm.hpp"

namespace sertos::process {

constexpr u32 MAX_THREADS_PER_PROCESS = 64;
constexpr u32 MAX_TOTAL_THREADS = 1024;
constexpr u32 THREAD_NAME_MAX = 32;
constexpr u64 THREAD_STACK_SIZE = 8 * memory::PAGE_SIZE;

constexpr u32 CLONE_VM = 0x00000100;
constexpr u32 CLONE_FS = 0x00000200;
constexpr u32 CLONE_FILES = 0x00000400;
constexpr u32 CLONE_SIGHAND = 0x00000800;
constexpr u32 CLONE_THREAD = 0x00010000;
constexpr u32 CLONE_SYSVSEM = 0x00040000;
constexpr u32 CLONE_SETTLS = 0x00080000;
constexpr u32 CLONE_PARENT_SETTID = 0x00100000;
constexpr u32 CLONE_CHILD_CLEARTID = 0x00200000;
constexpr u32 CLONE_CHILD_SETTID = 0x01000000;

enum class ThreadState : u8 {
    Invalid = 0,
    Ready,
    Running,
    Blocked,
    Sleeping,
    Zombie,
    Terminated
};

struct ThreadLocalStorage {
    u64 base;
    u64 size;
    u64 fsBase;
    u64 gsBase;
};

struct ThreadContext {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} __attribute__((packed));

struct Process;

struct Thread {
    u32 tid;
    u32 pid;
    ThreadState state;
    i32 exitCode;
    
    char name[THREAD_NAME_MAX];
    
    u64 kernelStack;
    u64 userStack;
    u64 stackBase;
    u64 stackSize;
    
    ThreadContext context;
    ThreadLocalStorage tls;
    u64 tlsBase;
    
    u64 sleepUntil;
    u64 cpuTime;
    u64 startTime;
    
    u32* clearChildTid;
    u32* setChildTid;
    
    u64 signalMask;
    u64 pendingSignals;
    
    u64 robustListHead;
    
    Process* process;
    
    Thread* next;
    Thread* prev;
    Thread* threadGroupNext;
    
    bool active;
    bool detached;
    bool inKernel;
};

struct ThreadGroup {
    u32 tgid;
    u32 leaderTid;
    Thread* leader;
    Thread* threads;
    u32 threadCount;
    u32 activeCount;
    bool active;
};

class ThreadManager {
public:
    static void initialize();
    
    static Thread* createThread(u32 pid, void* entry, void* arg, u32 flags);
    static Thread* createThread(Process* proc, u32 flags, u64 stackAddr, u64 tlsAddr, u32* parentTid, u32* childTid);
    static Thread* createMainThread(u32 pid, void* entry);
    static Thread* cloneThread(Thread* parent, u32 flags, void* stack, u32* parentTid, u32* childTid, u64 tls);
    
    static void terminateThread(Thread* thread, i32 exitCode);
    static void destroyThread(Thread* thread);
    static void exitThread(i32 exitCode);
    static void exitThreadGroup(Process* proc, i32 exitCode);
    
    static Thread* getThread(u32 tid);
    static Thread* currentThread();
    static void setCurrentThread(Thread* thread);
    
    static ThreadGroup* getThreadGroup(u32 tgid);
    static ThreadGroup* createThreadGroup(u32 pid);
    static void destroyThreadGroup(ThreadGroup* group);
    
    static void addToThreadGroup(Thread* thread, ThreadGroup* group);
    static void removeFromThreadGroup(Thread* thread);
    
    static u32 allocateTid();
    static void freeTid(u32 tid);
    
    static u32 threadCount();
    static u32 threadCountForProcess(u32 pid);
    
    static void setTLS(Thread* thread, u64 tlsBase);
    static u64 getTLS(Thread* thread);
    
    static bool isInitialized();

private:
    static void setupThreadStack(Thread* thread, void* entry, void* arg);
    static void setupKernelStack(Thread* thread);
    
    static Thread sThreads[MAX_TOTAL_THREADS];
    static ThreadGroup sThreadGroups[256];
    static Thread* sCurrentThread;
    static u32 sNextTid;
    static u32 sThreadCount;
    static bool sInitialized;
};

using TM = ThreadManager;

extern "C" void thread_entry_trampoline();
extern "C" void thread_context_switch(ThreadContext* oldCtx, ThreadContext* newCtx);

}
