#ifndef HEAP_H
#define HEAP_H

#include "types.h"

#define HEAP_START 0x100000
#define HEAP_SIZE 0x100000

void heap_init(void);
void* kmalloc(size_t size);
void* kmalloc_aligned(size_t size, size_t alignment);
void kfree(void* ptr);
void* krealloc(void* ptr, size_t size);
size_t heap_get_free_memory(void);
size_t heap_get_used_memory(void);

#endif
