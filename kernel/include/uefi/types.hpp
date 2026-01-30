#pragma once

#include "../types.hpp"

namespace sertos::uefi {

using EfiStatus = u64;
using EfiHandle = void*;
using EfiEvent = void*;
using EfiPhysicalAddress = u64;
using EfiVirtualAddress = u64;
using Char16 = u16;

constexpr u64 EFI_ERROR_BIT = 0x8000000000000000ULL;

constexpr EfiStatus EFI_SUCCESS = 0;
constexpr EfiStatus EFI_LOAD_ERROR = EFI_ERROR_BIT | 1;
constexpr EfiStatus EFI_INVALID_PARAMETER = EFI_ERROR_BIT | 2;
constexpr EfiStatus EFI_UNSUPPORTED = EFI_ERROR_BIT | 3;
constexpr EfiStatus EFI_BAD_BUFFER_SIZE = EFI_ERROR_BIT | 4;
constexpr EfiStatus EFI_BUFFER_TOO_SMALL = EFI_ERROR_BIT | 5;
constexpr EfiStatus EFI_NOT_READY = EFI_ERROR_BIT | 6;
constexpr EfiStatus EFI_DEVICE_ERROR = EFI_ERROR_BIT | 7;
constexpr EfiStatus EFI_WRITE_PROTECTED = EFI_ERROR_BIT | 8;
constexpr EfiStatus EFI_OUT_OF_RESOURCES = EFI_ERROR_BIT | 9;
constexpr EfiStatus EFI_NOT_FOUND = EFI_ERROR_BIT | 14;

inline bool efi_error(EfiStatus status) {
    return (status & EFI_ERROR_BIT) != 0;
}

enum class EfiMemoryType : u32 {
    ReservedMemoryType,
    LoaderCode,
    LoaderData,
    BootServicesCode,
    BootServicesData,
    RuntimeServicesCode,
    RuntimeServicesData,
    ConventionalMemory,
    UnusableMemory,
    ACPIReclaimMemory,
    ACPIMemoryNVS,
    MemoryMappedIO,
    MemoryMappedIOPortSpace,
    PalCode,
    PersistentMemory,
    MaxMemoryType
};

enum class EfiAllocateType : u32 {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
};

struct EfiMemoryDescriptor {
    u32 type;
    EfiPhysicalAddress physicalStart;
    EfiVirtualAddress virtualStart;
    u64 numberOfPages;
    u64 attribute;
};

struct EfiGuid {
    u32 data1;
    u16 data2;
    u16 data3;
    u8 data4[8];
};

constexpr EfiGuid EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID = {
    0x9042a9de, 0x23dc, 0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};

constexpr EfiGuid EFI_ACPI_20_TABLE_GUID = {
    0x8868e871, 0xe4f1, 0x11d3,
    {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}
};

constexpr EfiGuid EFI_LOADED_IMAGE_PROTOCOL_GUID = {
    0x5b1b31a1, 0x9562, 0x11d2,
    {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

constexpr EfiGuid EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID = {
    0x964e5b22, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

}
