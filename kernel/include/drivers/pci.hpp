#pragma once

#include "../types.hpp"

namespace sertos::drivers {

constexpr u16 PCI_CONFIG_ADDRESS = 0xCF8;
constexpr u16 PCI_CONFIG_DATA    = 0xCFC;
constexpr u32 PCI_MAX_DEVICES    = 32;

constexpr u16 PCI_VENDOR_INVALID = 0xFFFF;

struct PciDevice {
    u8  bus, device, function;
    u16 vendorId, deviceId;
    u16 subsystemVendorId, subsystemDeviceId;
    u8  classCode, subclass, progIf;
    u8  headerType;
    u8  irqLine;
    u32 bar[6];
    bool active;
};

class Pci {
public:
    static void initialize();
    static bool isInitialized();

    static u32  configRead(u8 bus, u8 device, u8 func, u8 offset);
    static u16  configRead16(u8 bus, u8 device, u8 func, u8 offset);
    static u8   configRead8(u8 bus, u8 device, u8 func, u8 offset);
    static void configWrite(u8 bus, u8 device, u8 func, u8 offset, u32 value);

    static PciDevice* findDevice(u16 vendorId, u16 deviceId);
    static PciDevice* getDevice(u32 index);
    static u32 deviceCount();

    static void enableBusMastering(u8 bus, u8 device, u8 func);

private:
    static void scanBus(u8 bus);
    static void scanDevice(u8 bus, u8 device);
    static void addDevice(u8 bus, u8 device, u8 func);

    static PciDevice sDevices[PCI_MAX_DEVICES];
    static u32 sDeviceCount;
    static bool sInitialized;
};

}
