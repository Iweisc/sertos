#include "../include/stdlib.hpp"
#include "../include/string.hpp"
#include "../include/syscall.hpp"
#include "../include/unistd.hpp"

namespace sertos::libc {

namespace {

void* heapStart = nullptr;
void* heapEnd = nullptr;
void* programBreak = nullptr;

struct BlockHeader {
    usize size;
    bool free;
    BlockHeader* next;
    BlockHeader* prev;
};

constexpr usize BLOCK_SIZE = sizeof(BlockHeader);
constexpr usize ALIGN_MASK = 15;

BlockHeader* freeList = nullptr;

u32 randSeed = 1;

constexpr usize MAX_ATEXIT_FUNCS = 32;
void (*atexitFuncs[MAX_ATEXIT_FUNCS])();
usize atexitCount = 0;

void (*quickExitFuncs[MAX_ATEXIT_FUNCS])();
usize quickExitCount = 0;

usize alignSize(usize x) {
    return (x + ALIGN_MASK) & ~ALIGN_MASK;
}

usize alignTo(usize x, usize alignment) {
    return (x + alignment - 1) & ~(alignment - 1);
}

BlockHeader* findFreeBlock(usize size) {
    BlockHeader* current = freeList;
    
    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return nullptr;
}

BlockHeader* requestSpace(usize size) {
    auto* block = static_cast<BlockHeader*>(sbrk(0));
    void* request = sbrk(static_cast<ssize_t>(BLOCK_SIZE + size));
    
    if (request == reinterpret_cast<void*>(-1)) {
        return nullptr;
    }
    
    block->size = size;
    block->free = false;
    block->next = nullptr;
    block->prev = nullptr;
    
    if (freeList == nullptr) {
        freeList = block;
    } else {
        BlockHeader* last = freeList;
        while (last->next) {
            last = last->next;
        }
        last->next = block;
        block->prev = last;
    }
    
    return block;
}

void splitBlock(BlockHeader* block, usize size) {
    if (block->size >= size + BLOCK_SIZE + 16) {
        auto* newBlock = reinterpret_cast<BlockHeader*>(
            reinterpret_cast<char*>(block) + BLOCK_SIZE + size
        );
        newBlock->size = block->size - size - BLOCK_SIZE;
        newBlock->free = true;
        newBlock->next = block->next;
        newBlock->prev = block;
        
        if (block->next) {
            block->next->prev = newBlock;
        }
        block->next = newBlock;
        block->size = size;
    }
}

void mergeBlocks(BlockHeader* block) {
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    
    if (block->prev && block->prev->free) {
        block->prev->size += BLOCK_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

bool isDigit(char c, i32 base) {
    if (c >= '0' && c <= '9') {
        return (c - '0') < base;
    }
    if (c >= 'a' && c <= 'z') {
        return (c - 'a' + 10) < base;
    }
    if (c >= 'A' && c <= 'Z') {
        return (c - 'A' + 10) < base;
    }
    return false;
}

i32 digitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

}

void* sbrk(ssize_t increment) {
    if (programBreak == nullptr) {
        i64 result = syscall1(SYS_BRK, 0);
        if (result < 0) {
            return reinterpret_cast<void*>(-1);
        }
        heapStart = reinterpret_cast<void*>(result);
        heapEnd = heapStart;
        programBreak = heapStart;
    }
    
    if (increment == 0) {
        return programBreak;
    }
    
    void* oldBreak = programBreak;
    void* newBreak = static_cast<char*>(programBreak) + increment;
    
    i64 result = syscall1(SYS_BRK, reinterpret_cast<u64>(newBreak));
    if (result < 0 || reinterpret_cast<void*>(result) != newBreak) {
        return reinterpret_cast<void*>(-1);
    }
    
    programBreak = newBreak;
    heapEnd = newBreak;
    
    return oldBreak;
}

void* malloc(usize size) {
    if (size == 0) {
        return nullptr;
    }
    
    size = alignSize(size);
    
    BlockHeader* block = findFreeBlock(size);
    
    if (block) {
        block->free = false;
        splitBlock(block, size);
        return reinterpret_cast<char*>(block) + BLOCK_SIZE;
    }
    
    block = requestSpace(size);
    if (!block) {
        return nullptr;
    }
    
    return reinterpret_cast<char*>(block) + BLOCK_SIZE;
}

void free(void* ptr) {
    if (!ptr) {
        return;
    }
    
    auto* block = reinterpret_cast<BlockHeader*>(
        static_cast<char*>(ptr) - BLOCK_SIZE
    );
    block->free = true;
    
    mergeBlocks(block);
}

void* calloc(usize nmemb, usize size) {
    usize total = nmemb * size;
    void* ptr = malloc(total);
    
    if (ptr) {
        memset(ptr, 0, total);
    }
    
    return ptr;
}

void* realloc(void* ptr, usize size) {
    if (!ptr) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return nullptr;
    }
    
    auto* block = reinterpret_cast<BlockHeader*>(
        static_cast<char*>(ptr) - BLOCK_SIZE
    );
    
    if (block->size >= size) {
        return ptr;
    }
    
    void* newPtr = malloc(size);
    if (!newPtr) {
        return nullptr;
    }
    
    memcpy(newPtr, ptr, block->size);
    free(ptr);
    
    return newPtr;
}

void* aligned_alloc(usize alignment, usize size) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return nullptr;
    }
    
