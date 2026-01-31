#pragma once

#include "../types.hpp"

namespace sertos::loader {

constexpr u32 MAX_SHARED_LIBRARIES = 64;
constexpr u32 MAX_SYMBOLS = 4096;
constexpr u32 MAX_RELOCATIONS = 8192;
constexpr u32 MAX_LIBRARY_NAME = 128;
constexpr u32 MAX_SYMBOL_NAME = 256;
constexpr u32 MAX_LOADED_LIBS_PER_PROCESS = 32;

constexpr u32 DT_NULL = 0;
constexpr u32 DT_NEEDED = 1;
constexpr u32 DT_PLTRELSZ = 2;
constexpr u32 DT_PLTGOT = 3;
constexpr u32 DT_HASH = 4;
constexpr u32 DT_STRTAB = 5;
constexpr u32 DT_SYMTAB = 6;
constexpr u32 DT_RELA = 7;
constexpr u32 DT_RELASZ = 8;
constexpr u32 DT_RELAENT = 9;
constexpr u32 DT_STRSZ = 10;
constexpr u32 DT_SYMENT = 11;
constexpr u32 DT_INIT = 12;
constexpr u32 DT_FINI = 13;
constexpr u32 DT_SONAME = 14;
constexpr u32 DT_RPATH = 15;
constexpr u32 DT_SYMBOLIC = 16;
constexpr u32 DT_REL = 17;
constexpr u32 DT_RELSZ = 18;
constexpr u32 DT_RELENT = 19;
constexpr u32 DT_PLTREL = 20;
constexpr u32 DT_DEBUG = 21;
constexpr u32 DT_TEXTREL = 22;
constexpr u32 DT_JMPREL = 23;
constexpr u32 DT_BIND_NOW = 24;
constexpr u32 DT_INIT_ARRAY = 25;
constexpr u32 DT_FINI_ARRAY = 26;
constexpr u32 DT_INIT_ARRAYSZ = 27;
constexpr u32 DT_FINI_ARRAYSZ = 28;

constexpr u32 R_X86_64_NONE = 0;
constexpr u32 R_X86_64_64 = 1;
constexpr u32 R_X86_64_PC32 = 2;
constexpr u32 R_X86_64_GOT32 = 3;
constexpr u32 R_X86_64_PLT32 = 4;
constexpr u32 R_X86_64_COPY = 5;
constexpr u32 R_X86_64_GLOB_DAT = 6;
constexpr u32 R_X86_64_JUMP_SLOT = 7;
constexpr u32 R_X86_64_RELATIVE = 8;
constexpr u32 R_X86_64_GOTPCREL = 9;
constexpr u32 R_X86_64_32 = 10;
constexpr u32 R_X86_64_32S = 11;
constexpr u32 R_X86_64_16 = 12;
constexpr u32 R_X86_64_PC16 = 13;
constexpr u32 R_X86_64_8 = 14;
constexpr u32 R_X86_64_PC8 = 15;

constexpr u8 STB_LOCAL = 0;
constexpr u8 STB_GLOBAL = 1;
constexpr u8 STB_WEAK = 2;

constexpr u8 STT_NOTYPE = 0;
constexpr u8 STT_OBJECT = 1;
constexpr u8 STT_FUNC = 2;
constexpr u8 STT_SECTION = 3;
constexpr u8 STT_FILE = 4;

struct Elf64Dyn {
    i64 tag;
    union {
        u64 val;
        u64 ptr;
    } un;
};

struct Elf64Sym {
    u32 name;
    u8 info;
    u8 other;
    u16 shndx;
    u64 value;
    u64 size;
};

struct Elf64Rela {
    u64 offset;
    u64 info;
    i64 addend;
};

struct Elf64Rel {
    u64 offset;
    u64 info;
};

struct Symbol {
    char name[MAX_SYMBOL_NAME];
    u64 address;
    u64 size;
    u8 binding;
    u8 type;
    u32 libraryId;
    bool defined;
};

struct Relocation {
    u64 offset;
    u32 type;
    u32 symbolIndex;
    i64 addend;
};

enum class LibraryState : u8 {
    Invalid = 0,
    Loading,
    Loaded,
    Initialized,
    Unloading
};

struct SharedLibrary {
    u32 id;
    char name[MAX_LIBRARY_NAME];
    char path[MAX_LIBRARY_NAME];
    LibraryState state;
    
    u64 baseAddress;
    u64 loadedSize;
    
    u64 dynamicSection;
    u64 stringTable;
    u64 symbolTable;
    u64 hashTable;
    u64 gotPlt;
    u64 pltRel;
    u64 relaSection;
    u64 relSection;
    
    u64 initFunction;
    u64 finiFunction;
    u64 initArray;
    u64 finiArray;
    u32 initArraySize;
    u32 finiArraySize;
    
    u32 symbolCount;
    u32 stringTableSize;
    u32 relaCount;
    u32 relCount;
    u32 pltRelCount;
    
    u32 refCount;
    u32 dependencies[MAX_LOADED_LIBS_PER_PROCESS];
    u32 dependencyCount;
    
    bool active;
};

struct ProcessLibraries {
    u32 pid;
    SharedLibrary* libraries[MAX_LOADED_LIBS_PER_PROCESS];
    u32 libraryCount;
    u64 ldBase;
};

class DynamicLinker {
public:
    static void initialize();
    
    static SharedLibrary* loadLibrary(const char* name, u32 pid);
    static bool unloadLibrary(SharedLibrary* lib, u32 pid);
    static SharedLibrary* findLibrary(const char* name);
    static SharedLibrary* getLibrary(u32 id);
    
    static u64 resolveSymbol(const char* name, u32 pid);
    static u64 resolveSymbolInLibrary(const char* name, SharedLibrary* lib);
    static Symbol* lookupSymbol(const char* name, u32 pid);
    
    static bool relocate(SharedLibrary* lib, u64 baseAddress);
    static bool bindNow(SharedLibrary* lib);
    static bool lazyBind(SharedLibrary* lib, u64 pltIndex);
    
    static bool initializeLibrary(SharedLibrary* lib);
    static bool finalizeLibrary(SharedLibrary* lib);
    
    static ProcessLibraries* getProcessLibraries(u32 pid);
    static bool addLibraryToProcess(SharedLibrary* lib, u32 pid);
    static bool removeLibraryFromProcess(SharedLibrary* lib, u32 pid);
    
    static void setLibrarySearchPath(const char* path);
    static const char* getLibrarySearchPath();
    
    static bool isInitialized();

private:
    static SharedLibrary* loadElfLibrary(const char* path, u64 baseAddress);
    static bool parseDynamicSection(SharedLibrary* lib);
    static bool loadDependencies(SharedLibrary* lib, u32 pid);
    
    static u64 elfHash(const char* name);
    static u64 gnuHash(const char* name);
    static Symbol* hashLookup(SharedLibrary* lib, const char* name);
    
    static bool applyRelocation(SharedLibrary* lib, Elf64Rela* rela, u64 baseAddress);
    static bool applyRelocationRel(SharedLibrary* lib, Elf64Rel* rel, u64 baseAddress);
    
    static u64 allocateLibrarySpace(usize size);
    static void freeLibrarySpace(u64 address, usize size);
    
    static SharedLibrary sLibraries[MAX_SHARED_LIBRARIES];
    static Symbol sSymbols[MAX_SYMBOLS];
    static ProcessLibraries sProcessLibraries[256];
    static char sSearchPath[512];
    static u32 sLibraryCount;
    static u32 sSymbolCount;
    static u64 sNextLibraryBase;
    static bool sInitialized;
};

using DL = DynamicLinker;

}
