#include "../../include/drivers/usb.hpp"
#include "../../include/cpu/io.hpp"
#include "../../include/memory/pmm.hpp"

namespace sertos::drivers {

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

void ioWait() {
    cpu::outb(0x80, 0);
}

}

UsbController UsbDriver::sControllers[MAX_USB_CONTROLLERS];
UsbDevice UsbDriver::sDevices[MAX_USB_DEVICES];
u32 UsbDriver::sControllerCount = 0;
u32 UsbDriver::sDeviceCount = 0;
u8 UsbDriver::sNextAddress = 1;
u64 UsbDriver::sAddressBitmap[2] = {1, 0};
UsbDeviceCallback UsbDriver::sOnConnect = nullptr;
UsbDeviceCallback UsbDriver::sOnDisconnect = nullptr;
bool UsbDriver::sInitialized = false;

void UsbDriver::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_USB_CONTROLLERS; i++) {
        memset(&sControllers[i], 0, sizeof(UsbController));
        sControllers[i].active = false;
    }
    
    for (u32 i = 0; i < MAX_USB_DEVICES; i++) {
        memset(&sDevices[i], 0, sizeof(UsbDevice));
        sDevices[i].active = false;
    }
    
    sControllerCount = 0;
    sDeviceCount = 0;
    sNextAddress = 1;
    sAddressBitmap[0] = 1;
    sAddressBitmap[1] = 0;
    
    sInitialized = true;
}

bool UsbDriver::registerController(UsbControllerType type, u64 baseAddr, u8 irq) {
    if (!sInitialized) return false;
    if (sControllerCount >= MAX_USB_CONTROLLERS) return false;
    
    UsbController* controller = nullptr;
    for (u32 i = 0; i < MAX_USB_CONTROLLERS; i++) {
        if (!sControllers[i].active) {
            controller = &sControllers[i];
            controller->id = i;
            break;
        }
    }
    
    if (!controller) return false;
    
    controller->type = type;
    controller->baseAddress = baseAddr;
    controller->irq = irq;
    controller->active = true;
    controller->driverData = nullptr;
    
    bool success = false;
    switch (type) {
        case UsbControllerType::XHCI:
            success = initXhci(controller);
            break;
        case UsbControllerType::EHCI:
            success = initEhci(controller);
            break;
        case UsbControllerType::OHCI:
            success = initOhci(controller);
            break;
        case UsbControllerType::UHCI:
            success = initUhci(controller);
            break;
        default:
            break;
    }
    
    if (success) {
        sControllerCount++;
    } else {
        controller->active = false;
    }
    
    return success;
}

void UsbDriver::unregisterController(u32 controllerId) {
    if (!sInitialized) return;
    if (controllerId >= MAX_USB_CONTROLLERS) return;
    
    UsbController* controller = &sControllers[controllerId];
    if (!controller->active) return;
    
    for (u32 i = 0; i < MAX_USB_DEVICES; i++) {
        if (sDevices[i].active && sDevices[i].controllerId == controllerId) {
            if (sOnDisconnect) {
                sOnDisconnect(&sDevices[i]);
            }
            freeAddress(sDevices[i].address);
            sDevices[i].active = false;
            sDeviceCount--;
        }
    }
    
    controller->active = false;
    sControllerCount--;
}

bool UsbDriver::enumerateDevices() {
    if (!sInitialized) return false;
    
    for (u32 i = 0; i < MAX_USB_CONTROLLERS; i++) {
        if (!sControllers[i].active) continue;
        
    }
    
    return true;
}

UsbDevice* UsbDriver::getDevice(u32 deviceId) {
    if (!sInitialized) return nullptr;
    if (deviceId >= MAX_USB_DEVICES) return nullptr;
    
    if (sDevices[deviceId].active) {
        return &sDevices[deviceId];
    }
    
    return nullptr;
}

UsbDevice* UsbDriver::findDevice(u16 vendorId, u16 productId) {
    if (!sInitialized) return nullptr;
    
    for (u32 i = 0; i < MAX_USB_DEVICES; i++) {
        if (sDevices[i].active &&
            sDevices[i].descriptor.vendorId == vendorId &&
            sDevices[i].descriptor.productId == productId) {
            return &sDevices[i];
        }
    }
    
    return nullptr;
}

u32 UsbDriver::deviceCount() {
    return sDeviceCount;
}

bool UsbDriver::controlTransfer(UsbDevice* device, UsbSetupPacket* setup, void* data, usize length) {
    if (!sInitialized || !device || !setup) return false;
    if (!device->active) return false;
    
    UsbController* controller = &sControllers[device->controllerId];
    if (!controller->active) return false;
    
    switch (controller->type) {
        case UsbControllerType::XHCI:
            break;
        case UsbControllerType::EHCI:
            break;
        case UsbControllerType::OHCI:
            break;
        case UsbControllerType::UHCI:
            break;
        default:
            return false;
    }
    
    return true;
}

bool UsbDriver::bulkTransfer(UsbDevice* device, u8 endpoint, void* data, usize length, bool in) {
    if (!sInitialized || !device || !data) return false;
    if (!device->active) return false;
    
    UsbEndpoint* ep = nullptr;
    for (u8 i = 0; i < device->endpointCount; i++) {
        if ((device->endpoints[i].address & 0x0F) == (endpoint & 0x0F) &&
            device->endpoints[i].type == UsbTransferType::Bulk) {
            ep = &device->endpoints[i];
            break;
        }
    }
    
    if (!ep) return false;
    
    return true;
}