    if (size % alignment != 0) {
        return nullptr;
    }
    
    usize totalSize = size + alignment + sizeof(void*);
    void* rawPtr = malloc(totalSize);
    if (!rawPtr) {
        return nullptr;
    }
    
    usize addr = reinterpret_cast<usize>(rawPtr) + sizeof(void*);
    usize alignedAddr = alignTo(addr, alignment);
    
    void** storedPtr = reinterpret_cast<void**>(alignedAddr - sizeof(void*));
    *storedPtr = rawPtr;
    
    return reinterpret_cast<void*>(alignedAddr);
}

i32 posix_memalign(void** memptr, usize alignment, usize size) {
    if (!memptr) return 22;
    if (alignment < sizeof(void*)) return 22;
    if ((alignment & (alignment - 1)) != 0) return 22;
    
    void* ptr = aligned_alloc(alignment, alignTo(size, alignment));
    if (!ptr) return 12;
    
    *memptr = ptr;
    return 0;
}

[[noreturn]] void exit(i32 status) {
    while (atexitCount > 0) {
        atexitCount--;
        if (atexitFuncs[atexitCount]) {
            atexitFuncs[atexitCount]();
        }
    }
    
    syscall1(SYS_EXIT, static_cast<u64>(status));
    __builtin_unreachable();
}

[[noreturn]] void _Exit(i32 status) {
    syscall1(SYS_EXIT, static_cast<u64>(status));
    __builtin_unreachable();
}

[[noreturn]] void abort() {
    kill(getpid(), SIGABRT);
    _Exit(134);
}

i32 atexit(void (*function)()) {
    if (atexitCount >= MAX_ATEXIT_FUNCS) {
        return -1;
    }
    atexitFuncs[atexitCount++] = function;
    return 0;
}

i32 at_quick_exit(void (*function)()) {
    if (quickExitCount >= MAX_ATEXIT_FUNCS) {
        return -1;
    }
    quickExitFuncs[quickExitCount++] = function;
    return 0;
}

[[noreturn]] void quick_exit(i32 status) {
    while (quickExitCount > 0) {
        quickExitCount--;
        if (quickExitFuncs[quickExitCount]) {
            quickExitFuncs[quickExitCount]();
        }
    }
    _Exit(status);
}

i32 atoi(const char* str) {
    return static_cast<i32>(strtol(str, nullptr, 10));
}

i64 atol(const char* str) {
    return strtol(str, nullptr, 10);
}

i64 atoll(const char* str) {
    return strtoll(str, nullptr, 10);
}

double atof(const char*) {
    return 0.0;
}

