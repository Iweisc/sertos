#pragma once

#include "types.hpp"

namespace sertos::libc {

void* malloc(usize size);
void free(void* ptr);
void* calloc(usize nmemb, usize size);
void* realloc(void* ptr, usize size);

[[noreturn]] void exit(i32 status);
[[noreturn]] void abort();

i32 atoi(const char* str);
i64 atol(const char* str);

void* sbrk(ssize_t increment);

}
