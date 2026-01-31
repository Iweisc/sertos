#include "../../libc/include/stdio.hpp"
#include "../../libc/include/unistd.hpp"

using namespace sertos::libc;

extern "C" void _start() {
    printf("Hello from userspace!\n");
    printf("My PID is: %d\n", getpid());
    _exit(0);
}
