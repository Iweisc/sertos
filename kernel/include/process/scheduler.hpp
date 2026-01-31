#pragma once

#include "../types.hpp"
#include "process.hpp"
#include "../cpu/idt.hpp"

namespace sertos::process {

constexpr u64 SCHEDULER_TICK_MS = 10;
constexpr u64 DEFAULT_TIME_SLICE = 5;

class Scheduler {
public:
    static void initialize();
    
    static void addProcess(Process* proc);
    static void removeProcess(Process* proc);
    
    static void schedule();
    static void yield();
    
    static void sleep(u64 milliseconds);
    static void wakeup(Process* proc);
    
    static void blockProcess(Process* proc);
    static void unblockProcess(Process* proc);
    
    static void tick();
    static u64 systemTime();
    
    static void start();
    static void stop();
    
    static bool isRunning();
    static bool isInitialized();

private:
    static void timerHandler(cpu::InterruptFrame* frame);
    static void switchTo(Process* next);
    static Process* selectNext();
    static void updateSleepingProcesses();
    
    static Process* sReadyQueueHead;
    static Process* sReadyQueueTail;
    static Process* sSleepingList;
    static Process* sIdleProcess;
    
    static u64 sSystemTicks;
    static u64 sCurrentTimeSlice;
    static bool sRunning;
    static bool sInitialized;
    static bool sNeedReschedule;
};

extern "C" void context_switch(CpuContext* oldContext, CpuContext* newContext);
extern "C" void enter_usermode(u64 rip, u64 rsp, u64 rflags);

}
