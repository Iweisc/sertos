#include "../../include/memory/vmm.hpp"

namespace sertos::memory {

PageTable* VirtualMemoryManager::sKernelPML4 = nullptr;
bool VirtualMemoryManager::sInitialized = false;

void VirtualMemoryManager::initialize() {
    if (sInitialized || !PMM::isInitialized()) {
        return;
    }
    
    sKernelPML4 = reinterpret_cast<PageTable*>(PMM::allocatePage());
    if (!sKernelPML4) {
        return;
    }
    
    for (int i = 0; i < 512; i++) {
        sKernelPML4->entries[i] = 0;
    }
    
    for (u64 addr = 0; addr < 4 * GB; addr += PAGE_SIZE) {
        mapPage(addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
    }
    
    switchPageTable(sKernelPML4);
    sInitialized = true;
}

bool VirtualMemoryManager::mapPage(u64 virtualAddr, u64 physicalAddr, u64 flags) {
    if (!sKernelPML4) {
        return false;
    }
    
    virtualAddr = align_down(virtualAddr, static_cast<u64>(PAGE_SIZE));
    physicalAddr = align_down(physicalAddr, static_cast<u64>(PAGE_SIZE));
    
    u64 pml4Index = (virtualAddr >> 39) & 0x1FF;
    u64 pdptIndex = (virtualAddr >> 30) & 0x1FF;
    u64 pdIndex = (virtualAddr >> 21) & 0x1FF;
    u64 ptIndex = (virtualAddr >> 12) & 0x1FF;
    
    PageTable* pdpt = getOrCreateTable(sKernelPML4, pml4Index, PAGE_PRESENT | PAGE_WRITABLE);
    if (!pdpt) return false;
    
    PageTable* pd = getOrCreateTable(pdpt, pdptIndex, PAGE_PRESENT | PAGE_WRITABLE);
    if (!pd) return false;
    
    PageTable* pt = getOrCreateTable(pd, pdIndex, PAGE_PRESENT | PAGE_WRITABLE);
    if (!pt) return false;
    
    pt->entries[ptIndex] = (physicalAddr & PAGE_ADDR_MASK) | flags;
    
    invalidatePage(virtualAddr);
    
    return true;
}

bool VirtualMemoryManager::unmapPage(u64 virtualAddr) {
    if (!sKernelPML4) {
        return false;
    }
    
    virtualAddr = align_down(virtualAddr, static_cast<u64>(PAGE_SIZE));
    
    u64 pml4Index = (virtualAddr >> 39) & 0x1FF;
    u64 pdptIndex = (virtualAddr >> 30) & 0x1FF;
    u64 pdIndex = (virtualAddr >> 21) & 0x1FF;
    u64 ptIndex = (virtualAddr >> 12) & 0x1FF;
    
    if (!(sKernelPML4->entries[pml4Index] & PAGE_PRESENT)) {
        return false;
    }
    
    PageTable* pdpt = reinterpret_cast<PageTable*>(sKernelPML4->entries[pml4Index] & PAGE_ADDR_MASK);
    if (!(pdpt->entries[pdptIndex] & PAGE_PRESENT)) {
        return false;
    }
    
    PageTable* pd = reinterpret_cast<PageTable*>(pdpt->entries[pdptIndex] & PAGE_ADDR_MASK);
    if (!(pd->entries[pdIndex] & PAGE_PRESENT)) {
        return false;
    }
    
    PageTable* pt = reinterpret_cast<PageTable*>(pd->entries[pdIndex] & PAGE_ADDR_MASK);
    
    pt->entries[ptIndex] = 0;
    invalidatePage(virtualAddr);
    
    return true;
}

u64 VirtualMemoryManager::getPhysicalAddress(u64 virtualAddr) {
    if (!sKernelPML4) {
        return 0;
    }
    
    u64 offset = virtualAddr & 0xFFF;
    virtualAddr = align_down(virtualAddr, static_cast<u64>(PAGE_SIZE));
    
    u64 pml4Index = (virtualAddr >> 39) & 0x1FF;
    u64 pdptIndex = (virtualAddr >> 30) & 0x1FF;
    u64 pdIndex = (virtualAddr >> 21) & 0x1FF;
    u64 ptIndex = (virtualAddr >> 12) & 0x1FF;
    
    if (!(sKernelPML4->entries[pml4Index] & PAGE_PRESENT)) {
        return 0;
    }
    
    PageTable* pdpt = reinterpret_cast<PageTable*>(sKernelPML4->entries[pml4Index] & PAGE_ADDR_MASK);
    if (!(pdpt->entries[pdptIndex] & PAGE_PRESENT)) {
        return 0;
    }
    
    PageTable* pd = reinterpret_cast<PageTable*>(pdpt->entries[pdptIndex] & PAGE_ADDR_MASK);
    if (!(pd->entries[pdIndex] & PAGE_PRESENT)) {
        return 0;
    }
    
    PageTable* pt = reinterpret_cast<PageTable*>(pd->entries[pdIndex] & PAGE_ADDR_MASK);
    if (!(pt->entries[ptIndex] & PAGE_PRESENT)) {
        return 0;
    }
    
    return (pt->entries[ptIndex] & PAGE_ADDR_MASK) + offset;
}

bool VirtualMemoryManager::mapRange(u64 virtualStart, u64 physicalStart, u64 size, u64 flags) {
    virtualStart = align_down(virtualStart, static_cast<u64>(PAGE_SIZE));
    physicalStart = align_down(physicalStart, static_cast<u64>(PAGE_SIZE));
    size = align_up(size, static_cast<u64>(PAGE_SIZE));
    
    for (u64 offset = 0; offset < size; offset += PAGE_SIZE) {
        if (!mapPage(virtualStart + offset, physicalStart + offset, flags)) {
            unmapRange(virtualStart, offset);
            return false;
        }
    }
    
    return true;
}

bool VirtualMemoryManager::unmapRange(u64 virtualStart, u64 size) {
    virtualStart = align_down(virtualStart, static_cast<u64>(PAGE_SIZE));
    size = align_up(size, static_cast<u64>(PAGE_SIZE));
    
    for (u64 offset = 0; offset < size; offset += PAGE_SIZE) {
        unmapPage(virtualStart + offset);
    }
    
    return true;
}

void VirtualMemoryManager::switchPageTable(PageTable* pml4) {
    if (!pml4) return;
    
    u64 cr3 = reinterpret_cast<u64>(pml4);
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

PageTable* VirtualMemoryManager::currentPageTable() {
    u64 cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return reinterpret_cast<PageTable*>(cr3 & PAGE_ADDR_MASK);
}

PageTable* VirtualMemoryManager::createAddressSpace() {
    PageTable* pml4 = reinterpret_cast<PageTable*>(PMM::allocatePage());
    if (!pml4) {
        return nullptr;
    }
    
    for (int i = 0; i < 256; i++) {
        pml4->entries[i] = 0;
    }
    
    for (int i = 256; i < 512; i++) {
        pml4->entries[i] = sKernelPML4->entries[i];
    }
    
    return pml4;
}

void VirtualMemoryManager::destroyAddressSpace(PageTable* pml4) {
    if (!pml4 || pml4 == sKernelPML4) {
        return;
    }
    
    PMM::freePage(pml4);
}

bool VirtualMemoryManager::isInitialized() {
    return sInitialized;
}

PageTable* VirtualMemoryManager::getOrCreateTable(PageTable* parent, u64 index, u64 flags) {
    if (parent->entries[index] & PAGE_PRESENT) {
        return reinterpret_cast<PageTable*>(parent->entries[index] & PAGE_ADDR_MASK);
    }
    
    PageTable* newTable = reinterpret_cast<PageTable*>(PMM::allocatePage());
    if (!newTable) {
        return nullptr;
    }
    
    for (int i = 0; i < 512; i++) {
        newTable->entries[i] = 0;
    }
    
    parent->entries[index] = (reinterpret_cast<u64>(newTable) & PAGE_ADDR_MASK) | flags;
    
    return newTable;
}

void VirtualMemoryManager::invalidatePage(u64 virtualAddr) {
    asm volatile("invlpg (%0)" : : "r"(virtualAddr) : "memory");
}

}
