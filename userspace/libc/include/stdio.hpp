#pragma once

#include "types.hpp"

namespace sertos::libc {

constexpr i32 EOF = -1;

i32 putchar(i32 c);
i32 puts(const char* s);
i32 printf(const char* format, ...);
i32 sprintf(char* str, const char* format, ...);

ssize_t write(i32 fd, const void* buf, usize count);
ssize_t read(i32 fd, void* buf, usize count);

}
