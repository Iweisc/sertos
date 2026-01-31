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
constexpr u64 SYS_LSEEK = 15;
constexpr u64 SYS_STAT = 16;
constexpr u64 SYS_FSTAT = 17;
constexpr u64 SYS_MKDIR = 18;
constexpr u64 SYS_RMDIR = 19;
constexpr u64 SYS_UNLINK = 20;
constexpr u64 SYS_RENAME = 21;
constexpr u64 SYS_CHDIR = 22;
constexpr u64 SYS_GETCWD = 23;
constexpr u64 SYS_DUP = 24;
constexpr u64 SYS_DUP2 = 25;
constexpr u64 SYS_PIPE = 26;
constexpr u64 SYS_KILL = 27;
constexpr u64 SYS_SIGNAL = 28;
constexpr u64 SYS_SIGACTION = 29;
constexpr u64 SYS_SIGPROCMASK = 30;
constexpr u64 SYS_GETUID = 31;
constexpr u64 SYS_GETGID = 32;
constexpr u64 SYS_SETUID = 33;
constexpr u64 SYS_SETGID = 34;
constexpr u64 SYS_GETEUID = 35;
constexpr u64 SYS_GETEGID = 36;
constexpr u64 SYS_SETEUID = 37;
constexpr u64 SYS_SETEGID = 38;
constexpr u64 SYS_GETPPID = 39;
constexpr u64 SYS_SHMGET = 40;
constexpr u64 SYS_SHMAT = 41;
constexpr u64 SYS_SHMDT = 42;
constexpr u64 SYS_SHMCTL = 43;
constexpr u64 SYS_MSGGET = 44;
constexpr u64 SYS_MSGSND = 45;
constexpr u64 SYS_MSGRCV = 46;
constexpr u64 SYS_MSGCTL = 47;
constexpr u64 SYS_IOCTL = 48;
constexpr u64 SYS_FCNTL = 49;
constexpr u64 SYS_ACCESS = 50;
constexpr u64 SYS_CHMOD = 51;
constexpr u64 SYS_CHOWN = 52;
constexpr u64 SYS_UMASK = 53;
constexpr u64 SYS_LINK = 54;
constexpr u64 SYS_SYMLINK = 55;
constexpr u64 SYS_READLINK = 56;
constexpr u64 SYS_TRUNCATE = 57;
constexpr u64 SYS_FTRUNCATE = 58;
constexpr u64 SYS_SYNC = 59;
constexpr u64 SYS_FSYNC = 60;
constexpr u64 SYS_GETDENTS = 61;
constexpr u64 SYS_SOCKET = 62;
constexpr u64 SYS_BIND = 63;
constexpr u64 SYS_LISTEN = 64;
constexpr u64 SYS_ACCEPT = 65;
constexpr u64 SYS_CONNECT = 66;
constexpr u64 SYS_SEND = 67;
constexpr u64 SYS_RECV = 68;
constexpr u64 SYS_SHUTDOWN_SOCK = 69;
constexpr u64 SYS_SETSOCKOPT = 70;
constexpr u64 SYS_GETSOCKOPT = 71;
constexpr u64 SYS_POLL = 72;
constexpr u64 SYS_SELECT = 73;
constexpr u64 SYS_NANOSLEEP = 74;
constexpr u64 SYS_CLOCK_GETTIME = 75;
constexpr u64 SYS_CLOCK_SETTIME = 76;
constexpr u64 SYS_UNAME = 77;
constexpr u64 SYS_SYSINFO = 78;
constexpr u64 SYS_REBOOT = 79;
constexpr u64 SYS_POWEROFF = 80;

constexpr u64 STDIN_FD = 0;
constexpr u64 STDOUT_FD = 1;
constexpr u64 STDERR_FD = 2;

constexpr i32 O_RDONLY = 0;
constexpr i32 O_WRONLY = 1;
constexpr i32 O_RDWR = 2;
constexpr i32 O_CREAT = 0100;
constexpr i32 O_EXCL = 0200;
constexpr i32 O_TRUNC = 01000;
constexpr i32 O_APPEND = 02000;
constexpr i32 O_NONBLOCK = 04000;
constexpr i32 O_DIRECTORY = 0200000;
constexpr i32 O_CLOEXEC = 02000000;

constexpr i32 SEEK_SET = 0;
constexpr i32 SEEK_CUR = 1;
constexpr i32 SEEK_END = 2;

constexpr i32 F_DUPFD = 0;
constexpr i32 F_GETFD = 1;
constexpr i32 F_SETFD = 2;
constexpr i32 F_GETFL = 3;
constexpr i32 F_SETFL = 4;
constexpr i32 FD_CLOEXEC = 1;

constexpr i32 R_OK = 4;
constexpr i32 W_OK = 2;
constexpr i32 X_OK = 1;
constexpr i32 F_OK = 0;

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
