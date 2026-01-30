#include "../include/heap.h"

typedef struct block_header {
    size_t size;
    bool free;
    struct block_header* next;
    struct block_header* prev;
} block_header_t;

static block_header_t* heap_start_block = NULL;
static size_t total_memory = 0;
static size_t used_memory = 0;

#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define MIN_BLOCK_SIZE 16

void heap_init(void) {
    heap_start_block = (block_header_t*)HEAP_START;
    heap_start_block->size = HEAP_SIZE - BLOCK_HEADER_SIZE;
    heap_start_block->free = true;
    heap_start_block->next = NULL;
    heap_start_block->prev = NULL;
    
    total_memory = HEAP_SIZE;
    used_memory = BLOCK_HEADER_SIZE;
}

static block_header_t* find_free_block(size_t size) {
    block_header_t* current = heap_start_block;
    
    while (current != NULL) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

static void split_block(block_header_t* block, size_t size) {
    if (block->size >= size + BLOCK_HEADER_SIZE + MIN_BLOCK_SIZE) {
        block_header_t* new_block = (block_header_t*)((uint8_t*)block + BLOCK_HEADER_SIZE + size);
        new_block->size = block->size - size - BLOCK_HEADER_SIZE;
        new_block->free = true;
        new_block->next = block->next;
        new_block->prev = block;
        
        if (block->next != NULL) {
            block->next->prev = new_block;
        }
        
        block->next = new_block;
        block->size = size;
        
        used_memory += BLOCK_HEADER_SIZE;
    }
}

static void merge_free_blocks(block_header_t* block) {
    if (block->next != NULL && block->next->free) {
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next != NULL) {
            block->next->prev = block;
        }
        used_memory -= BLOCK_HEADER_SIZE;
    }
    
    if (block->prev != NULL && block->prev->free) {
        block->prev->size += BLOCK_HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next != NULL) {
            block->next->prev = block->prev;
        }
        used_memory -= BLOCK_HEADER_SIZE;
    }
}

void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    size = (size + 3) & ~3;
    
    block_header_t* block = find_free_block(size);
    
    if (block == NULL) {
        return NULL;
    }
    
    split_block(block, size);
    block->free = false;
    used_memory += block->size;
    
    return (void*)((uint8_t*)block + BLOCK_HEADER_SIZE);
}

void* kmalloc_aligned(size_t size, size_t alignment) {
    size_t total_size = size + alignment - 1 + sizeof(void*);
    void* ptr = kmalloc(total_size);
    
    if (ptr == NULL) {
        return NULL;
    }
    
    size_t addr = (size_t)ptr + sizeof(void*);
    size_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    
    ((void**)aligned_addr)[-1] = ptr;
    
    return (void*)aligned_addr;
}

void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    
    if (block->free) {
        return;
    }
    
    used_memory -= block->size;
    block->free = true;
    
    merge_free_blocks(block);
}

void* krealloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return kmalloc(size);
    }
    
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    
    if (block->size >= size) {
        return ptr;
    }
    
    void* new_ptr = kmalloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for (size_t i = 0; i < block->size; i++) {
        dst[i] = src[i];
    }
    
    kfree(ptr);
    
    return new_ptr;
}

size_t heap_get_free_memory(void) {
    return total_memory - used_memory;
}

size_t heap_get_used_memory(void) {
    return used_memory;
}
