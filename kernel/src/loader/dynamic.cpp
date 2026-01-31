#include "../../include/loader/dynamic.hpp"
#include "../../include/loader/elf.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/memory/vmm.hpp"
#include "../../include/fs/sertfs.hpp"
#include "../../include/process/process.hpp"

namespace sertos::loader {

namespace {

void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

void memcpy(void* dest, const void* src, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    const u8* s = reinterpret_cast<const u8*>(src);
    for (usize i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

void strcpy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

bool strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) return false;
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

usize strlen(const char* s) {
    usize len = 0;
    while (s[len]) len++;
    return len;
}

void strcat(char* dest, const char* src, usize maxLen) {
    usize destLen = strlen(dest);
    usize i = 0;
    while (src[i] && destLen + i < maxLen - 1) {
        dest[destLen + i] = src[i];
        i++;
    }
    dest[destLen + i] = '\0';
}

}

SharedLibrary DynamicLinker::sLibraries[MAX_SHARED_LIBRARIES];
Symbol DynamicLinker::sSymbols[MAX_SYMBOLS];
ProcessLibraries DynamicLinker::sProcessLibraries[256];
char DynamicLinker::sSearchPath[512] = "/lib:/usr/lib";
u32 DynamicLinker::sLibraryCount = 0;
u32 DynamicLinker::sSymbolCount = 0;
u64 DynamicLinker::sNextLibraryBase = 0x7F0000000000ULL;
bool DynamicLinker::sInitialized = false;

void DynamicLinker::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_SHARED_LIBRARIES; i++) {
        memset(&sLibraries[i], 0, sizeof(SharedLibrary));
        sLibraries[i].state = LibraryState::Invalid;
        sLibraries[i].active = false;
    }
    
    for (u32 i = 0; i < MAX_SYMBOLS; i++) {
        memset(&sSymbols[i], 0, sizeof(Symbol));
        sSymbols[i].defined = false;
    }
    
    for (u32 i = 0; i < 256; i++) {
        memset(&sProcessLibraries[i], 0, sizeof(ProcessLibraries));
        sProcessLibraries[i].pid = 0;
        sProcessLibraries[i].libraryCount = 0;
    }
    
    sLibraryCount = 0;
    sSymbolCount = 0;
    sNextLibraryBase = 0x7F0000000000ULL;
    sInitialized = true;
}

SharedLibrary* DynamicLinker::loadLibrary(const char* name, u32 pid) {
    if (!sInitialized || !name) return nullptr;
    
    SharedLibrary* existing = findLibrary(name);
    if (existing) {
        existing->refCount++;
        addLibraryToProcess(existing, pid);
        return existing;
    }
    
    char fullPath[MAX_LIBRARY_NAME * 2];
    bool found = false;
    
    if (name[0] == '/') {
        strcpy(fullPath, name, sizeof(fullPath));
        found = fs::SertFs::exists(fullPath);
    } else {
        char searchPath[512];
        strcpy(searchPath, sSearchPath, sizeof(searchPath));
        
        char* path = searchPath;
        while (*path && !found) {
            char* end = path;
            while (*end && *end != ':') end++;
            
            char savedChar = *end;
            *end = '\0';
            
            strcpy(fullPath, path, sizeof(fullPath));
            strcat(fullPath, "/", sizeof(fullPath));
            strcat(fullPath, name, sizeof(fullPath));
            
            if (fs::SertFs::exists(fullPath)) {
                found = true;
            }
            
            *end = savedChar;
            path = savedChar ? end + 1 : end;
        }
    }
    
    if (!found) return nullptr;
    
    u64 baseAddress = allocateLibrarySpace(0x1000000);
    if (baseAddress == 0) return nullptr;
    
    SharedLibrary* lib = loadElfLibrary(fullPath, baseAddress);
    if (!lib) {
        freeLibrarySpace(baseAddress, 0x1000000);
        return nullptr;
    }
    
    strcpy(lib->name, name, MAX_LIBRARY_NAME);
    strcpy(lib->path, fullPath, MAX_LIBRARY_NAME);
    lib->refCount = 1;
    
    if (!parseDynamicSection(lib)) {
        freeLibrarySpace(baseAddress, lib->loadedSize);
        lib->state = LibraryState::Invalid;
        lib->active = false;
        return nullptr;
    }
    
    if (!loadDependencies(lib, pid)) {
        freeLibrarySpace(baseAddress, lib->loadedSize);
        lib->state = LibraryState::Invalid;
        lib->active = false;
        return nullptr;
    }
    
    if (!relocate(lib, baseAddress)) {
        freeLibrarySpace(baseAddress, lib->loadedSize);
        lib->state = LibraryState::Invalid;
        lib->active = false;
        return nullptr;
    }
    
    lib->state = LibraryState::Loaded;
    
    addLibraryToProcess(lib, pid);
    
    if (!initializeLibrary(lib)) {
        unloadLibrary(lib, pid);
        return nullptr;
    }
    
    lib->state = LibraryState::Initialized;
    
    return lib;
}

