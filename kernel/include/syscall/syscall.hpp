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

constexpr u64 SYS_GETUID = 15;
constexpr u64 SYS_GETGID = 16;
constexpr u64 SYS_SETUID = 17;
constexpr u64 SYS_SETGID = 18;
constexpr u64 SYS_GETEUID = 19;
constexpr u64 SYS_GETEGID = 20;
constexpr u64 SYS_SETEUID = 21;
constexpr u64 SYS_SETEGID = 22;

constexpr u64 SYS_PIPE = 23;
constexpr u64 SYS_DUP = 24;
constexpr u64 SYS_DUP2 = 25;
constexpr u64 SYS_LSEEK = 26;
constexpr u64 SYS_STAT = 27;
constexpr u64 SYS_FSTAT = 28;
constexpr u64 SYS_MKDIR = 29;
constexpr u64 SYS_RMDIR = 30;
constexpr u64 SYS_UNLINK = 31;
constexpr u64 SYS_RENAME = 32;
constexpr u64 SYS_CHDIR = 33;
constexpr u64 SYS_GETCWD = 34;

constexpr u64 SYS_KILL = 35;
constexpr u64 SYS_SIGNAL = 36;
constexpr u64 SYS_SIGACTION = 37;
constexpr u64 SYS_SIGPROCMASK = 38;
constexpr u64 SYS_SIGSUSPEND = 39;

constexpr u64 SYS_SHMGET = 40;
constexpr u64 SYS_SHMAT = 41;
constexpr u64 SYS_SHMDT = 42;
constexpr u64 SYS_SHMCTL = 43;

constexpr u64 SYS_MSGGET = 44;
constexpr u64 SYS_MSGSND = 45;
constexpr u64 SYS_MSGRCV = 46;
constexpr u64 SYS_MSGCTL = 47;

constexpr u64 SYS_SOCKET = 48;
constexpr u64 SYS_BIND = 49;
constexpr u64 SYS_LISTEN = 50;
constexpr u64 SYS_ACCEPT = 51;
constexpr u64 SYS_CONNECT = 52;
constexpr u64 SYS_SEND = 53;
constexpr u64 SYS_RECV = 54;
constexpr u64 SYS_SHUTDOWN = 55;

constexpr u64 SYS_MPROTECT = 56;
constexpr u64 SYS_GETPPID = 57;
constexpr u64 SYS_GETPGID = 58;
constexpr u64 SYS_SETPGID = 59;
constexpr u64 SYS_SETSID = 60;
constexpr u64 SYS_GETSID = 61;

constexpr u64 SYS_IOCTL = 62;
constexpr u64 SYS_FCNTL = 63;

constexpr u64 SYS_CLOCK_GETTIME = 64;
constexpr u64 SYS_CLOCK_SETTIME = 65;
constexpr u64 SYS_NANOSLEEP = 66;

constexpr u64 SYS_GETRLIMIT = 67;
constexpr u64 SYS_SETRLIMIT = 68;
constexpr u64 SYS_GETRUSAGE = 69;

constexpr u64 SYS_UNAME = 70;
constexpr u64 SYS_SYSINFO = 71;

constexpr u64 SYS_PRCTL = 72;
constexpr u64 SYS_CAPGET = 73;
constexpr u64 SYS_CAPSET = 74;

constexpr u64 SYS_REBOOT = 75;
constexpr u64 SYS_SYNC = 76;

constexpr u64 SYS_CLONE = 77;
constexpr u64 SYS_FUTEX = 78;
constexpr u64 SYS_SET_TID_ADDRESS = 79;
constexpr u64 SYS_GETTID = 80;
constexpr u64 SYS_TKILL = 81;
constexpr u64 SYS_TGKILL = 82;
constexpr u64 SYS_EXIT_GROUP = 83;
constexpr u64 SYS_SET_ROBUST_LIST = 84;
constexpr u64 SYS_GET_ROBUST_LIST = 85;

