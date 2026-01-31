#pragma once

#include "../types.hpp"
#include "../memory/vmm.hpp"

namespace sertos::process {

constexpr u32 MAX_PROCESSES = 256;
constexpr u32 PROCESS_NAME_MAX = 64;

enum class ProcessState : u8 {
    Invalid = 0,
    Ready,
    Running,
    Blocked,
    Sleeping,
    Zombie,
    Terminated
};

struct CpuContext {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} __attribute__((packed));

struct Process {
    u32 pid;
    u32 parentPid;
    ProcessState state;
    i32 exitCode;
    
    char name[PROCESS_NAME_MAX];
    
    memory::PageTable* pageTable;
    
    u64 kernelStack;
    u64 userStack;
    u64 heapStart;
    u64 heapEnd;
    u64 programBreak;
    
    CpuContext context;
    
    u64 sleepUntil;
    
    u64 cpuTime;
    u64 startTime;
    
    Process* next;
    Process* prev;
};

class ProcessManager {
public:
    static void initialize();
    
    static Process* createProcess(const char* name);
    static Process* createKernelProcess(const char* name, void (*entry)());
    static Process* forkProcess(Process* parent);
    static void terminateProcess(Process* proc, i32 exitCode);
    static void destroyProcess(Process* proc);
    
    static Process* getProcess(u32 pid);
    static Process* currentProcess();
    static void setCurrentProcess(Process* proc);
    
    static u32 allocatePid();
    static void freePid(u32 pid);
    
    static u32 processCount();
    static bool isInitialized();

private:
    static void setupKernelStack(Process* proc);
    static void setupUserStack(Process* proc);
    
    static Process sProcesses[MAX_PROCESSES];
    static Process* sCurrentProcess;
    static u32 sNextPid;
    static u32 sProcessCount;
    static bool sInitialized;
};

using PM = ProcessManager;

}
