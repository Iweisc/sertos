#pragma once

#include "../types.hpp"
#include "../../../boot/include/bootinfo.hpp"

namespace sertos::memory {

constexpr u64 PAGE_SIZE = 4096;

class PhysicalMemoryManager {
public:
    static void initialize(const boot::BootInfo* bootInfo);
    
    static void* allocatePage();
    static void* allocatePages(usize count);
    static void freePage(void* page);
    static void freePages(void* pages, usize count);
    
    static u64 totalMemory();
    static u64 usedMemory();
    static u64 freeMemory();
    static u64 totalPages();
    static u64 usedPages();
    static u64 freePages();
    
    static bool isInitialized();

private:
    static void setBit(u64 index);
    static void clearBit(u64 index);
    static bool testBit(u64 index);
    static u64 findFirstFree();
    static u64 findContiguousFree(usize count);
    
    static u64* sBitmap;
    static u64 sBitmapSize;
    static u64 sTotalPages;
    static u64 sUsedPages;
    static u64 sTotalMemory;
    static bool sInitialized;
};

using PMM = PhysicalMemoryManager;

}