bool DynamicLinker::unloadLibrary(SharedLibrary* lib, u32 pid) {
    if (!sInitialized || !lib) return false;
    
    removeLibraryFromProcess(lib, pid);
    
    lib->refCount--;
    
    if (lib->refCount > 0) return true;
    
    lib->state = LibraryState::Unloading;
    
    finalizeLibrary(lib);
    
    for (u32 i = 0; i < lib->dependencyCount; i++) {
        SharedLibrary* dep = getLibrary(lib->dependencies[i]);
        if (dep) {
            unloadLibrary(dep, pid);
        }
    }
    
    freeLibrarySpace(lib->baseAddress, lib->loadedSize);
    
    lib->state = LibraryState::Invalid;
    lib->active = false;
    sLibraryCount--;
    
    return true;
}

SharedLibrary* DynamicLinker::findLibrary(const char* name) {
    if (!sInitialized || !name) return nullptr;
    
    for (u32 i = 0; i < MAX_SHARED_LIBRARIES; i++) {
        if (sLibraries[i].active && strcmp(sLibraries[i].name, name)) {
            return &sLibraries[i];
        }
    }
    
    return nullptr;
}

SharedLibrary* DynamicLinker::getLibrary(u32 id) {
    if (!sInitialized || id >= MAX_SHARED_LIBRARIES) return nullptr;
    
    if (sLibraries[id].active) {
        return &sLibraries[id];
    }
    
    return nullptr;
}

u64 DynamicLinker::resolveSymbol(const char* name, u32 pid) {
    if (!sInitialized || !name) return 0;
    
    ProcessLibraries* procLibs = getProcessLibraries(pid);
    if (!procLibs) return 0;
    
    for (u32 i = 0; i < procLibs->libraryCount; i++) {
        SharedLibrary* lib = procLibs->libraries[i];
        if (!lib) continue;
        
        u64 addr = resolveSymbolInLibrary(name, lib);
        if (addr != 0) return addr;
    }
    
    return 0;
}

u64 DynamicLinker::resolveSymbolInLibrary(const char* name, SharedLibrary* lib) {
    if (!sInitialized || !name || !lib) return 0;
    
    Symbol* sym = hashLookup(lib, name);
    if (sym && sym->defined) {
        return sym->address;
    }
    
    return 0;
}

Symbol* DynamicLinker::lookupSymbol(const char* name, u32 pid) {
    if (!sInitialized || !name) return nullptr;
    
    ProcessLibraries* procLibs = getProcessLibraries(pid);
    if (!procLibs) return nullptr;
    
    for (u32 i = 0; i < procLibs->libraryCount; i++) {
        SharedLibrary* lib = procLibs->libraries[i];
        if (!lib) continue;
        
        Symbol* sym = hashLookup(lib, name);
        if (sym && sym->defined) return sym;
    }
    
    return nullptr;
}

