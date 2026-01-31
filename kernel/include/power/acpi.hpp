#pragma once

#include "../types.hpp"

namespace sertos::power {

constexpr u32 MAX_ACPI_TABLES = 32;

enum class PowerState : u8 {
    S0 = 0,
    S1,
    S2,
    S3,
    S4,
    S5
};

enum class DevicePowerState : u8 {
    D0 = 0,
    D1,
    D2,
    D3hot,
    D3cold
};

struct RsdpDescriptor {
    char signature[8];
    u8 checksum;
    char oemId[6];
    u8 revision;
    u32 rsdtAddress;
} __attribute__((packed));

struct RsdpDescriptor20 {
    RsdpDescriptor firstPart;
    u32 length;
    u64 xsdtAddress;
    u8 extendedChecksum;
    u8 reserved[3];
} __attribute__((packed));

struct AcpiSdtHeader {
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oemId[6];
    char oemTableId[8];
    u32 oemRevision;
    u32 creatorId;
    u32 creatorRevision;
} __attribute__((packed));

struct Rsdt {
    AcpiSdtHeader header;
    u32 tablePointers[];
} __attribute__((packed));

struct Xsdt {
    AcpiSdtHeader header;
    u64 tablePointers[];
} __attribute__((packed));

struct Fadt {
    AcpiSdtHeader header;
    u32 firmwareCtrl;
    u32 dsdt;
    u8 reserved;
    u8 preferredPmProfile;
    u16 sciInterrupt;
    u32 smiCommandPort;
    u8 acpiEnable;
    u8 acpiDisable;
    u8 s4BiosReq;
    u8 pstateControl;
    u32 pm1aEventBlock;
    u32 pm1bEventBlock;
    u32 pm1aControlBlock;
    u32 pm1bControlBlock;
    u32 pm2ControlBlock;
    u32 pmTimerBlock;
    u32 gpe0Block;
    u32 gpe1Block;
    u8 pm1EventLength;
    u8 pm1ControlLength;
    u8 pm2ControlLength;
    u8 pmTimerLength;
    u8 gpe0Length;
    u8 gpe1Length;
    u8 gpe1Base;
    u8 cstateControl;
    u16 worstC2Latency;
    u16 worstC3Latency;
    u16 flushSize;
    u16 flushStride;
    u8 dutyOffset;
    u8 dutyWidth;
    u8 dayAlarm;
    u8 monthAlarm;
    u8 century;
    u16 bootArchitectureFlags;
    u8 reserved2;
    u32 flags;
    u8 resetReg[12];
    u8 resetValue;
    u16 armBootArchitectureFlags;
    u8 fadtMinorVersion;
    u64 xFirmwareControl;
    u64 xDsdt;
    u8 xPm1aEventBlock[12];
    u8 xPm1bEventBlock[12];
    u8 xPm1aControlBlock[12];
    u8 xPm1bControlBlock[12];
    u8 xPm2ControlBlock[12];
    u8 xPmTimerBlock[12];
    u8 xGpe0Block[12];
    u8 xGpe1Block[12];
    u8 sleepControlReg[12];
    u8 sleepStatusReg[12];
    u64 hypervisorVendorId;
} __attribute__((packed));

struct Madt {
    AcpiSdtHeader header;
    u32 lapicAddress;
    u32 flags;
} __attribute__((packed));

struct GenericAddress {
    u8 addressSpace;
    u8 bitWidth;
    u8 bitOffset;
    u8 accessSize;
    u64 address;
} __attribute__((packed));

struct AcpiTable {
    char signature[5];
    u64 address;
    u32 length;
    bool valid;
};

class ACPI {
public:
    static void initialize();
    
    static bool enable();
    static bool disable();
    static bool isEnabled();
    
    static bool shutdown();
    static bool reboot();
    static bool suspend(PowerState state);
    static bool resume();
    
    static PowerState currentPowerState();
    static bool setPowerState(PowerState state);
    
    static bool setDevicePowerState(u32 deviceId, DevicePowerState state);
    static DevicePowerState getDevicePowerState(u32 deviceId);
    
    static AcpiSdtHeader* findTable(const char* signature);
    static Fadt* getFadt();
    static Madt* getMadt();
    
    static u32 getPmTimer();
    static void pmTimerSleep(u32 microseconds);
    
    static bool isInitialized();

private:
    static bool findRsdp();
    static bool parseRsdt();
    static bool parseXsdt();
    static bool validateChecksum(void* table, usize length);
    
    static bool enterSleepState(PowerState state);
    static void prepareForSleep(PowerState state);
    static void wakeFromSleep();
    
    static RsdpDescriptor* sRsdp;
    static RsdpDescriptor20* sRsdp20;
    static Rsdt* sRsdt;
    static Xsdt* sXsdt;
    static Fadt* sFadt;
    static Madt* sMadt;
    static AcpiTable sTables[MAX_ACPI_TABLES];
    static u32 sTableCount;
    static PowerState sCurrentState;
    static bool sAcpiEnabled;
    static bool sInitialized;
    static bool sUseXsdt;
};

class PowerManager {
public:
    static void initialize();
    
    static bool shutdown();
    static bool reboot();
    static bool sleep();
    static bool hibernate();
    static bool suspend();
    
    static void setIdleCallback(void (*callback)());
    static void idle();
    
    static u32 getBatteryLevel();
    static bool isOnBattery();
    static bool isCharging();
    
    static void setCpuFrequency(u32 frequency);
    static u32 getCpuFrequency();
    static u32 getMaxCpuFrequency();
    static u32 getMinCpuFrequency();
    
    static void enablePowerSaving();
    static void disablePowerSaving();
    static bool isPowerSavingEnabled();
    
    static bool isInitialized();

private:
    static void (*sIdleCallback)();
    static u32 sBatteryLevel;
    static bool sOnBattery;
    static bool sCharging;
    static bool sPowerSaving;
    static bool sInitialized;
};

}
