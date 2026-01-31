#include "../include/stdlib.hpp"
#include "../include/string.hpp"
#include "../include/syscall.hpp"

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

usize alignSize(usize x) {
    return (x + ALIGN_MASK) & ~ALIGN_MASK;
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
    void* request = sbrk(BLOCK_SIZE + size);
    
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

[[noreturn]] void exit(i32 status) {
    syscall1(SYS_EXIT, status);
    __builtin_unreachable();
}

[[noreturn]] void abort() {
    exit(-1);
}

i32 atoi(const char* str) {
    i32 result = 0;
    i32 sign = 1;
    
    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

i64 atol(const char* str) {
    i64 result = 0;
    i32 sign = 1;
    
    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

}
