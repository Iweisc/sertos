#include "../../include/drivers/pci.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::drivers {

PciDevice Pci::sDevices[PCI_MAX_DEVICES];
u32 Pci::sDeviceCount = 0;
bool Pci::sInitialized = false;

void Pci::initialize() {
    sDeviceCount = 0;
    for (u32 i = 0; i < PCI_MAX_DEVICES; i++) {
        sDevices[i].active = false;
    }
    scanBus(0);
    sInitialized = true;
}

bool Pci::isInitialized() {
    return sInitialized;
}

u32 Pci::configRead(u8 bus, u8 device, u8 func, u8 offset) {
    u32 address = 0x80000000u
        | (static_cast<u32>(bus) << 16)
        | (static_cast<u32>(device) << 11)
        | (static_cast<u32>(func) << 8)
        | (offset & 0xFC);
    cpu::outl(PCI_CONFIG_ADDRESS, address);
    return cpu::inl(PCI_CONFIG_DATA);
}

u16 Pci::configRead16(u8 bus, u8 device, u8 func, u8 offset) {
    u32 val = configRead(bus, device, func, offset & 0xFC);
    return static_cast<u16>((val >> ((offset & 2) * 8)) & 0xFFFF);
}

u8 Pci::configRead8(u8 bus, u8 device, u8 func, u8 offset) {
    u32 val = configRead(bus, device, func, offset & 0xFC);
    return static_cast<u8>((val >> ((offset & 3) * 8)) & 0xFF);
}

void Pci::configWrite(u8 bus, u8 device, u8 func, u8 offset, u32 value) {
    u32 address = 0x80000000u
        | (static_cast<u32>(bus) << 16)
        | (static_cast<u32>(device) << 11)
        | (static_cast<u32>(func) << 8)
        | (offset & 0xFC);
    cpu::outl(PCI_CONFIG_ADDRESS, address);
    cpu::outl(PCI_CONFIG_DATA, value);
}

PciDevice* Pci::findDevice(u16 vendorId, u16 deviceId) {
    for (u32 i = 0; i < sDeviceCount; i++) {
        if (sDevices[i].active &&
            sDevices[i].vendorId == vendorId &&
            sDevices[i].deviceId == deviceId) {
            return &sDevices[i];
        }
    }
    return nullptr;
}

PciDevice* Pci::getDevice(u32 index) {
    if (index < sDeviceCount) return &sDevices[index];
    return nullptr;
}

u32 Pci::deviceCount() {
    return sDeviceCount;
}

void Pci::enableBusMastering(u8 bus, u8 device, u8 func) {
    u32 command = configRead(bus, device, func, 0x04);
    command |= (1 << 2);  // Bus Master Enable
    command |= (1 << 0);  // I/O Space Enable
    command |= (1 << 1);  // Memory Space Enable
    configWrite(bus, device, func, 0x04, command);
}

void Pci::scanBus(u8 bus) {
    for (u8 device = 0; device < 32; device++) {
        scanDevice(bus, device);
    }
}

void Pci::scanDevice(u8 bus, u8 device) {
    u16 vendorId = configRead16(bus, device, 0, 0x00);
    if (vendorId == PCI_VENDOR_INVALID) return;

    addDevice(bus, device, 0);

    u8 headerType = configRead8(bus, device, 0, 0x0E);
    if (headerType & 0x80) {
        // Multi-function device
        for (u8 func = 1; func < 8; func++) {
            vendorId = configRead16(bus, device, func, 0x00);
            if (vendorId != PCI_VENDOR_INVALID) {
                addDevice(bus, device, func);
            }
        }
    }
}

void Pci::addDevice(u8 bus, u8 device, u8 func) {
    if (sDeviceCount >= PCI_MAX_DEVICES) return;

    PciDevice& dev = sDevices[sDeviceCount];
    dev.bus = bus;
    dev.device = device;
    dev.function = func;

    u32 reg0 = configRead(bus, device, func, 0x00);
    dev.vendorId = reg0 & 0xFFFF;
    dev.deviceId = (reg0 >> 16) & 0xFFFF;

    u32 reg2 = configRead(bus, device, func, 0x08);
    dev.classCode = (reg2 >> 24) & 0xFF;
    dev.subclass = (reg2 >> 16) & 0xFF;
    dev.progIf = (reg2 >> 8) & 0xFF;

    dev.headerType = configRead8(bus, device, func, 0x0E) & 0x7F;

    u32 reg0B = configRead(bus, device, func, 0x2C);
    dev.subsystemVendorId = reg0B & 0xFFFF;
    dev.subsystemDeviceId = (reg0B >> 16) & 0xFFFF;

    for (u32 i = 0; i < 6; i++) {
        dev.bar[i] = configRead(bus, device, func, 0x10 + i * 4);
    }

    dev.irqLine = configRead8(bus, device, func, 0x3C);
    dev.active = true;

    sDeviceCount++;
}

}
