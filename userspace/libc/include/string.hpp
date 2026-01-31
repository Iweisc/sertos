#pragma once

#include "types.hpp"

namespace sertos::libc {

usize strlen(const char* s);
usize strnlen(const char* s, usize maxlen);

char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, usize n);
usize strlcpy(char* dest, const char* src, usize size);

i32 strcmp(const char* s1, const char* s2);
i32 strncmp(const char* s1, const char* s2, usize n);
i32 strcasecmp(const char* s1, const char* s2);
i32 strncasecmp(const char* s1, const char* s2, usize n);

char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, usize n);
usize strlcat(char* dest, const char* src, usize size);

char* strchr(const char* s, i32 c);
char* strrchr(const char* s, i32 c);
char* strstr(const char* haystack, const char* needle);
char* strtok(char* str, const char* delim);
char* strtok_r(char* str, const char* delim, char** saveptr);

usize strspn(const char* s, const char* accept);
usize strcspn(const char* s, const char* reject);
char* strpbrk(const char* s, const char* accept);

char* strdup(const char* s);
char* strndup(const char* s, usize n);

char* strerror(i32 errnum);
i32 strerror_r(i32 errnum, char* buf, usize buflen);

void* memcpy(void* dest, const void* src, usize n);
void* memmove(void* dest, const void* src, usize n);
void* memset(void* s, i32 c, usize n);
i32 memcmp(const void* s1, const void* s2, usize n);
void* memchr(const void* s, i32 c, usize n);
void* memrchr(const void* s, i32 c, usize n);
void* memmem(const void* haystack, usize haystacklen, const void* needle, usize needlelen);

void bzero(void* s, usize n);
void bcopy(const void* src, void* dest, usize n);
i32 bcmp(const void* s1, const void* s2, usize n);

}
