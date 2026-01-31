#pragma once

#include "types.hpp"

namespace sertos::libc {

pid_t getpid();
pid_t fork();
i32 execve(const char* pathname, char* const argv[], char* const envp[]);
pid_t wait(i32* status);
[[noreturn]] void _exit(i32 status);
u32 sleep(u32 seconds);
i32 usleep(u64 usec);
void yield();

ssize_t read(i32 fd, void* buf, usize count);
ssize_t write(i32 fd, const void* buf, usize count);
i32 close(i32 fd);

void* brk(void* addr);
void* sbrk(ssize_t increment);

}
