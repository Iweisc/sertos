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
    
    for (int i = 0; i < 256; i++) {
        if (pml4->entries[i] & PAGE_PRESENT) {
            PageTable* pdpt = reinterpret_cast<PageTable*>(pml4->entries[i] & PAGE_ADDR_MASK);
            for (int j = 0; j < 512; j++) {
                if (pdpt->entries[j] & PAGE_PRESENT) {
                    PageTable* pd = reinterpret_cast<PageTable*>(pdpt->entries[j] & PAGE_ADDR_MASK);
                    for (int k = 0; k < 512; k++) {
                        if (pd->entries[k] & PAGE_PRESENT) {
                            PageTable* pt = reinterpret_cast<PageTable*>(pd->entries[k] & PAGE_ADDR_MASK);
                            PMM::freePage(pt);
                        }
                    }
                    PMM::freePage(pd);
                }
            }
            PMM::freePage(pdpt);
        }
    }
    
    PMM::freePage(pml4);
}

bool VirtualMemoryManager::mapPageIn(PageTable* pml4, u64 virtualAddr, u64 physicalAddr, u64 flags) {
    if (!pml4) {
        return false;
    }
    
    virtualAddr = align_down(virtualAddr, static_cast<u64>(PAGE_SIZE));
    physicalAddr = align_down(physicalAddr, static_cast<u64>(PAGE_SIZE));
    
    u64 pml4Index = (virtualAddr >> 39) & 0x1FF;
    u64 pdptIndex = (virtualAddr >> 30) & 0x1FF;
    u64 pdIndex = (virtualAddr >> 21) & 0x1FF;
    u64 ptIndex = (virtualAddr >> 12) & 0x1FF;
    
    PageTable* pdpt = getOrCreateTableIn(pml4, pml4, pml4Index, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    if (!pdpt) return false;
    
    PageTable* pd = getOrCreateTableIn(pml4, pdpt, pdptIndex, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    if (!pd) return false;
    
    PageTable* pt = getOrCreateTableIn(pml4, pd, pdIndex, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    if (!pt) return false;
    
    pt->entries[ptIndex] = (physicalAddr & PAGE_ADDR_MASK) | flags;
    
    if (pml4 == currentPageTable()) {
        invalidatePage(virtualAddr);
    }
    
    return true;
}

bool VirtualMemoryManager::unmapPageIn(PageTable* pml4, u64 virtualAddr) {
    if (!pml4) {
        return false;
    }
    
    virtualAddr = align_down(virtualAddr, static_cast<u64>(PAGE_SIZE));
    
    u64 pml4Index = (virtualAddr >> 39) & 0x1FF;
    u64 pdptIndex = (virtualAddr >> 30) & 0x1FF;
    u64 pdIndex = (virtualAddr >> 21) & 0x1FF;
    u64 ptIndex = (virtualAddr >> 12) & 0x1FF;
    
    if (!(pml4->entries[pml4Index] & PAGE_PRESENT)) {
        return false;
    }
    
    PageTable* pdpt = reinterpret_cast<PageTable*>(pml4->entries[pml4Index] & PAGE_ADDR_MASK);
    if (!(pdpt->entries[pdptIndex] & PAGE_PRESENT)) {
        return false;
    }
    
    PageTable* pd = reinterpret_cast<PageTable*>(pdpt->entries[pdptIndex] & PAGE_ADDR_MASK);
    if (!(pd->entries[pdIndex] & PAGE_PRESENT)) {
        return false;
    }
    
    PageTable* pt = reinterpret_cast<PageTable*>(pd->entries[pdIndex] & PAGE_ADDR_MASK);
    
    pt->entries[ptIndex] = 0;
    
    if (pml4 == currentPageTable()) {
        invalidatePage(virtualAddr);
    }
    
    return true;
}

u64 VirtualMemoryManager::getPhysicalAddressIn(PageTable* pml4, u64 virtualAddr) {
    if (!pml4) {
        return 0;
    }
    
    u64 offset = virtualAddr & 0xFFF;
    virtualAddr = align_down(virtualAddr, static_cast<u64>(PAGE_SIZE));
    
    u64 pml4Index = (virtualAddr >> 39) & 0x1FF;
    u64 pdptIndex = (virtualAddr >> 30) & 0x1FF;
    u64 pdIndex = (virtualAddr >> 21) & 0x1FF;
    u64 ptIndex = (virtualAddr >> 12) & 0x1FF;
    
    if (!(pml4->entries[pml4Index] & PAGE_PRESENT)) {
        return 0;
    }
    
    PageTable* pdpt = reinterpret_cast<PageTable*>(pml4->entries[pml4Index] & PAGE_ADDR_MASK);
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

PageTable* VirtualMemoryManager::kernelPageTable() {
    return sKernelPML4;
}

PageTable* VirtualMemoryManager::cloneAddressSpace(PageTable* source) {
    if (!source) {
        return nullptr;
    }
    
    PageTable* pml4 = reinterpret_cast<PageTable*>(PMM::allocatePage());
    if (!pml4) {
        return nullptr;
    }
    
    for (int i = 0; i < 512; i++) {
        pml4->entries[i] = 0;
    }
    
    for (int i = 256; i < 512; i++) {
        pml4->entries[i] = sKernelPML4->entries[i];
    }
    
    for (int i = 0; i < 256; i++) {
        if (!(source->entries[i] & PAGE_PRESENT)) continue;
        
        PageTable* srcPdpt = reinterpret_cast<PageTable*>(source->entries[i] & PAGE_ADDR_MASK);
        PageTable* dstPdpt = reinterpret_cast<PageTable*>(PMM::allocatePage());
        if (!dstPdpt) {
            destroyAddressSpace(pml4);
            return nullptr;
        }
        
        for (int j = 0; j < 512; j++) {
            dstPdpt->entries[j] = 0;
        }
        
        pml4->entries[i] = (reinterpret_cast<u64>(dstPdpt) & PAGE_ADDR_MASK) |
                          (source->entries[i] & ~PAGE_ADDR_MASK);
        
        for (int j = 0; j < 512; j++) {
            if (!(srcPdpt->entries[j] & PAGE_PRESENT)) continue;
            
            PageTable* srcPd = reinterpret_cast<PageTable*>(srcPdpt->entries[j] & PAGE_ADDR_MASK);
            PageTable* dstPd = reinterpret_cast<PageTable*>(PMM::allocatePage());
            if (!dstPd) {
                destroyAddressSpace(pml4);
                return nullptr;
            }
            
            for (int k = 0; k < 512; k++) {
                dstPd->entries[k] = 0;
            }
            
            dstPdpt->entries[j] = (reinterpret_cast<u64>(dstPd) & PAGE_ADDR_MASK) |
                                 (srcPdpt->entries[j] & ~PAGE_ADDR_MASK);
            
            for (int k = 0; k < 512; k++) {
                if (!(srcPd->entries[k] & PAGE_PRESENT)) continue;
                
                PageTable* srcPt = reinterpret_cast<PageTable*>(srcPd->entries[k] & PAGE_ADDR_MASK);
                PageTable* dstPt = reinterpret_cast<PageTable*>(PMM::allocatePage());
                if (!dstPt) {
                    destroyAddressSpace(pml4);
                    return nullptr;
                }
                
                for (int l = 0; l < 512; l++) {
                    dstPt->entries[l] = 0;
                }
                
                dstPd->entries[k] = (reinterpret_cast<u64>(dstPt) & PAGE_ADDR_MASK) |
                                   (srcPd->entries[k] & ~PAGE_ADDR_MASK);
                
                for (int l = 0; l < 512; l++) {
                    if (!(srcPt->entries[l] & PAGE_PRESENT)) continue;
                    
                    u64 srcPhys = srcPt->entries[l] & PAGE_ADDR_MASK;
                    void* newPage = PMM::allocatePage();
                    if (!newPage) {
                        destroyAddressSpace(pml4);
                        return nullptr;
                    }
                    
                    u8* dst = reinterpret_cast<u8*>(newPage);
                    u8* src = reinterpret_cast<u8*>(srcPhys);
                    for (usize m = 0; m < PAGE_SIZE; m++) {
                        dst[m] = src[m];
                    }
                    
                    dstPt->entries[l] = (reinterpret_cast<u64>(newPage) & PAGE_ADDR_MASK) |
                                       (srcPt->entries[l] & ~PAGE_ADDR_MASK);
                }
            }
        }
    }
    
    return pml4;
}

bool VirtualMemoryManager::allocateUserPages(PageTable* pml4, u64 virtualAddr, usize count, u64 flags) {
    if (!pml4 || count == 0) {
        return false;
    }
    
    virtualAddr = align_down(virtualAddr, static_cast<u64>(PAGE_SIZE));
    
    for (usize i = 0; i < count; i++) {
        void* page = PMM::allocatePage();
        if (!page) {
            freeUserPages(pml4, virtualAddr, i);
            return false;
        }
        
        u8* pagePtr = reinterpret_cast<u8*>(page);
        for (usize j = 0; j < PAGE_SIZE; j++) {
            pagePtr[j] = 0;
        }
        
        if (!mapPageIn(pml4, virtualAddr + i * PAGE_SIZE, reinterpret_cast<u64>(page), flags)) {
            PMM::freePage(page);
            freeUserPages(pml4, virtualAddr, i);
            return false;
        }
    }
    
    return true;
}

bool VirtualMemoryManager::freeUserPages(PageTable* pml4, u64 virtualAddr, usize count) {
    if (!pml4 || count == 0) {
        return false;
    }
    
    virtualAddr = align_down(virtualAddr, static_cast<u64>(PAGE_SIZE));
    
    for (usize i = 0; i < count; i++) {
        u64 physAddr = getPhysicalAddressIn(pml4, virtualAddr + i * PAGE_SIZE);
        if (physAddr) {
            PMM::freePage(reinterpret_cast<void*>(physAddr));
            unmapPageIn(pml4, virtualAddr + i * PAGE_SIZE);
        }
    }
    
    return true;
}

bool VirtualMemoryManager::isUserAddress(u64 addr) {
    return addr >= USER_SPACE_START && addr <= USER_SPACE_END;
}

bool VirtualMemoryManager::isKernelAddress(u64 addr) {
    return addr > USER_SPACE_END;
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

PageTable* VirtualMemoryManager::getOrCreateTableIn(PageTable*, PageTable* parent, u64 index, u64 flags) {
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
