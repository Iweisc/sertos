#include "../include/unistd.hpp"
#include "../include/syscall.hpp"

namespace sertos::libc {

pid_t getpid() {
    return static_cast<pid_t>(syscall0(SYS_GETPID));
}

pid_t fork() {
    return static_cast<pid_t>(syscall0(SYS_FORK));
}

i32 execve(const char* pathname, char* const argv[], char* const envp[]) {
    return static_cast<i32>(syscall3(SYS_EXEC, 
        reinterpret_cast<u64>(pathname), 
        reinterpret_cast<u64>(argv), 
        reinterpret_cast<u64>(envp)));
}

pid_t wait(i32* status) {
    return static_cast<pid_t>(syscall1(SYS_WAIT, reinterpret_cast<u64>(status)));
}

[[noreturn]] void _exit(i32 status) {
    syscall1(SYS_EXIT, status);
    __builtin_unreachable();
}

u32 sleep(u32 seconds) {
    syscall1(SYS_SLEEP, seconds * 1000);
    return 0;
}

i32 usleep(u64 usec) {
    syscall1(SYS_SLEEP, usec / 1000);
    return 0;
}

void yield() {
    syscall0(SYS_YIELD);
}

ssize_t read(i32 fd, void* buf, usize count) {
    return syscall3(SYS_READ, fd, reinterpret_cast<u64>(buf), count);
}

ssize_t write(i32 fd, const void* buf, usize count) {
    return syscall3(SYS_WRITE, fd, reinterpret_cast<u64>(buf), count);
}

i32 close(i32 fd) {
    return static_cast<i32>(syscall1(SYS_CLOSE, fd));
}

void* brk(void* addr) {
    return reinterpret_cast<void*>(syscall1(SYS_BRK, reinterpret_cast<u64>(addr)));
}

}
