#pragma once

#include "types.hpp"

namespace sertos::libc {

usize strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, usize n);
i32 strcmp(const char* s1, const char* s2);
i32 strncmp(const char* s1, const char* s2, usize n);
char* strcat(char* dest, const char* src);
char* strchr(const char* s, i32 c);
char* strrchr(const char* s, i32 c);

void* memcpy(void* dest, const void* src, usize n);
void* memmove(void* dest, const void* src, usize n);
void* memset(void* s, i32 c, usize n);
i32 memcmp(const void* s1, const void* s2, usize n);

}
