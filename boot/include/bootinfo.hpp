#pragma once

#include "../../kernel/include/types.hpp"

namespace sertos::boot {

struct FramebufferInfo {
    u64 address;
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    u8 redMaskSize;
    u8 redMaskShift;
    u8 greenMaskSize;
    u8 greenMaskShift;
    u8 blueMaskSize;
    u8 blueMaskShift;
};

struct MemoryRegion {
    u64 base;
    u64 length;
    u32 type;
    u32 reserved;
};

enum class MemoryRegionType : u32 {
    Usable = 1,
    Reserved = 2,
    AcpiReclaimable = 3,
    AcpiNvs = 4,
    BadMemory = 5,
    BootloaderReclaimable = 6,
    KernelAndModules = 7,
    Framebuffer = 8
};

constexpr u32 BOOT_INFO_MAGIC = 0x5345524F;

struct BootInfo {
    u32 magic;
    u32 version;
    
    FramebufferInfo framebuffer;
    
    u64 memoryMapAddress;
    u32 memoryMapEntries;
    u32 memoryMapEntrySize;
    
    u64 rsdpAddress;
    
    u64 kernelPhysicalBase;
    u64 kernelVirtualBase;
    u64 kernelSize;
    
    u64 totalMemory;
    u64 usableMemory;
    
    u64 higherHalfDirectMapOffset;
};

}