bool UsbDriver::interruptTransfer(UsbDevice* device, u8 endpoint, void* data, usize length, bool in) {
    if (!sInitialized || !device || !data) return false;
    if (!device->active) return false;
    
    UsbEndpoint* ep = nullptr;
    for (u8 i = 0; i < device->endpointCount; i++) {
        if ((device->endpoints[i].address & 0x0F) == (endpoint & 0x0F) &&
            device->endpoints[i].type == UsbTransferType::Interrupt) {
            ep = &device->endpoints[i];
            break;
        }
    }
    
    if (!ep) return false;
    
    return true;
}

bool UsbDriver::setConfiguration(UsbDevice* device, u8 configuration) {
    if (!sInitialized || !device) return false;
    
    UsbSetupPacket setup;
    setup.requestType = 0x00;
    setup.request = 0x09;
    setup.value = configuration;
    setup.index = 0;
    setup.length = 0;
    
    if (!controlTransfer(device, &setup, nullptr, 0)) {
        return false;
    }
    
    device->configurationValue = configuration;
    device->state = UsbDeviceState::Configured;
    
    return true;
}

bool UsbDriver::setInterface(UsbDevice* device, u8 interface, u8 alternateSetting) {
    if (!sInitialized || !device) return false;
    
    UsbSetupPacket setup;
    setup.requestType = 0x01;
    setup.request = 0x0B;
    setup.value = alternateSetting;
    setup.index = interface;
    setup.length = 0;
    
    return controlTransfer(device, &setup, nullptr, 0);
}

bool UsbDriver::clearHalt(UsbDevice* device, u8 endpoint) {
    if (!sInitialized || !device) return false;
    
    UsbSetupPacket setup;
    setup.requestType = 0x02;
    setup.request = 0x01;
    setup.value = 0;
    setup.index = endpoint;
    setup.length = 0;
    
    return controlTransfer(device, &setup, nullptr, 0);
}

bool UsbDriver::resetDevice(UsbDevice* device) {
    if (!sInitialized || !device) return false;
    
    device->state = UsbDeviceState::Default;
    
    return true;
}

void UsbDriver::registerDeviceCallback(UsbDeviceCallback onConnect, UsbDeviceCallback onDisconnect) {
    sOnConnect = onConnect;
    sOnDisconnect = onDisconnect;
}

bool UsbDriver::isInitialized() {
    return sInitialized;
}

bool UsbDriver::initXhci(UsbController* controller) {
    if (!controller) return false;
    
    controller->mmioBase = controller->baseAddress;
    
    return true;
}

bool UsbDriver::initEhci(UsbController* controller) {
    if (!controller) return false;
    
    controller->mmioBase = controller->baseAddress;
    
    return true;
}

bool UsbDriver::initOhci(UsbController* controller) {
    if (!controller) return false;
    
    controller->mmioBase = controller->baseAddress;
    
    return true;
}

bool UsbDriver::initUhci(UsbController* controller) {
    if (!controller) return false;
    
    return true;
}

bool UsbDriver::getDeviceDescriptor(UsbDevice* device) {
    if (!device) return false;
    
    UsbSetupPacket setup;
    setup.requestType = 0x80;
    setup.request = 0x06;
    setup.value = 0x0100;
    setup.index = 0;
    setup.length = sizeof(UsbDeviceDescriptor);
    
    return controlTransfer(device, &setup, &device->descriptor, sizeof(UsbDeviceDescriptor));
}

bool UsbDriver::setDeviceAddress(UsbDevice* device, u8 address) {
    if (!device) return false;
    
    UsbSetupPacket setup;
    setup.requestType = 0x00;
    setup.request = 0x05;
    setup.value = address;
    setup.index = 0;
    setup.length = 0;
    
    if (!controlTransfer(device, &setup, nullptr, 0)) {
        return false;
    }
    
    device->address = address;
    device->state = UsbDeviceState::Address;
    
    return true;
}

bool UsbDriver::getConfigDescriptor(UsbDevice* device, u8 index, void* buffer, usize length) {
    if (!device || !buffer) return false;
    
    UsbSetupPacket setup;
    setup.requestType = 0x80;
    setup.request = 0x06;
    setup.value = 0x0200 | index;
    setup.index = 0;
    setup.length = static_cast<u16>(length);
    
    return controlTransfer(device, &setup, buffer, length);
}

u8 UsbDriver::allocateAddress() {
    for (u8 i = 1; i < 128; i++) {
        u64 bit = 1ULL << (i % 64);
        u8 idx = i / 64;
        
        if (!(sAddressBitmap[idx] & bit)) {
            sAddressBitmap[idx] |= bit;
            return i;
        }
    }
    
    return 0;
}

void UsbDriver::freeAddress(u8 address) {
    if (address == 0 || address >= 128) return;
    
    u64 bit = 1ULL << (address % 64);
    u8 idx = address / 64;
    
    sAddressBitmap[idx] &= ~bit;
}

}
