#include "../../include/uefi/uefi.hpp"

namespace sertos::uefi {

EfiHandle UefiServices::sImageHandle = nullptr;
EfiSystemTable* UefiServices::sSystemTable = nullptr;
bool UefiServices::sBootServicesExited = false;

void UefiServices::initialize(EfiHandle imageHandle, EfiSystemTable* systemTable) {
    sImageHandle = imageHandle;
    sSystemTable = systemTable;
    sBootServicesExited = false;
}

EfiSystemTable* UefiServices::systemTable() {
    return sSystemTable;
}

EfiBootServices* UefiServices::bootServices() {
    return sSystemTable ? sSystemTable->bootServices : nullptr;
}

EfiRuntimeServices* UefiServices::runtimeServices() {
    return sSystemTable ? sSystemTable->runtimeServices : nullptr;
}

EfiHandle UefiServices::imageHandle() {
    return sImageHandle;
}

bool UefiServices::bootServicesExited() {
    return sBootServicesExited;
}

void UefiServices::print(const char* str) {
    if (!sSystemTable || !sSystemTable->conOut || sBootServicesExited) {
        return;
    }
    
    Char16 buffer[2] = {0, 0};
    while (*str) {
        if (*str == '\n') {
            buffer[0] = u'\r';
            sSystemTable->conOut->outputString(sSystemTable->conOut, buffer);
        }
        buffer[0] = static_cast<Char16>(*str);
        sSystemTable->conOut->outputString(sSystemTable->conOut, buffer);
        str++;
    }
}

void UefiServices::print(const Char16* str) {
    if (!sSystemTable || !sSystemTable->conOut || sBootServicesExited) {
        return;
    }
    sSystemTable->conOut->outputString(sSystemTable->conOut, str);
}

void UefiServices::println(const char* str) {
    print(str);
    print("\n");
}

EfiStatus UefiServices::getMemoryMap(EfiMemoryDescriptor** map, usize* mapSize, usize* mapKey, usize* descriptorSize) {
    if (!sSystemTable || !sSystemTable->bootServices || sBootServicesExited) {
        return EFI_NOT_READY;
    }
    
    auto bs = sSystemTable->bootServices;
    
    *mapSize = 0;
    u32 descriptorVersion = 0;
    EfiStatus status = bs->getMemoryMap(mapSize, nullptr, mapKey, descriptorSize, &descriptorVersion);
    
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }
    
    *mapSize += 2 * *descriptorSize;
    
    status = bs->allocatePool(EfiMemoryType::LoaderData, *mapSize, reinterpret_cast<void**>(map));
    if (efi_error(status)) {
        return status;
    }
    
    status = bs->getMemoryMap(mapSize, *map, mapKey, descriptorSize, &descriptorVersion);
    if (efi_error(status)) {
        bs->freePool(*map);
        *map = nullptr;
        return status;
    }
    
    return EFI_SUCCESS;
}

EfiStatus UefiServices::exitBootServices() {
    if (!sSystemTable || !sSystemTable->bootServices || sBootServicesExited) {
        return EFI_NOT_READY;
    }
    
    auto bs = sSystemTable->bootServices;
    
    EfiMemoryDescriptor* memoryMap = nullptr;
    usize mapSize = 0;
    usize mapKey = 0;
    usize descriptorSize = 0;
    
    EfiStatus status = getMemoryMap(&memoryMap, &mapSize, &mapKey, &descriptorSize);
    if (efi_error(status)) {
        return status;
    }
    
    status = bs->exitBootServices(sImageHandle, mapKey);
    if (efi_error(status)) {
        status = getMemoryMap(&memoryMap, &mapSize, &mapKey, &descriptorSize);
        if (efi_error(status)) {
            return status;
        }
        status = bs->exitBootServices(sImageHandle, mapKey);
    }
    
    if (!efi_error(status)) {
        sBootServicesExited = true;
    }
    
    return status;
}

EfiGraphicsOutputProtocol* UefiServices::getGraphicsOutput() {
    if (!sSystemTable || !sSystemTable->bootServices || sBootServicesExited) {
        return nullptr;
    }
    
    EfiGraphicsOutputProtocol* gop = nullptr;
    EfiStatus status = sSystemTable->bootServices->locateProtocol(
        &EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,
        nullptr,
        reinterpret_cast<void**>(&gop)
    );
    
    if (efi_error(status)) {
        return nullptr;
    }
    
    return gop;
}

void* UefiServices::findConfigurationTable(const EfiGuid& guid) {
    if (!sSystemTable) {
        return nullptr;
    }
    
    for (usize i = 0; i < sSystemTable->numberOfTableEntries; i++) {
        auto& entry = sSystemTable->configurationTable[i];
        if (entry.vendorGuid.data1 == guid.data1 &&
            entry.vendorGuid.data2 == guid.data2 &&
            entry.vendorGuid.data3 == guid.data3) {
            bool match = true;
            for (int j = 0; j < 8; j++) {
                if (entry.vendorGuid.data4[j] != guid.data4[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return entry.vendorTable;
            }
        }
    }
    
    return nullptr;
}

}