bool DynamicLinker::relocate(SharedLibrary* lib, u64 baseAddress) {
    if (!sInitialized || !lib) return false;
    
    if (lib->relaSection && lib->relaCount > 0) {
        Elf64Rela* rela = reinterpret_cast<Elf64Rela*>(lib->relaSection);
        for (u32 i = 0; i < lib->relaCount; i++) {
            if (!applyRelocation(lib, &rela[i], baseAddress)) {
                return false;
            }
        }
    }
    
    if (lib->relSection && lib->relCount > 0) {
        Elf64Rel* rel = reinterpret_cast<Elf64Rel*>(lib->relSection);
        for (u32 i = 0; i < lib->relCount; i++) {
            if (!applyRelocationRel(lib, &rel[i], baseAddress)) {
                return false;
            }
        }
    }
    
    if (lib->pltRel && lib->pltRelCount > 0) {
        Elf64Rela* rela = reinterpret_cast<Elf64Rela*>(lib->pltRel);
        for (u32 i = 0; i < lib->pltRelCount; i++) {
            if (!applyRelocation(lib, &rela[i], baseAddress)) {
                return false;
            }
        }
    }
    
    return true;
}

bool DynamicLinker::bindNow(SharedLibrary* lib) {
    if (!sInitialized || !lib) return false;
    
    return relocate(lib, lib->baseAddress);
}

bool DynamicLinker::lazyBind(SharedLibrary* lib, u64 pltIndex) {
    if (!sInitialized || !lib) return false;
    
    if (!lib->pltRel || pltIndex >= lib->pltRelCount) return false;
    
    Elf64Rela* rela = reinterpret_cast<Elf64Rela*>(lib->pltRel) + pltIndex;
    
    return applyRelocation(lib, rela, lib->baseAddress);
}

bool DynamicLinker::initializeLibrary(SharedLibrary* lib) {
    if (!sInitialized || !lib) return false;
    
    if (lib->initFunction) {
        using InitFunc = void (*)();
        InitFunc init = reinterpret_cast<InitFunc>(lib->initFunction);
        init();
    }
    
    if (lib->initArray && lib->initArraySize > 0) {
        using InitFunc = void (*)();
        InitFunc* initArray = reinterpret_cast<InitFunc*>(lib->initArray);
        u32 count = lib->initArraySize / sizeof(InitFunc);
        
        for (u32 i = 0; i < count; i++) {
            if (initArray[i]) {
                initArray[i]();
            }
        }
    }
    
    return true;
}

bool DynamicLinker::finalizeLibrary(SharedLibrary* lib) {
    if (!sInitialized || !lib) return false;
    
    if (lib->finiArray && lib->finiArraySize > 0) {
        using FiniFunc = void (*)();
        FiniFunc* finiArray = reinterpret_cast<FiniFunc*>(lib->finiArray);
        u32 count = lib->finiArraySize / sizeof(FiniFunc);
        
        for (i32 i = static_cast<i32>(count) - 1; i >= 0; i--) {
            if (finiArray[i]) {
                finiArray[i]();
            }
        }
    }
    
    if (lib->finiFunction) {
        using FiniFunc = void (*)();
        FiniFunc fini = reinterpret_cast<FiniFunc>(lib->finiFunction);
        fini();
    }
    
    return true;
}

ProcessLibraries* DynamicLinker::getProcessLibraries(u32 pid) {
    if (!sInitialized) return nullptr;
    
    for (u32 i = 0; i < 256; i++) {
        if (sProcessLibraries[i].pid == pid) {
            return &sProcessLibraries[i];
        }
    }
    
    for (u32 i = 0; i < 256; i++) {
        if (sProcessLibraries[i].pid == 0) {
            sProcessLibraries[i].pid = pid;
            sProcessLibraries[i].libraryCount = 0;
            return &sProcessLibraries[i];
        }
    }
    
    return nullptr;
}

bool DynamicLinker::addLibraryToProcess(SharedLibrary* lib, u32 pid) {
    if (!sInitialized || !lib) return false;
    
    ProcessLibraries* procLibs = getProcessLibraries(pid);
    if (!procLibs) return false;
    
    for (u32 i = 0; i < procLibs->libraryCount; i++) {
        if (procLibs->libraries[i] == lib) return true;
    }
    
    if (procLibs->libraryCount >= MAX_LOADED_LIBS_PER_PROCESS) return false;
    
    procLibs->libraries[procLibs->libraryCount++] = lib;
    
    return true;
}

