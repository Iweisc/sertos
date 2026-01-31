#pragma once

#include "types.hpp"

namespace sertos::libc {

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

constexpr u64 STDOUT_FD = 1;
constexpr u64 STDERR_FD = 2;

inline i64 syscall0(u64 num) {
    i64 ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

inline i64 syscall1(u64 num, u64 arg1) {
    i64 ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

inline i64 syscall2(u64 num, u64 arg1, u64 arg2) {
    i64 ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

inline i64 syscall3(u64 num, u64 arg1, u64 arg2, u64 arg3) {
    i64 ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

inline i64 syscall4(u64 num, u64 arg1, u64 arg2, u64 arg3, u64 arg4) {
    i64 ret;
    register u64 r10 asm("r10") = arg4;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

inline i64 syscall6(u64 num, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6) {
    i64 ret;
    register u64 r10 asm("r10") = arg4;
    register u64 r8 asm("r8") = arg5;
    register u64 r9 asm("r9") = arg6;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

}
