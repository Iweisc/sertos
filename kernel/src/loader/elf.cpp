#include "../../include/loader/elf.hpp"
#include "../../include/memory/vmm.hpp"
#include "../../include/memory/pmm.hpp"

namespace sertos::loader {

using sertos::align_up;
using sertos::align_down;

static void memcpy(void* dest, const void* src, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    const u8* s = reinterpret_cast<const u8*>(src);
    for (usize i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

static void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

ElfLoadResult ElfLoader::load(process::Process* proc, const u8* data, usize size) {
    ElfLoadResult result = {ElfError::Success, 0, 0, 0};
    
    if (!validate(data, size)) {
        result.error = ElfError::InvalidMagic;
        return result;
    }
    
    const Elf64Header* header = getHeader(data);
    
    if (header->ident[4] != ELFCLASS64) {
        result.error = ElfError::InvalidClass;
        return result;
    }
    
    if (header->ident[5] != ELFDATA2LSB) {
        result.error = ElfError::InvalidEndian;
        return result;
    }
    
    if (header->type != ET_EXEC && header->type != ET_DYN) {
        result.error = ElfError::InvalidType;
        return result;
    }
    
    if (header->machine != EM_X86_64) {
        result.error = ElfError::InvalidMachine;
        return result;
    }
    
    result.entryPoint = header->entry;
    result.baseAddress = 0xFFFFFFFFFFFFFFFFULL;
    result.endAddress = 0;
    
    const Elf64ProgramHeader* phdrs = reinterpret_cast<const Elf64ProgramHeader*>(
        data + header->phoff
    );
    
    for (u16 i = 0; i < header->phnum; i++) {
        const Elf64ProgramHeader* phdr = &phdrs[i];
        
        if (phdr->type != PT_LOAD) {
            continue;
        }
        
        ElfError err = loadSegment(proc, data, phdr);
        if (err != ElfError::Success) {
            result.error = err;
            return result;
        }
        
        if (phdr->vaddr < result.baseAddress) {
            result.baseAddress = phdr->vaddr;
        }
        
        u64 segmentEnd = phdr->vaddr + phdr->memsz;
        if (segmentEnd > result.endAddress) {
            result.endAddress = segmentEnd;
        }
    }
    
    proc->heapStart = align_up(result.endAddress, static_cast<u64>(memory::PAGE_SIZE));
    proc->heapEnd = proc->heapStart;
    proc->programBreak = proc->heapStart;
    
    return result;
}

bool ElfLoader::validate(const u8* data, usize size) {
    if (size < sizeof(Elf64Header)) {
        return false;
    }
    
    const Elf64Header* header = getHeader(data);
    
    u32 magic = *reinterpret_cast<const u32*>(header->ident);
    if (magic != ELF_MAGIC) {
        return false;
    }
    
    return true;
}

const Elf64Header* ElfLoader::getHeader(const u8* data) {
    return reinterpret_cast<const Elf64Header*>(data);
}

ElfError ElfLoader::loadSegment(process::Process* proc, const u8* data, const Elf64ProgramHeader* phdr) {
    if (phdr->memsz == 0) {
        return ElfError::Success;
    }
    
    u64 pageFlags = elfFlagsToPageFlags(phdr->flags);
    
    u64 alignedVaddr = align_down(phdr->vaddr, static_cast<u64>(memory::PAGE_SIZE));
    u64 alignedEnd = align_up(phdr->vaddr + phdr->memsz, static_cast<u64>(memory::PAGE_SIZE));
    u64 numPages = (alignedEnd - alignedVaddr) / memory::PAGE_SIZE;
    
    for (u64 i = 0; i < numPages; i++) {
        u64 vaddr = alignedVaddr + i * memory::PAGE_SIZE;
        
        u64 existingPhys = memory::VMM::getPhysicalAddressIn(proc->pageTable, vaddr);
        if (existingPhys) {
            continue;
        }
        
        void* physPage = memory::PMM::allocatePage();
        if (!physPage) {
            return ElfError::MemoryError;
        }
        
        memset(physPage, 0, memory::PAGE_SIZE);
        
        if (!memory::VMM::mapPageIn(proc->pageTable, vaddr, 
                                    reinterpret_cast<u64>(physPage), pageFlags)) {
            memory::PMM::freePage(physPage);
            return ElfError::MemoryError;
        }
    }
    
    if (phdr->filesz > 0) {
        u64 offset = phdr->vaddr - alignedVaddr;
        const u8* srcData = data + phdr->offset;
        
        for (u64 copied = 0; copied < phdr->filesz; ) {
            u64 vaddr = phdr->vaddr + copied;
            u64 pageOffset = vaddr & (memory::PAGE_SIZE - 1);
            u64 physAddr = memory::VMM::getPhysicalAddressIn(proc->pageTable, vaddr);
            
            if (!physAddr) {
                return ElfError::MemoryError;
            }
            
            u64 toCopy = memory::PAGE_SIZE - pageOffset;
            if (toCopy > phdr->filesz - copied) {
                toCopy = phdr->filesz - copied;
            }
            
            memcpy(reinterpret_cast<void*>(physAddr), srcData + copied, toCopy);
            copied += toCopy;
        }
    }
    
    return ElfError::Success;
}

u64 ElfLoader::elfFlagsToPageFlags(u32 elfFlags) {
    u64 pageFlags = memory::PAGE_PRESENT | memory::PAGE_USER;
    
    if (elfFlags & PF_W) {
        pageFlags |= memory::PAGE_WRITABLE;
    }
    
    if (!(elfFlags & PF_X)) {
        pageFlags |= memory::PAGE_NO_EXECUTE;
    }
    
    return pageFlags;
}

}
