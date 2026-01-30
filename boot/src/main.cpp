#include "../include/bootinfo.hpp"
#include "../../kernel/include/uefi/uefi.hpp"

using namespace sertos;
using namespace sertos::uefi;
using namespace sertos::boot;

namespace {

constexpr u64 KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000ULL;
constexpr u64 HIGHER_HALF_OFFSET = 0xFFFF800000000000ULL;

BootInfo gBootInfo;

void initializeFramebuffer(EfiGraphicsOutputProtocol* gop) {
    if (!gop || !gop->mode || !gop->mode->info) {
        return;
    }
    
    auto mode = gop->mode;
    auto info = mode->info;
    
    gBootInfo.framebuffer.address = mode->frameBufferBase;
    gBootInfo.framebuffer.width = info->horizontalResolution;
    gBootInfo.framebuffer.height = info->verticalResolution;
    gBootInfo.framebuffer.pitch = info->pixelsPerScanLine * 4;
    gBootInfo.framebuffer.bpp = 32;
    
    switch (info->pixelFormat) {
        case EfiGraphicsPixelFormat::PixelRedGreenBlueReserved8BitPerColor:
            gBootInfo.framebuffer.redMaskSize = 8;
            gBootInfo.framebuffer.redMaskShift = 0;
            gBootInfo.framebuffer.greenMaskSize = 8;
            gBootInfo.framebuffer.greenMaskShift = 8;
            gBootInfo.framebuffer.blueMaskSize = 8;
            gBootInfo.framebuffer.blueMaskShift = 16;
            break;
        case EfiGraphicsPixelFormat::PixelBlueGreenRedReserved8BitPerColor:
            gBootInfo.framebuffer.blueMaskSize = 8;
            gBootInfo.framebuffer.blueMaskShift = 0;
            gBootInfo.framebuffer.greenMaskSize = 8;
            gBootInfo.framebuffer.greenMaskShift = 8;
            gBootInfo.framebuffer.redMaskSize = 8;
            gBootInfo.framebuffer.redMaskShift = 16;
            break;
        case EfiGraphicsPixelFormat::PixelBitMask:
            break;
        default:
            break;
    }
}

void processMemoryMap(EfiMemoryDescriptor* map, usize mapSize, usize descriptorSize) {
    gBootInfo.memoryMapAddress = reinterpret_cast<u64>(map);
    gBootInfo.memoryMapEntrySize = static_cast<u32>(descriptorSize);
    gBootInfo.memoryMapEntries = static_cast<u32>(mapSize / descriptorSize);
    
    u64 totalMemory = 0;
    u64 usableMemory = 0;
    
    auto* entry = map;
    for (usize i = 0; i < mapSize / descriptorSize; i++) {
        u64 regionSize = entry->numberOfPages * 4096;
        totalMemory += regionSize;
        
        auto type = static_cast<EfiMemoryType>(entry->type);
        if (type == EfiMemoryType::ConventionalMemory ||
            type == EfiMemoryType::BootServicesCode ||
            type == EfiMemoryType::BootServicesData ||
            type == EfiMemoryType::LoaderCode ||
            type == EfiMemoryType::LoaderData) {
            usableMemory += regionSize;
        }
        
        entry = reinterpret_cast<EfiMemoryDescriptor*>(
            reinterpret_cast<u8*>(entry) + descriptorSize
        );
    }
    
    gBootInfo.totalMemory = totalMemory;
    gBootInfo.usableMemory = usableMemory;
}

void halt() {
    while (true) {
        asm volatile("hlt");
    }
}

}

extern "C" void kernelMain(BootInfo* bootInfo);

extern "C" EfiStatus efi_main(EfiHandle imageHandle, EfiSystemTable* systemTable) {
    UefiServices::initialize(imageHandle, systemTable);
    
    systemTable->conOut->clearScreen(systemTable->conOut);
    UefiServices::println("SertOS UEFI Bootloader v1.0");
    UefiServices::println("Initializing...");
    
    systemTable->bootServices->setWatchdogTimer(0, 0, 0, nullptr);
    
    auto gop = UefiServices::getGraphicsOutput();
    if (gop) {
        UefiServices::println("Graphics output protocol found");
        
        u32 bestMode = 0;
        u32 bestWidth = 0;
        u32 bestHeight = 0;
        
        for (u32 i = 0; i < gop->mode->maxMode; i++) {
            EfiGraphicsOutputModeInformation* info = nullptr;
            usize infoSize = 0;
            
            if (!efi_error(gop->queryMode(gop, i, &infoSize, &info))) {
                if (info->horizontalResolution >= bestWidth && 
                    info->verticalResolution >= bestHeight &&
                    info->horizontalResolution <= 1920 &&
                    info->verticalResolution <= 1080) {
                    bestMode = i;
                    bestWidth = info->horizontalResolution;
                    bestHeight = info->verticalResolution;
                }
            }
        }
        
        if (bestMode != gop->mode->mode) {
            gop->setMode(gop, bestMode);
        }
        
        initializeFramebuffer(gop);
        UefiServices::println("Framebuffer initialized");
    } else {
        UefiServices::println("Warning: No graphics output protocol found");
    }
    
    void* rsdp = UefiServices::findConfigurationTable(EFI_ACPI_20_TABLE_GUID);
    if (rsdp) {
        gBootInfo.rsdpAddress = reinterpret_cast<u64>(rsdp);
        UefiServices::println("ACPI RSDP found");
    } else {
        UefiServices::println("Warning: ACPI RSDP not found");
    }
    
    gBootInfo.magic = BOOT_INFO_MAGIC;
    gBootInfo.version = 1;
    gBootInfo.kernelVirtualBase = KERNEL_VIRTUAL_BASE;
    gBootInfo.higherHalfDirectMapOffset = HIGHER_HALF_OFFSET;
    
    UefiServices::println("Preparing to exit boot services...");
    
    EfiMemoryDescriptor* memoryMap = nullptr;
    usize mapSize = 0;
    usize mapKey = 0;
    usize descriptorSize = 0;
    
    EfiStatus status = UefiServices::getMemoryMap(&memoryMap, &mapSize, &mapKey, &descriptorSize);
    if (efi_error(status)) {
        UefiServices::println("Error: Failed to get memory map");
        halt();
    }
    
    processMemoryMap(memoryMap, mapSize, descriptorSize);
    
    UefiServices::println("Exiting boot services...");
    
    status = systemTable->bootServices->exitBootServices(imageHandle, mapKey);
    if (efi_error(status)) {
        status = UefiServices::getMemoryMap(&memoryMap, &mapSize, &mapKey, &descriptorSize);
        if (efi_error(status)) {
            halt();
        }
        processMemoryMap(memoryMap, mapSize, descriptorSize);
        status = systemTable->bootServices->exitBootServices(imageHandle, mapKey);
        if (efi_error(status)) {
            halt();
        }
    }
    
    kernelMain(&gBootInfo);
    
    halt();
    return EFI_SUCCESS;
}
