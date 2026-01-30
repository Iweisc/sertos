#pragma once

#include "types.hpp"

namespace sertos::uefi {

struct EfiTableHeader {
    u64 signature;
    u32 revision;
    u32 headerSize;
    u32 crc32;
    u32 reserved;
};

struct EfiSimpleTextInputProtocol;
struct EfiSimpleTextOutputProtocol;
struct EfiRuntimeServices;
struct EfiBootServices;
struct EfiConfigurationTable;

struct EfiSimpleTextOutputProtocol {
    EfiStatus (*reset)(EfiSimpleTextOutputProtocol* self, bool extendedVerification);
    EfiStatus (*outputString)(EfiSimpleTextOutputProtocol* self, const Char16* string);
    EfiStatus (*testString)(EfiSimpleTextOutputProtocol* self, const Char16* string);
    EfiStatus (*queryMode)(EfiSimpleTextOutputProtocol* self, usize modeNumber, usize* columns, usize* rows);
    EfiStatus (*setMode)(EfiSimpleTextOutputProtocol* self, usize modeNumber);
    EfiStatus (*setAttribute)(EfiSimpleTextOutputProtocol* self, usize attribute);
    EfiStatus (*clearScreen)(EfiSimpleTextOutputProtocol* self);
    EfiStatus (*setCursorPosition)(EfiSimpleTextOutputProtocol* self, usize column, usize row);
    EfiStatus (*enableCursor)(EfiSimpleTextOutputProtocol* self, bool visible);
    void* mode;
};

struct EfiInputKey {
    u16 scanCode;
    Char16 unicodeChar;
};

struct EfiSimpleTextInputProtocol {
    EfiStatus (*reset)(EfiSimpleTextInputProtocol* self, bool extendedVerification);
    EfiStatus (*readKeyStroke)(EfiSimpleTextInputProtocol* self, EfiInputKey* key);
    EfiEvent waitForKey;
};

struct EfiBootServices {
    EfiTableHeader header;
    
    void* raiseTPL;
    void* restoreTPL;
    
    EfiStatus (*allocatePages)(EfiAllocateType type, EfiMemoryType memoryType, usize pages, EfiPhysicalAddress* memory);
    EfiStatus (*freePages)(EfiPhysicalAddress memory, usize pages);
    EfiStatus (*getMemoryMap)(usize* memoryMapSize, EfiMemoryDescriptor* memoryMap, usize* mapKey, usize* descriptorSize, u32* descriptorVersion);
    EfiStatus (*allocatePool)(EfiMemoryType poolType, usize size, void** buffer);
    EfiStatus (*freePool)(void* buffer);
    
    void* createEvent;
    void* setTimer;
    void* waitForEvent;
    void* signalEvent;
    void* closeEvent;
    void* checkEvent;
    
    void* installProtocolInterface;
    void* reinstallProtocolInterface;
    void* uninstallProtocolInterface;
    EfiStatus (*handleProtocol)(EfiHandle handle, const EfiGuid* protocol, void** interface);
    void* reserved;
    void* registerProtocolNotify;
    EfiStatus (*locateHandle)(u32 searchType, const EfiGuid* protocol, void* searchKey, usize* bufferSize, EfiHandle* buffer);
    void* locateDevicePath;
    void* installConfigurationTable;
    
    void* loadImage;
    void* startImage;
    void* exit;
    void* unloadImage;
    EfiStatus (*exitBootServices)(EfiHandle imageHandle, usize mapKey);
    
    void* getNextMonotonicCount;
    void* stall;
    EfiStatus (*setWatchdogTimer)(usize timeout, u64 watchdogCode, usize dataSize, const Char16* watchdogData);
    
    void* connectController;
    void* disconnectController;
    
    void* openProtocol;
    void* closeProtocol;
    void* openProtocolInformation;
    
    void* protocolsPerHandle;
    EfiStatus (*locateHandleBuffer)(u32 searchType, const EfiGuid* protocol, void* searchKey, usize* noHandles, EfiHandle** buffer);
    EfiStatus (*locateProtocol)(const EfiGuid* protocol, void* registration, void** interface);
    void* installMultipleProtocolInterfaces;
    void* uninstallMultipleProtocolInterfaces;
    
    void* calculateCrc32;
    
    void* copyMem;
    void* setMem;
    void* createEventEx;
};

struct EfiTime {
    u16 year;
    u8 month;
    u8 day;
    u8 hour;
    u8 minute;
    u8 second;
    u8 pad1;
    u32 nanosecond;
    i16 timeZone;
    u8 daylight;
    u8 pad2;
};

struct EfiTimeCapabilities {
    u32 resolution;
    u32 accuracy;
    bool setsToZero;
};

struct EfiRuntimeServices {
    EfiTableHeader header;
    
    EfiStatus (*getTime)(EfiTime* time, EfiTimeCapabilities* capabilities);
    EfiStatus (*setTime)(EfiTime* time);
    void* getWakeupTime;
    void* setWakeupTime;
    
    EfiStatus (*setVirtualAddressMap)(usize memoryMapSize, usize descriptorSize, u32 descriptorVersion, EfiMemoryDescriptor* virtualMap);
    void* convertPointer;
    
    void* getVariable;
    void* getNextVariableName;
    void* setVariable;
    
    void* getNextHighMonotonicCount;
    void* resetSystem;
    
    void* updateCapsule;
    void* queryCapsuleCapabilities;
    void* queryVariableInfo;
};

struct EfiConfigurationTable {
    EfiGuid vendorGuid;
    void* vendorTable;
};

struct EfiSystemTable {
    EfiTableHeader header;
    const Char16* firmwareVendor;
    u32 firmwareRevision;
    EfiHandle consoleInHandle;
    EfiSimpleTextInputProtocol* conIn;
    EfiHandle consoleOutHandle;
    EfiSimpleTextOutputProtocol* conOut;
    EfiHandle standardErrorHandle;
    EfiSimpleTextOutputProtocol* stdErr;
    EfiRuntimeServices* runtimeServices;
    EfiBootServices* bootServices;
    usize numberOfTableEntries;
    EfiConfigurationTable* configurationTable;
};

}
