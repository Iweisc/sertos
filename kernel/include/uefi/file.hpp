#pragma once

#include "types.hpp"
#include "tables.hpp"

namespace sertos::uefi {

constexpr u64 EFI_FILE_MODE_READ = 0x0000000000000001ULL;
constexpr u64 EFI_FILE_MODE_WRITE = 0x0000000000000002ULL;
constexpr u64 EFI_FILE_MODE_CREATE = 0x8000000000000000ULL;

constexpr u64 EFI_FILE_READ_ONLY = 0x0000000000000001ULL;
constexpr u64 EFI_FILE_HIDDEN = 0x0000000000000002ULL;
constexpr u64 EFI_FILE_SYSTEM = 0x0000000000000004ULL;
constexpr u64 EFI_FILE_RESERVED = 0x0000000000000008ULL;
constexpr u64 EFI_FILE_DIRECTORY = 0x0000000000000010ULL;
constexpr u64 EFI_FILE_ARCHIVE = 0x0000000000000020ULL;

struct EfiFileInfo {
    u64 size;
    u64 fileSize;
    u64 physicalSize;
    EfiTime createTime;
    EfiTime lastAccessTime;
    EfiTime modificationTime;
    u64 attribute;
    Char16 fileName[1];
};

constexpr EfiGuid EFI_FILE_INFO_GUID = {
    0x09576e92, 0x6d3f, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

struct EfiFileProtocol {
    u64 revision;
    EfiStatus (*open)(EfiFileProtocol* self, EfiFileProtocol** newHandle, const Char16* fileName, u64 openMode, u64 attributes);
    EfiStatus (*close)(EfiFileProtocol* self);
    EfiStatus (*del)(EfiFileProtocol* self);
    EfiStatus (*read)(EfiFileProtocol* self, usize* bufferSize, void* buffer);
    EfiStatus (*write)(EfiFileProtocol* self, usize* bufferSize, const void* buffer);
    EfiStatus (*getPosition)(EfiFileProtocol* self, u64* position);
    EfiStatus (*setPosition)(EfiFileProtocol* self, u64 position);
    EfiStatus (*getInfo)(EfiFileProtocol* self, const EfiGuid* informationType, usize* bufferSize, void* buffer);
    EfiStatus (*setInfo)(EfiFileProtocol* self, const EfiGuid* informationType, usize bufferSize, const void* buffer);
    EfiStatus (*flush)(EfiFileProtocol* self);
};

struct EfiSimpleFileSystemProtocol {
    u64 revision;
    EfiStatus (*openVolume)(EfiSimpleFileSystemProtocol* self, EfiFileProtocol** root);
};

struct EfiLoadedImageProtocol {
    u32 revision;
    EfiHandle parentHandle;
    EfiSystemTable* systemTable;
    EfiHandle deviceHandle;
    void* filePath;
    void* reserved;
    u32 loadOptionsSize;
    void* loadOptions;
    void* imageBase;
    u64 imageSize;
    EfiMemoryType imageCodeType;
    EfiMemoryType imageDataType;
    EfiStatus (*unload)(EfiHandle imageHandle);
};

}
