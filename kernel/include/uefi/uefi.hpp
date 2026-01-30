#pragma once

#include "types.hpp"
#include "tables.hpp"
#include "graphics.hpp"
#include "file.hpp"

namespace sertos::uefi {

class UefiServices {
public:
    static void initialize(EfiHandle imageHandle, EfiSystemTable* systemTable);
    static EfiSystemTable* systemTable();
    static EfiBootServices* bootServices();
    static EfiRuntimeServices* runtimeServices();
    static EfiHandle imageHandle();
    static bool bootServicesExited();
    
    static void print(const char* str);
    static void print(const Char16* str);
    static void println(const char* str);
    
    static EfiStatus getMemoryMap(EfiMemoryDescriptor** map, usize* mapSize, usize* mapKey, usize* descriptorSize);
    static EfiStatus exitBootServices();
    
    static EfiGraphicsOutputProtocol* getGraphicsOutput();
    static void* findConfigurationTable(const EfiGuid& guid);

private:
    static EfiHandle sImageHandle;
    static EfiSystemTable* sSystemTable;
    static bool sBootServicesExited;
};

}
