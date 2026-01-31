#include "../../include/syscall/syscall.hpp"
#include "../../include/cpu/gdt.hpp"
#include "../../include/graphics/console.hpp"

namespace sertos::syscall {

SyscallHandler Syscall::sHandlers[SYSCALL_MAX];
bool Syscall::sInitialized = false;

constexpr u32 MSR_EFER = 0xC0000080;
constexpr u32 MSR_STAR = 0xC0000081;
constexpr u32 MSR_LSTAR = 0xC0000082;
constexpr u32 MSR_SFMASK = 0xC0000084;

constexpr u64 EFER_SCE = 1ULL << 0;

static inline u64 rdmsr(u32 msr) {
    u32 low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return (static_cast<u64>(high) << 32) | low;
}

static inline void wrmsr(u32 msr, u64 value) {
    u32 low = value & 0xFFFFFFFF;
    u32 high = value >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

void Syscall::initialize() {
    if (sInitialized) {
        return;
    }
    
    for (u64 i = 0; i < SYSCALL_MAX; i++) {
        sHandlers[i] = nullptr;
    }
    
    cpu::IDT::setHandler(0x80, syscallInterruptHandler);
    
    setupSyscallMSR();
    
    sInitialized = true;
}

void Syscall::setupSyscallMSR() {
    u64 efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);
    
    u64 star = (static_cast<u64>(cpu::KERNEL_CODE_SELECTOR) << 32) |
               (static_cast<u64>(cpu::USER_CODE_SELECTOR - 16) << 48);
    wrmsr(MSR_STAR, star);
    
    wrmsr(MSR_LSTAR, reinterpret_cast<u64>(syscall_entry));
    
    wrmsr(MSR_SFMASK, 0x200);
}

void Syscall::registerHandler(u64 number, SyscallHandler handler) {
    if (number < SYSCALL_MAX) {
        sHandlers[number] = handler;
    }
}

i64 Syscall::dispatch(u64 number, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6) {
    if (number >= SYSCALL_MAX || !sHandlers[number]) {
        return ENOSYS;
    }
    
    return sHandlers[number](arg1, arg2, arg3, arg4, arg5, arg6);
}

void Syscall::syscallInterruptHandler(cpu::InterruptFrame* frame) {
    u64 syscallNum = frame->rax;
    u64 arg1 = frame->rdi;
    u64 arg2 = frame->rsi;
    u64 arg3 = frame->rdx;
    u64 arg4 = frame->r10;
    u64 arg5 = frame->r8;
    u64 arg6 = frame->r9;
    
    i64 result = dispatch(syscallNum, arg1, arg2, arg3, arg4, arg5, arg6);
    
    frame->rax = static_cast<u64>(result);
}

extern "C" i64 syscall_handler_asm(u64 number, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6) {
    return Syscall::dispatch(number, arg1, arg2, arg3, arg4, arg5, arg6);
}

}