constexpr u64 SYS_EPOLL_CREATE = 86;
constexpr u64 SYS_EPOLL_CREATE1 = 87;
constexpr u64 SYS_EPOLL_CTL = 88;
constexpr u64 SYS_EPOLL_WAIT = 89;
constexpr u64 SYS_EPOLL_PWAIT = 90;
constexpr u64 SYS_POLL = 91;
constexpr u64 SYS_PPOLL = 92;
constexpr u64 SYS_SELECT = 93;
constexpr u64 SYS_PSELECT6 = 94;

constexpr u64 SYS_EVENTFD = 95;
constexpr u64 SYS_EVENTFD2 = 96;
constexpr u64 SYS_TIMERFD_CREATE = 97;
constexpr u64 SYS_TIMERFD_SETTIME = 98;
constexpr u64 SYS_TIMERFD_GETTIME = 99;
constexpr u64 SYS_SIGNALFD = 100;
constexpr u64 SYS_SIGNALFD4 = 101;

constexpr u64 SYS_OPENAT = 102;
constexpr u64 SYS_MKDIRAT = 103;
constexpr u64 SYS_UNLINKAT = 104;
constexpr u64 SYS_RENAMEAT = 105;
constexpr u64 SYS_FSTATAT = 106;
constexpr u64 SYS_READLINKAT = 107;
constexpr u64 SYS_FACCESSAT = 108;
constexpr u64 SYS_FCHMODAT = 109;
constexpr u64 SYS_FCHOWNAT = 110;

constexpr u64 SYS_MREMAP = 111;
constexpr u64 SYS_MADVISE = 112;
constexpr u64 SYS_MINCORE = 113;
constexpr u64 SYS_MEMFD_CREATE = 114;

constexpr u64 SYS_GETRANDOM = 115;
constexpr u64 SYS_PRLIMIT64 = 116;
constexpr u64 SYS_WAITID = 117;

constexpr u64 SYS_SENDTO = 118;
constexpr u64 SYS_RECVFROM = 119;
constexpr u64 SYS_SETSOCKOPT = 120;
constexpr u64 SYS_GETSOCKOPT = 121;
constexpr u64 SYS_GETSOCKNAME = 122;
constexpr u64 SYS_GETPEERNAME = 123;
constexpr u64 SYS_SOCKETPAIR = 124;

constexpr u64 SYS_ARCH_PRCTL = 125;
constexpr u64 SYS_READV = 126;
constexpr u64 SYS_WRITEV = 127;

constexpr u64 SYSCALL_MAX = 256;

constexpr i64 ENOSYS = -38;
constexpr i64 EINVAL = -22;
constexpr i64 ENOMEM = -12;
constexpr i64 EBADF = -9;
constexpr i64 EFAULT = -14;
constexpr i64 ECHILD = -10;
constexpr i64 EPERM = -1;
constexpr i64 ENOENT = -2;
constexpr i64 ESRCH = -3;
constexpr i64 EINTR = -4;
constexpr i64 EIO = -5;
constexpr i64 ENXIO = -6;
constexpr i64 E2BIG = -7;
constexpr i64 ENOEXEC = -8;
constexpr i64 EAGAIN = -11;
constexpr i64 EACCES = -13;
constexpr i64 EBUSY = -16;
constexpr i64 EEXIST = -17;
constexpr i64 EXDEV = -18;
constexpr i64 ENODEV = -19;
constexpr i64 ENOTDIR = -20;
constexpr i64 EISDIR = -21;
constexpr i64 ENFILE = -23;
constexpr i64 EMFILE = -24;
constexpr i64 ENOTTY = -25;
constexpr i64 ETXTBSY = -26;
constexpr i64 EFBIG = -27;
constexpr i64 ENOSPC = -28;
constexpr i64 ESPIPE = -29;
constexpr i64 EROFS = -30;
constexpr i64 EMLINK = -31;
constexpr i64 EPIPE = -32;
constexpr i64 EDOM = -33;
constexpr i64 ERANGE = -34;
constexpr i64 EDEADLK = -35;
constexpr i64 ENAMETOOLONG = -36;
constexpr i64 ENOLCK = -37;
constexpr i64 ENOTEMPTY = -39;
constexpr i64 ELOOP = -40;
constexpr i64 EWOULDBLOCK = EAGAIN;

constexpr u64 STDIN_FD = 0;
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
