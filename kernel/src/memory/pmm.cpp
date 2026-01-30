#include "../../include/memory/pmm.hpp"
#include "../../include/uefi/types.hpp"

namespace sertos::memory {

u64* PhysicalMemoryManager::sBitmap = nullptr;
u64 PhysicalMemoryManager::sBitmapSize = 0;
u64 PhysicalMemoryManager::sTotalPages = 0;
u64 PhysicalMemoryManager::sUsedPages = 0;
u64 PhysicalMemoryManager::sTotalMemory = 0;
bool PhysicalMemoryManager::sInitialized = false;

void PhysicalMemoryManager::initialize(const boot::BootInfo* bootInfo) {
    if (!bootInfo || sInitialized) {
        return;
    }
    
    sTotalMemory = bootInfo->totalMemory;
    sTotalPages = sTotalMemory / PAGE_SIZE;
    sBitmapSize = (sTotalPages + 63) / 64;
    
    auto* memoryMap = reinterpret_cast<uefi::EfiMemoryDescriptor*>(bootInfo->memoryMapAddress);
    u32 entryCount = bootInfo->memoryMapEntries;
    u32 entrySize = bootInfo->memoryMapEntrySize;
    
    u64 bitmapBytes = sBitmapSize * sizeof(u64);
    u64 bitmapPages = (bitmapBytes + PAGE_SIZE - 1) / PAGE_SIZE;
    
    u64 bitmapAddress = 0;
    auto* entry = memoryMap;
    
    for (u32 i = 0; i < entryCount; i++) {
        auto type = static_cast<uefi::EfiMemoryType>(entry->type);
        
        if (type == uefi::EfiMemoryType::ConventionalMemory) {
            u64 regionSize = entry->numberOfPages * PAGE_SIZE;
            if (regionSize >= bitmapBytes && entry->physicalStart >= 0x100000) {
                bitmapAddress = entry->physicalStart;
                break;
            }
        }
        
        entry = reinterpret_cast<uefi::EfiMemoryDescriptor*>(
            reinterpret_cast<u8*>(entry) + entrySize
        );
    }
    
    if (bitmapAddress == 0) {
        return;
    }
    
    sBitmap = reinterpret_cast<u64*>(bitmapAddress);
    
    for (u64 i = 0; i < sBitmapSize; i++) {
        sBitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
    }
    sUsedPages = sTotalPages;
    
    entry = memoryMap;
    for (u32 i = 0; i < entryCount; i++) {
        auto type = static_cast<uefi::EfiMemoryType>(entry->type);
        
        if (type == uefi::EfiMemoryType::ConventionalMemory) {
            u64 startPage = entry->physicalStart / PAGE_SIZE;
            u64 pageCount = entry->numberOfPages;
            
            for (u64 j = 0; j < pageCount; j++) {
                clearBit(startPage + j);
                sUsedPages--;
            }
        }
        
        entry = reinterpret_cast<uefi::EfiMemoryDescriptor*>(
            reinterpret_cast<u8*>(entry) + entrySize
        );
    }
    
    u64 bitmapStartPage = bitmapAddress / PAGE_SIZE;
    for (u64 i = 0; i < bitmapPages; i++) {
        setBit(bitmapStartPage + i);
        sUsedPages++;
    }
    
    for (u64 i = 0; i < 0x100000 / PAGE_SIZE; i++) {
        if (!testBit(i)) {
            setBit(i);
            sUsedPages++;
        }
    }
    
    sInitialized = true;
}

void* PhysicalMemoryManager::allocatePage() {
    if (!sInitialized) {
        return nullptr;
    }
    
    u64 index = findFirstFree();
    if (index == static_cast<u64>(-1)) {
        return nullptr;
    }
    
    setBit(index);
    sUsedPages++;
    
    return reinterpret_cast<void*>(index * PAGE_SIZE);
}

void* PhysicalMemoryManager::allocatePages(usize count) {
    if (!sInitialized || count == 0) {
        return nullptr;
    }
    
    if (count == 1) {
        return allocatePage();
    }
    
    u64 startIndex = findContiguousFree(count);
    if (startIndex == static_cast<u64>(-1)) {
        return nullptr;
    }
    
    for (usize i = 0; i < count; i++) {
        setBit(startIndex + i);
    }
    sUsedPages += count;
    
    return reinterpret_cast<void*>(startIndex * PAGE_SIZE);
}

void PhysicalMemoryManager::freePage(void* page) {
    if (!sInitialized || !page) {
        return;
    }
    
    u64 address = reinterpret_cast<u64>(page);
    if (address % PAGE_SIZE != 0) {
        return;
    }
    
    u64 index = address / PAGE_SIZE;
    if (index >= sTotalPages) {
        return;
    }
    
    if (testBit(index)) {
        clearBit(index);
        sUsedPages--;
    }
}

void PhysicalMemoryManager::freePages(void* pages, usize count) {
    if (!sInitialized || !pages || count == 0) {
        return;
    }
    
    u64 address = reinterpret_cast<u64>(pages);
    if (address % PAGE_SIZE != 0) {
        return;
    }
    
    u64 startIndex = address / PAGE_SIZE;
    
    for (usize i = 0; i < count; i++) {
        u64 index = startIndex + i;
        if (index < sTotalPages && testBit(index)) {
            clearBit(index);
            sUsedPages--;
        }
    }
}

u64 PhysicalMemoryManager::totalMemory() { return sTotalMemory; }
u64 PhysicalMemoryManager::usedMemory() { return sUsedPages * PAGE_SIZE; }
u64 PhysicalMemoryManager::freeMemory() { return (sTotalPages - sUsedPages) * PAGE_SIZE; }
u64 PhysicalMemoryManager::totalPages() { return sTotalPages; }
u64 PhysicalMemoryManager::usedPages() { return sUsedPages; }
u64 PhysicalMemoryManager::freePages() { return sTotalPages - sUsedPages; }
bool PhysicalMemoryManager::isInitialized() { return sInitialized; }

void PhysicalMemoryManager::setBit(u64 index) {
    if (index >= sTotalPages) return;
    sBitmap[index / 64] |= (1ULL << (index % 64));
}

void PhysicalMemoryManager::clearBit(u64 index) {
    if (index >= sTotalPages) return;
    sBitmap[index / 64] &= ~(1ULL << (index % 64));
}

bool PhysicalMemoryManager::testBit(u64 index) {
    if (index >= sTotalPages) return true;
    return (sBitmap[index / 64] & (1ULL << (index % 64))) != 0;
}

u64 PhysicalMemoryManager::findFirstFree() {
    for (u64 i = 0; i < sBitmapSize; i++) {
        if (sBitmap[i] != 0xFFFFFFFFFFFFFFFFULL) {
            for (u64 j = 0; j < 64; j++) {
                u64 index = i * 64 + j;
                if (index >= sTotalPages) {
                    return static_cast<u64>(-1);
                }
                if (!testBit(index)) {
                    return index;
                }
            }
        }
    }
    return static_cast<u64>(-1);
}

u64 PhysicalMemoryManager::findContiguousFree(usize count) {
    u64 consecutive = 0;
    u64 startIndex = 0;
    
    for (u64 i = 0; i < sTotalPages; i++) {
        if (!testBit(i)) {
            if (consecutive == 0) {
                startIndex = i;
            }
            consecutive++;
            if (consecutive >= count) {
                return startIndex;
            }
        } else {
            consecutive = 0;
        }
    }
    
    return static_cast<u64>(-1);
}

}