bool DynamicLinker::removeLibraryFromProcess(SharedLibrary* lib, u32 pid) {
    if (!sInitialized || !lib) return false;
    
    ProcessLibraries* procLibs = getProcessLibraries(pid);
    if (!procLibs) return false;
    
    for (u32 i = 0; i < procLibs->libraryCount; i++) {
        if (procLibs->libraries[i] == lib) {
            for (u32 j = i; j < procLibs->libraryCount - 1; j++) {
                procLibs->libraries[j] = procLibs->libraries[j + 1];
            }
            procLibs->libraryCount--;
            return true;
        }
    }
    
    return false;
}

void DynamicLinker::setLibrarySearchPath(const char* path) {
    if (path) {
        strcpy(sSearchPath, path, sizeof(sSearchPath));
    }
}

const char* DynamicLinker::getLibrarySearchPath() {
    return sSearchPath;
}

bool DynamicLinker::isInitialized() {
    return sInitialized;
}

SharedLibrary* DynamicLinker::loadElfLibrary(const char* path, u64 baseAddress) {
    if (!path) return nullptr;
    
    fs::FileHandle file = fs::SertFs::open(path, fs::O_READ);
    if (!file.valid) return nullptr;
    
    SharedLibrary* lib = nullptr;
    for (u32 i = 0; i < MAX_SHARED_LIBRARIES; i++) {
        if (!sLibraries[i].active) {
            lib = &sLibraries[i];
            lib->id = i;
            break;
        }
    }
    
    if (!lib) {
        fs::SertFs::close(&file);
        return nullptr;
    }
    
    memset(lib, 0, sizeof(SharedLibrary));
    lib->state = LibraryState::Loading;
    lib->baseAddress = baseAddress;
    lib->active = true;
    sLibraryCount++;
    
    fs::SertFs::close(&file);
    
    return lib;
}

bool DynamicLinker::parseDynamicSection(SharedLibrary* lib) {
    if (!lib || !lib->dynamicSection) return true;
    
    Elf64Dyn* dyn = reinterpret_cast<Elf64Dyn*>(lib->dynamicSection);
    
    while (dyn->tag != DT_NULL) {
        switch (dyn->tag) {
            case DT_STRTAB:
                lib->stringTable = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_SYMTAB:
                lib->symbolTable = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_HASH:
                lib->hashTable = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_PLTGOT:
                lib->gotPlt = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_JMPREL:
                lib->pltRel = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_PLTRELSZ:
                lib->pltRelCount = static_cast<u32>(dyn->un.val / sizeof(Elf64Rela));
                break;
            case DT_RELA:
                lib->relaSection = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_RELASZ:
                lib->relaCount = static_cast<u32>(dyn->un.val / sizeof(Elf64Rela));
                break;
            case DT_REL:
                lib->relSection = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_RELSZ:
                lib->relCount = static_cast<u32>(dyn->un.val / sizeof(Elf64Rel));
                break;
            case DT_STRSZ:
                lib->stringTableSize = static_cast<u32>(dyn->un.val);
                break;
            case DT_INIT:
                lib->initFunction = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_FINI:
                lib->finiFunction = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_INIT_ARRAY:
                lib->initArray = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_FINI_ARRAY:
                lib->finiArray = lib->baseAddress + dyn->un.ptr;
                break;
            case DT_INIT_ARRAYSZ:
                lib->initArraySize = static_cast<u32>(dyn->un.val);
                break;
            case DT_FINI_ARRAYSZ:
                lib->finiArraySize = static_cast<u32>(dyn->un.val);
                break;
        }
        dyn++;
    }
    
    return true;
}

bool DynamicLinker::loadDependencies(SharedLibrary* lib, u32 pid) {
    if (!lib || !lib->dynamicSection) return true;
    
    Elf64Dyn* dyn = reinterpret_cast<Elf64Dyn*>(lib->dynamicSection);
    
    while (dyn->tag != DT_NULL) {
        if (dyn->tag == DT_NEEDED) {
            const char* depName = reinterpret_cast<const char*>(lib->stringTable + dyn->un.val);
            
            SharedLibrary* dep = loadLibrary(depName, pid);
            if (!dep) {
                return false;
            }
            
            if (lib->dependencyCount < MAX_LOADED_LIBS_PER_PROCESS) {
                lib->dependencies[lib->dependencyCount++] = dep->id;
            }
        }
        dyn++;
    }
    
    return true;
}

