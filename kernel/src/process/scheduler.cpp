#include "../../include/process/scheduler.hpp"
#include "../../include/cpu/gdt.hpp"
#include "../../include/cpu/pic.hpp"
#include "../../include/memory/vmm.hpp"

namespace sertos::process {

Process* Scheduler::sReadyQueueHead = nullptr;
Process* Scheduler::sReadyQueueTail = nullptr;
Process* Scheduler::sSleepingList = nullptr;
Process* Scheduler::sIdleProcess = nullptr;

u64 Scheduler::sSystemTicks = 0;
u64 Scheduler::sCurrentTimeSlice = 0;
bool Scheduler::sRunning = false;
bool Scheduler::sInitialized = false;
bool Scheduler::sNeedReschedule = false;

static void idleTask() {
    while (true) {
        asm volatile("hlt");
    }
}

void Scheduler::initialize() {
    if (sInitialized) {
        return;
    }
    
    sReadyQueueHead = nullptr;
    sReadyQueueTail = nullptr;
    sSleepingList = nullptr;
    sSystemTicks = 0;
    sCurrentTimeSlice = DEFAULT_TIME_SLICE;
    sRunning = false;
    sNeedReschedule = false;
    
    sIdleProcess = PM::createKernelProcess("idle", idleTask);
    if (sIdleProcess) {
        sIdleProcess->state = ProcessState::Ready;
    }
    
    cpu::IDT::setHandler(cpu::IRQ_TIMER, timerHandler);
    
    sInitialized = true;
}

void Scheduler::addProcess(Process* proc) {
    if (!proc || proc->state == ProcessState::Invalid) {
        return;
    }
    
    proc->state = ProcessState::Ready;
    proc->next = nullptr;
    proc->prev = sReadyQueueTail;
    
    if (sReadyQueueTail) {
        sReadyQueueTail->next = proc;
    } else {
        sReadyQueueHead = proc;
    }
    sReadyQueueTail = proc;
}

void Scheduler::removeProcess(Process* proc) {
    if (!proc) {
        return;
    }
    
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        sReadyQueueHead = proc->next;
    }
    
    if (proc->next) {
        proc->next->prev = proc->prev;
    } else {
        sReadyQueueTail = proc->prev;
    }
    
    proc->next = nullptr;
    proc->prev = nullptr;
}

void Scheduler::schedule() {
    if (!sInitialized || !sRunning) {
        return;
    }
    
    Process* current = PM::currentProcess();
    Process* next = selectNext();
    
    if (!next) {
        next = sIdleProcess;
    }
    
    if (next == current) {
        return;
    }
    
    if (current && current->state == ProcessState::Running) {
        current->state = ProcessState::Ready;
        addProcess(current);
    }
    
    switchTo(next);
}

void Scheduler::yield() {
    Process* current = PM::currentProcess();
    if (current) {
        current->state = ProcessState::Ready;
        addProcess(current);
    }
    schedule();
}

void Scheduler::sleep(u64 milliseconds) {
    Process* current = PM::currentProcess();
    if (!current) {
        return;
    }
    
    current->sleepUntil = sSystemTicks + (milliseconds / SCHEDULER_TICK_MS);
    current->state = ProcessState::Sleeping;
    
    current->next = sSleepingList;
    if (sSleepingList) {
        sSleepingList->prev = current;
    }
    sSleepingList = current;
    current->prev = nullptr;
    
    schedule();
}

void Scheduler::wakeup(Process* proc) {
    if (!proc || proc->state != ProcessState::Sleeping) {
        return;
    }
    
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        sSleepingList = proc->next;
    }
    
    if (proc->next) {
        proc->next->prev = proc->prev;
    }
    
    proc->next = nullptr;
    proc->prev = nullptr;
    
    addProcess(proc);
}

void Scheduler::blockProcess(Process* proc) {
    if (!proc) {
        return;
    }
    
    proc->state = ProcessState::Blocked;
    removeProcess(proc);
    
    if (proc == PM::currentProcess()) {
        schedule();
    }
}

void Scheduler::unblockProcess(Process* proc) {
    if (!proc || proc->state != ProcessState::Blocked) {
        return;
    }
    
    addProcess(proc);
}

void Scheduler::tick() {
    sSystemTicks++;
    
    updateSleepingProcesses();
    
    Process* current = PM::currentProcess();
    if (current && current != sIdleProcess) {
        current->cpuTime++;
        sCurrentTimeSlice--;
        
        if (sCurrentTimeSlice == 0) {
            sCurrentTimeSlice = DEFAULT_TIME_SLICE;
            sNeedReschedule = true;
        }
    }
}

u64 Scheduler::systemTime() {
    return sSystemTicks * SCHEDULER_TICK_MS;
}

void Scheduler::start() {
    if (!sInitialized) {
        return;
    }
    
    sRunning = true;
    
    Process* first = selectNext();
    if (!first) {
        first = sIdleProcess;
    }
    
    if (first) {
        switchTo(first);
    }
}

void Scheduler::stop() {
    sRunning = false;
}

bool Scheduler::isRunning() {
    return sRunning;
}

bool Scheduler::isInitialized() {
    return sInitialized;
}

void Scheduler::timerHandler(cpu::InterruptFrame* frame) {
    cpu::PIC::sendEOI(0);
    
    tick();
    
    if (sNeedReschedule && sRunning) {
        sNeedReschedule = false;
        
        Process* current = PM::currentProcess();
        if (current && current->state == ProcessState::Running) {
            current->context.rax = frame->rax;
            current->context.rbx = frame->rbx;
            current->context.rcx = frame->rcx;
            current->context.rdx = frame->rdx;
            current->context.rsi = frame->rsi;
            current->context.rdi = frame->rdi;
            current->context.rbp = frame->rbp;
            current->context.r8 = frame->r8;
            current->context.r9 = frame->r9;
            current->context.r10 = frame->r10;
            current->context.r11 = frame->r11;
            current->context.r12 = frame->r12;
            current->context.r13 = frame->r13;
            current->context.r14 = frame->r14;
            current->context.r15 = frame->r15;
            current->context.rip = frame->rip;
            current->context.rsp = frame->rsp;
            current->context.rflags = frame->rflags;
            current->context.cs = frame->cs;
            current->context.ss = frame->ss;
        }
        
        schedule();
    }
}

void Scheduler::switchTo(Process* next) {
    if (!next) {
        return;
    }
    
    Process* current = PM::currentProcess();
    
    next->state = ProcessState::Running;
    removeProcess(next);
    PM::setCurrentProcess(next);
    
    cpu::GDT::setKernelStack(next->kernelStack);
    
    if (next->pageTable != memory::VMM::currentPageTable()) {
        memory::VMM::switchPageTable(next->pageTable);
    }
    
    if (current) {
        context_switch(&current->context, &next->context);
    } else {
        CpuContext dummy;
        context_switch(&dummy, &next->context);
    }
}

Process* Scheduler::selectNext() {
    Process* next = sReadyQueueHead;
    
    if (next) {
        removeProcess(next);
    }
    
    return next;
}

void Scheduler::updateSleepingProcesses() {
    Process* proc = sSleepingList;
    
    while (proc) {
        Process* next = proc->next;
        
        if (sSystemTicks >= proc->sleepUntil) {
            wakeup(proc);
        }
        
        proc = next;
    }
}

}
