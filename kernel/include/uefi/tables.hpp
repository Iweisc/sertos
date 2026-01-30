#pragma once

#include "types.hpp"

#define EFIAPI __attribute__((ms_abi))

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
    EfiStatus (EFIAPI *reset)(EfiSimpleTextOutputProtocol* self, bool extendedVerification);
    EfiStatus (EFIAPI *outputString)(EfiSimpleTextOutputProtocol* self, const Char16* string);
    EfiStatus (EFIAPI *testString)(EfiSimpleTextOutputProtocol* self, const Char16* string);
    EfiStatus (EFIAPI *queryMode)(EfiSimpleTextOutputProtocol* self, usize modeNumber, usize* columns, usize* rows);
    EfiStatus (EFIAPI *setMode)(EfiSimpleTextOutputProtocol* self, usize modeNumber);
    EfiStatus (EFIAPI *setAttribute)(EfiSimpleTextOutputProtocol* self, usize attribute);
    EfiStatus (EFIAPI *clearScreen)(EfiSimpleTextOutputProtocol* self);
    EfiStatus (EFIAPI *setCursorPosition)(EfiSimpleTextOutputProtocol* self, usize column, usize row);
    EfiStatus (EFIAPI *enableCursor)(EfiSimpleTextOutputProtocol* self, bool visible);
    void* mode;
};

struct EfiInputKey {
    u16 scanCode;
    Char16 unicodeChar;
};

struct EfiSimpleTextInputProtocol {
    EfiStatus (EFIAPI *reset)(EfiSimpleTextInputProtocol* self, bool extendedVerification);
    EfiStatus (EFIAPI *readKeyStroke)(EfiSimpleTextInputProtocol* self, EfiInputKey* key);
    EfiEvent waitForKey;
};

struct EfiBootServices {
    EfiTableHeader header;
    
    void* raiseTPL;
    void* restoreTPL;
    
    EfiStatus (EFIAPI *allocatePages)(EfiAllocateType type, EfiMemoryType memoryType, usize pages, EfiPhysicalAddress* memory);
    EfiStatus (EFIAPI *freePages)(EfiPhysicalAddress memory, usize pages);
    EfiStatus (EFIAPI *getMemoryMap)(usize* memoryMapSize, EfiMemoryDescriptor* memoryMap, usize* mapKey, usize* descriptorSize, u32* descriptorVersion);
    EfiStatus (EFIAPI *allocatePool)(EfiMemoryType poolType, usize size, void** buffer);
    EfiStatus (EFIAPI *freePool)(void* buffer);
    
    void* createEvent;
    void* setTimer;
    void* waitForEvent;
    void* signalEvent;
    void* closeEvent;
    void* checkEvent;
    
    void* installProtocolInterface;
    void* reinstallProtocolInterface;
    void* uninstallProtocolInterface;
    EfiStatus (EFIAPI *handleProtocol)(EfiHandle handle, const EfiGuid* protocol, void** interface);
    void* reserved;
    void* registerProtocolNotify;
    EfiStatus (EFIAPI *locateHandle)(u32 searchType, const EfiGuid* protocol, void* searchKey, usize* bufferSize, EfiHandle* buffer);
    void* locateDevicePath;
    void* installConfigurationTable;
    
    void* loadImage;
    void* startImage;
    void* exit;
    void* unloadImage;
    EfiStatus (EFIAPI *exitBootServices)(EfiHandle imageHandle, usize mapKey);
    
    void* getNextMonotonicCount;
    void* stall;
    EfiStatus (EFIAPI *setWatchdogTimer)(usize timeout, u64 watchdogCode, usize dataSize, const Char16* watchdogData);
    
    void* connectController;
    void* disconnectController;
    
    void* openProtocol;
    void* closeProtocol;
    void* openProtocolInformation;
    
    void* protocolsPerHandle;
    EfiStatus (EFIAPI *locateHandleBuffer)(u32 searchType, const EfiGuid* protocol, void* searchKey, usize* noHandles, EfiHandle** buffer);
    EfiStatus (EFIAPI *locateProtocol)(const EfiGuid* protocol, void* registration, void** interface);
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
    
    EfiStatus (EFIAPI *getTime)(EfiTime* time, EfiTimeCapabilities* capabilities);
    EfiStatus (EFIAPI *setTime)(EfiTime* time);
    void* getWakeupTime;
    void* setWakeupTime;
    
    EfiStatus (EFIAPI *setVirtualAddressMap)(usize memoryMapSize, usize descriptorSize, u32 descriptorVersion, EfiMemoryDescriptor* virtualMap);
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
