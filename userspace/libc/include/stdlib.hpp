#pragma once

#include "types.hpp"

namespace sertos::libc {

constexpr i32 EXIT_SUCCESS = 0;
constexpr i32 EXIT_FAILURE = 1;

constexpr i32 RAND_MAX = 0x7FFFFFFF;

void* malloc(usize size);
void free(void* ptr);
void* calloc(usize nmemb, usize size);
void* realloc(void* ptr, usize size);
void* aligned_alloc(usize alignment, usize size);
i32 posix_memalign(void** memptr, usize alignment, usize size);

[[noreturn]] void exit(i32 status);
[[noreturn]] void _Exit(i32 status);
[[noreturn]] void abort();
i32 atexit(void (*function)());
i32 at_quick_exit(void (*function)());
[[noreturn]] void quick_exit(i32 status);

i32 atoi(const char* str);
i64 atol(const char* str);
i64 atoll(const char* str);
double atof(const char* str);

i64 strtol(const char* str, char** endptr, i32 base);
u64 strtoul(const char* str, char** endptr, i32 base);
i64 strtoll(const char* str, char** endptr, i32 base);
u64 strtoull(const char* str, char** endptr, i32 base);

i32 rand();
void srand(u32 seed);
i32 rand_r(u32* seedp);

char* getenv(const char* name);
i32 setenv(const char* name, const char* value, i32 overwrite);
i32 unsetenv(const char* name);
i32 putenv(char* string);
i32 clearenv();

i32 system(const char* command);

void qsort(void* base, usize nmemb, usize size, i32 (*compar)(const void*, const void*));
void* bsearch(const void* key, const void* base, usize nmemb, usize size, 
              i32 (*compar)(const void*, const void*));

i32 abs(i32 n);
i64 labs(i64 n);
i64 llabs(i64 n);

struct div_t {
    i32 quot;
    i32 rem;
};

struct ldiv_t {
    i64 quot;
    i64 rem;
};

struct lldiv_t {
    i64 quot;
    i64 rem;
};

div_t div(i32 numer, i32 denom);
ldiv_t ldiv(i64 numer, i64 denom);
lldiv_t lldiv(i64 numer, i64 denom);

char* realpath(const char* path, char* resolved_path);
char* mktemp(char* templ);
i32 mkstemp(char* templ);
char* mkdtemp(char* templ);

void* sbrk(ssize_t increment);

i32 mblen(const char* s, usize n);
i32 mbtowc(i32* pwc, const char* s, usize n);
i32 wctomb(char* s, i32 wc);
usize mbstowcs(i32* dest, const char* src, usize n);
usize wcstombs(char* dest, const i32* src, usize n);

}