i64 strtol(const char* str, char** endptr, i32 base) {
    const char* p = str;
    i64 result = 0;
    bool negative = false;
    
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    
    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }
    
    if (base == 0) {
        if (*p == '0') {
            p++;
            if (*p == 'x' || *p == 'X') {
                base = 16;
                p++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }
    
    while (isDigit(*p, base)) {
        result = result * base + digitValue(*p);
        p++;
    }
    
    if (endptr) {
        *endptr = const_cast<char*>(p);
    }
    
    return negative ? -result : result;
}

u64 strtoul(const char* str, char** endptr, i32 base) {
    const char* p = str;
    u64 result = 0;
    
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    
    if (*p == '+') {
        p++;
    }
    
    if (base == 0) {
        if (*p == '0') {
            p++;
            if (*p == 'x' || *p == 'X') {
                base = 16;
                p++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }
    
    while (isDigit(*p, base)) {
        result = result * static_cast<u64>(base) + static_cast<u64>(digitValue(*p));
        p++;
    }
    
    if (endptr) {
        *endptr = const_cast<char*>(p);
    }
    
    return result;
}

i64 strtoll(const char* str, char** endptr, i32 base) {
    return strtol(str, endptr, base);
}

u64 strtoull(const char* str, char** endptr, i32 base) {
    return strtoul(str, endptr, base);
}

i32 rand() {
    return rand_r(&randSeed);
}

void srand(u32 seed) {
    randSeed = seed;
}

i32 rand_r(u32* seedp) {
    *seedp = *seedp * 1103515245 + 12345;
    return static_cast<i32>((*seedp / 65536) % (RAND_MAX + 1));
}

char* getenv(const char*) {
    return nullptr;
}

i32 setenv(const char*, const char*, i32) {
    return -1;
}

i32 unsetenv(const char*) {
    return -1;
}

i32 putenv(char*) {
    return -1;
}

i32 clearenv() {
    return 0;
}

i32 system(const char* command) {
    if (!command) {
        return 1;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    
    if (pid == 0) {
        char* argv[] = {const_cast<char*>("/bin/sh"), const_cast<char*>("-c"), 
                        const_cast<char*>(command), nullptr};
        execve("/bin/sh", argv, nullptr);
        _Exit(127);
    }
    
    i32 status;
    waitpid(pid, &status, 0);
    return status;
}

void qsort(void* base, usize nmemb, usize size, i32 (*compar)(const void*, const void*)) {
    if (nmemb <= 1) return;
    
    char* arr = static_cast<char*>(base);
    char* temp = static_cast<char*>(malloc(size));
    if (!temp) return;
    
    for (usize i = 0; i < nmemb - 1; i++) {
        for (usize j = 0; j < nmemb - i - 1; j++) {
            void* a = arr + j * size;
            void* b = arr + (j + 1) * size;
            
            if (compar(a, b) > 0) {
                memcpy(temp, a, size);
                memcpy(a, b, size);
                memcpy(b, temp, size);
            }
        }
    }
    
    free(temp);
}

void* bsearch(const void* key, const void* base, usize nmemb, usize size,
              i32 (*compar)(const void*, const void*)) {
    const char* arr = static_cast<const char*>(base);
    usize low = 0;
    usize high = nmemb;
    
    while (low < high) {
        usize mid = low + (high - low) / 2;
        const void* elem = arr + mid * size;
        i32 cmp = compar(key, elem);
        
        if (cmp == 0) {
            return const_cast<void*>(elem);
        } else if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    
    return nullptr;
}

i32 abs(i32 n) {
    return n < 0 ? -n : n;
}

i64 labs(i64 n) {
    return n < 0 ? -n : n;
}

i64 llabs(i64 n) {
    return n < 0 ? -n : n;
}

div_t div(i32 numer, i32 denom) {
    div_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

ldiv_t ldiv(i64 numer, i64 denom) {
    ldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

lldiv_t lldiv(i64 numer, i64 denom) {
    lldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

char* realpath(const char* path, char* resolved_path) {
    if (!path) return nullptr;
    
    char* result = resolved_path;
    if (!result) {
        result = static_cast<char*>(malloc(256));
        if (!result) return nullptr;
    }
    
    if (path[0] == '/') {
        strlcpy(result, path, 256);
    } else {
        getcwd(result, 256);
        usize len = strlen(result);
        if (len > 0 && result[len - 1] != '/') {
            result[len] = '/';
            result[len + 1] = '\0';
        }
        strlcat(result, path, 256);
    }
    
    return result;
}

char* mktemp(char* templ) {
    usize len = strlen(templ);
    if (len < 6) return templ;
    
    char* suffix = templ + len - 6;
    for (i32 i = 0; i < 6; i++) {
        if (suffix[i] != 'X') return templ;
    }
    
    static u32 counter = 0;
    counter++;
    
    for (i32 i = 0; i < 6; i++) {
        suffix[i] = 'a' + ((counter >> (i * 4)) & 0xF);
    }
    
    return templ;
}

i32 mkstemp(char* templ) {
    mktemp(templ);
    return open(templ, O_RDWR | O_CREAT | O_EXCL, 0600);
}

char* mkdtemp(char* templ) {
    mktemp(templ);
    if (mkdir(templ, 0700) < 0) {
        return nullptr;
    }
    return templ;
}

i32 mblen(const char*, usize) {
    return 1;
}

i32 mbtowc(i32* pwc, const char* s, usize) {
    if (!s) return 0;
    if (pwc) *pwc = static_cast<unsigned char>(*s);
    return *s ? 1 : 0;
}

i32 wctomb(char* s, i32 wc) {
    if (!s) return 0;
    *s = static_cast<char>(wc);
    return 1;
}

usize mbstowcs(i32* dest, const char* src, usize n) {
    usize i = 0;
    while (i < n && src[i]) {
        if (dest) dest[i] = static_cast<unsigned char>(src[i]);
        i++;
    }
    return i;
}

usize wcstombs(char* dest, const i32* src, usize n) {
    usize i = 0;
    while (i < n && src[i]) {
        if (dest) dest[i] = static_cast<char>(src[i]);
        i++;
    }
    return i;
}

}