u64 DynamicLinker::elfHash(const char* name) {
    u64 h = 0;
    u64 g;
    
    while (*name) {
        h = (h << 4) + static_cast<u8>(*name++);
        g = h & 0xF0000000;
        if (g) {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    
    return h;
}

u64 DynamicLinker::gnuHash(const char* name) {
    u64 h = 5381;
    
    while (*name) {
        h = (h << 5) + h + static_cast<u8>(*name++);
    }
    
    return h;
}

Symbol* DynamicLinker::hashLookup(SharedLibrary* lib, const char* name) {
    if (!lib || !name || !lib->symbolTable || !lib->stringTable) return nullptr;
    
    Elf64Sym* symtab = reinterpret_cast<Elf64Sym*>(lib->symbolTable);
    const char* strtab = reinterpret_cast<const char*>(lib->stringTable);
    
    for (u32 i = 0; i < lib->symbolCount; i++) {
        const char* symName = strtab + symtab[i].name;
        if (strcmp(symName, name)) {
            if (sSymbolCount < MAX_SYMBOLS) {
                Symbol* sym = &sSymbols[sSymbolCount++];
                strcpy(sym->name, symName, MAX_SYMBOL_NAME);
                sym->address = lib->baseAddress + symtab[i].value;
                sym->size = symtab[i].size;
                sym->binding = symtab[i].info >> 4;
                sym->type = symtab[i].info & 0xF;
                sym->libraryId = lib->id;
                sym->defined = symtab[i].shndx != 0;
                return sym;
            }
        }
    }
    
    return nullptr;
}

bool DynamicLinker::applyRelocation(SharedLibrary* lib, Elf64Rela* rela, u64 baseAddress) {
    if (!lib || !rela) return false;
    
    u32 type = static_cast<u32>(rela->info & 0xFFFFFFFF);
    u32 symIndex = static_cast<u32>(rela->info >> 32);
    
    u64* target = reinterpret_cast<u64*>(baseAddress + rela->offset);
    u64 symValue = 0;
    
    if (symIndex != 0 && lib->symbolTable && lib->stringTable) {
        Elf64Sym* symtab = reinterpret_cast<Elf64Sym*>(lib->symbolTable);
        const char* strtab = reinterpret_cast<const char*>(lib->stringTable);
        const char* symName = strtab + symtab[symIndex].name;
        
        symValue = resolveSymbol(symName, 0);
        if (symValue == 0 && symtab[symIndex].shndx != 0) {
            symValue = baseAddress + symtab[symIndex].value;
        }
    }
    
    switch (type) {
        case R_X86_64_NONE:
            break;
        case R_X86_64_64:
            *target = symValue + rela->addend;
            break;
        case R_X86_64_PC32:
            *reinterpret_cast<u32*>(target) = static_cast<u32>(symValue + rela->addend - reinterpret_cast<u64>(target));
            break;
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
            *target = symValue;
            break;
        case R_X86_64_RELATIVE:
            *target = baseAddress + rela->addend;
            break;
        case R_X86_64_32:
            *reinterpret_cast<u32*>(target) = static_cast<u32>(symValue + rela->addend);
            break;
        case R_X86_64_32S:
            *reinterpret_cast<i32*>(target) = static_cast<i32>(symValue + rela->addend);
            break;
        default:
            return false;
    }
    
    return true;
}

bool DynamicLinker::applyRelocationRel(SharedLibrary* lib, Elf64Rel* rel, u64 baseAddress) {
    if (!lib || !rel) return false;
    
    Elf64Rela rela;
    rela.offset = rel->offset;
    rela.info = rel->info;
    rela.addend = 0;
    
    return applyRelocation(lib, &rela, baseAddress);
}

u64 DynamicLinker::allocateLibrarySpace(usize size) {
    u64 addr = sNextLibraryBase;
    sNextLibraryBase += size;
    sNextLibraryBase = (sNextLibraryBase + 0xFFF) & ~0xFFFULL;
    return addr;
}

void DynamicLinker::freeLibrarySpace(u64, usize) {
}

}
