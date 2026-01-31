#pragma once

#include "../types.hpp"
#include "../process/process.hpp"

namespace sertos::loader {

constexpr u32 ELF_MAGIC = 0x464C457F;

constexpr u8 ELFCLASS64 = 2;
constexpr u8 ELFDATA2LSB = 1;
constexpr u16 ET_EXEC = 2;
constexpr u16 ET_DYN = 3;
constexpr u16 EM_X86_64 = 62;

constexpr u32 PT_NULL = 0;
constexpr u32 PT_LOAD = 1;
constexpr u32 PT_DYNAMIC = 2;
constexpr u32 PT_INTERP = 3;
constexpr u32 PT_NOTE = 4;
constexpr u32 PT_PHDR = 6;
constexpr u32 PT_TLS = 7;

constexpr u32 PF_X = 0x1;
constexpr u32 PF_W = 0x2;
constexpr u32 PF_R = 0x4;

struct Elf64Header {
    u8 ident[16];
    u16 type;
    u16 machine;
    u32 version;
    u64 entry;
    u64 phoff;
    u64 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} __attribute__((packed));

struct Elf64ProgramHeader {
    u32 type;
    u32 flags;
    u64 offset;
    u64 vaddr;
    u64 paddr;
    u64 filesz;
    u64 memsz;
    u64 align;
} __attribute__((packed));

struct Elf64SectionHeader {
    u32 name;
    u32 type;
    u64 flags;
    u64 addr;
    u64 offset;
    u64 size;
    u32 link;
    u32 info;
    u64 addralign;
    u64 entsize;
} __attribute__((packed));

enum class ElfError {
    Success = 0,
    InvalidMagic,
    InvalidClass,
    InvalidEndian,
    InvalidType,
    InvalidMachine,
    LoadFailed,
    MemoryError,
    FileError
};

struct ElfLoadResult {
    ElfError error;
    u64 entryPoint;
    u64 baseAddress;
    u64 endAddress;
};

class ElfLoader {
public:
    static ElfLoadResult load(process::Process* proc, const u8* data, usize size);
    static bool validate(const u8* data, usize size);
    static const Elf64Header* getHeader(const u8* data);

private:
    static ElfError loadSegment(process::Process* proc, const u8* data, const Elf64ProgramHeader* phdr);
    static u64 elfFlagsToPageFlags(u32 elfFlags);
};

}
