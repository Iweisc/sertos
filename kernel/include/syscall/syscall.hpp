#pragma once

#include "../types.hpp"
#include "../cpu/idt.hpp"

namespace sertos::syscall {

constexpr u64 SYS_EXIT = 0;
constexpr u64 SYS_WRITE = 1;
constexpr u64 SYS_READ = 2;
constexpr u64 SYS_OPEN = 3;
constexpr u64 SYS_CLOSE = 4;
constexpr u64 SYS_MMAP = 5;
constexpr u64 SYS_MUNMAP = 6;
constexpr u64 SYS_BRK = 7;
constexpr u64 SYS_GETPID = 8;
constexpr u64 SYS_FORK = 9;
constexpr u64 SYS_EXEC = 10;
constexpr u64 SYS_WAIT = 11;
constexpr u64 SYS_YIELD = 12;
constexpr u64 SYS_SLEEP = 13;
constexpr u64 SYS_GETTIME = 14;

constexpr u64 SYSCALL_MAX = 32;

constexpr i64 ENOSYS = -38;
constexpr i64 EINVAL = -22;
constexpr i64 ENOMEM = -12;
constexpr i64 EBADF = -9;
constexpr i64 EFAULT = -14;
constexpr i64 ECHILD = -10;

constexpr u64 STDOUT_FD = 1;
constexpr u64 STDERR_FD = 2;

struct SyscallFrame {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 interruptNumber, errorCode;
    u64 rip, cs, rflags, rsp, ss;
};

using SyscallHandler = i64 (*)(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6);

class Syscall {
public:
    static void initialize();
    static void registerHandler(u64 number, SyscallHandler handler);
    static i64 dispatch(u64 number, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6);

private:
    static void syscallInterruptHandler(cpu::InterruptFrame* frame);
    static void setupSyscallMSR();
    
    static SyscallHandler sHandlers[SYSCALL_MAX];
    static bool sInitialized;
};

extern "C" void syscall_entry();
extern "C" i64 syscall_handler_asm(u64 number, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6);

void registerAllHandlers();

}
