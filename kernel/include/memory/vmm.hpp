#pragma once

#include "../types.hpp"
#include "pmm.hpp"

namespace sertos::memory {

constexpr u64 PAGE_PRESENT = 1ULL << 0;
constexpr u64 PAGE_WRITABLE = 1ULL << 1;
constexpr u64 PAGE_USER = 1ULL << 2;
constexpr u64 PAGE_WRITE_THROUGH = 1ULL << 3;
constexpr u64 PAGE_CACHE_DISABLE = 1ULL << 4;
constexpr u64 PAGE_ACCESSED = 1ULL << 5;
constexpr u64 PAGE_DIRTY = 1ULL << 6;
constexpr u64 PAGE_HUGE = 1ULL << 7;
constexpr u64 PAGE_GLOBAL = 1ULL << 8;
constexpr u64 PAGE_NO_EXECUTE = 1ULL << 63;

constexpr u64 PAGE_ADDR_MASK = 0x000FFFFFFFFFF000ULL;

struct PageTable {
    u64 entries[512];
};

class VirtualMemoryManager {
public:
    static void initialize();
    
    static bool mapPage(u64 virtualAddr, u64 physicalAddr, u64 flags);
    static bool unmapPage(u64 virtualAddr);
    static u64 getPhysicalAddress(u64 virtualAddr);
    
    static bool mapRange(u64 virtualStart, u64 physicalStart, u64 size, u64 flags);
    static bool unmapRange(u64 virtualStart, u64 size);
    
    static void switchPageTable(PageTable* pml4);
    static PageTable* currentPageTable();
    
    static PageTable* createAddressSpace();
    static void destroyAddressSpace(PageTable* pml4);
    
    static bool isInitialized();

private:
    static PageTable* getOrCreateTable(PageTable* parent, u64 index, u64 flags);
    static void invalidatePage(u64 virtualAddr);
    
    static PageTable* sKernelPML4;
    static bool sInitialized;
};

using VMM = VirtualMemoryManager;

}
