#pragma once

#include "../types.hpp"

namespace sertos::drivers {

constexpr u32 MAX_USB_DEVICES = 128;
constexpr u32 MAX_USB_ENDPOINTS = 32;
constexpr u32 MAX_USB_CONTROLLERS = 8;
constexpr u32 USB_MAX_PACKET_SIZE = 1024;

enum class UsbSpeed : u8 {
    Low = 0,
    Full = 1,
    High = 2,
    Super = 3,
    SuperPlus = 4
};

enum class UsbTransferType : u8 {
    Control = 0,
    Isochronous = 1,
    Bulk = 2,
    Interrupt = 3
};

enum class UsbDirection : u8 {
    Out = 0,
    In = 1
};

enum class UsbDeviceState : u8 {
    Detached = 0,
    Attached,
    Powered,
    Default,
    Address,
    Configured,
    Suspended
};

enum class UsbControllerType : u8 {
    Unknown = 0,
    UHCI,
    OHCI,
    EHCI,
    XHCI
};

struct UsbDeviceDescriptor {
    u8 length;
    u8 descriptorType;
    u16 usbVersion;
    u8 deviceClass;
    u8 deviceSubClass;
    u8 deviceProtocol;
    u8 maxPacketSize0;
    u16 vendorId;
    u16 productId;
    u16 deviceVersion;
    u8 manufacturerIndex;
    u8 productIndex;
    u8 serialNumberIndex;
    u8 numConfigurations;
} __attribute__((packed));

struct UsbConfigDescriptor {
    u8 length;
    u8 descriptorType;
    u16 totalLength;
    u8 numInterfaces;
    u8 configurationValue;
    u8 configurationIndex;
    u8 attributes;
    u8 maxPower;
} __attribute__((packed));

struct UsbInterfaceDescriptor {
    u8 length;
    u8 descriptorType;
    u8 interfaceNumber;
    u8 alternateSetting;
    u8 numEndpoints;
    u8 interfaceClass;
    u8 interfaceSubClass;
    u8 interfaceProtocol;
    u8 interfaceIndex;
} __attribute__((packed));

struct UsbEndpointDescriptor {
    u8 length;
    u8 descriptorType;
    u8 endpointAddress;
    u8 attributes;
    u16 maxPacketSize;
    u8 interval;
} __attribute__((packed));

struct UsbSetupPacket {
    u8 requestType;
    u8 request;
    u16 value;
    u16 index;
    u16 length;
} __attribute__((packed));

struct UsbEndpoint {
    u8 address;
    UsbTransferType type;
    UsbDirection direction;
    u16 maxPacketSize;
    u8 interval;
    bool active;
};

struct UsbDevice {
    u32 id;
    u8 address;
    u8 port;
    u8 controllerId;
    UsbSpeed speed;
    UsbDeviceState state;
    UsbDeviceDescriptor descriptor;
    UsbEndpoint endpoints[MAX_USB_ENDPOINTS];
    u8 endpointCount;
    u8 configurationValue;
    void* driverData;
    bool active;
};

struct UsbController {
    u32 id;
    UsbControllerType type;
    u64 baseAddress;
    u64 mmioBase;
    u8 irq;
    bool active;
    void* driverData;
};

struct UsbTransfer {
    UsbDevice* device;
    UsbEndpoint* endpoint;
    void* buffer;
    usize length;
    usize transferred;
    bool complete;
    i32 status;
};

using UsbDeviceCallback = void (*)(UsbDevice* device);
using UsbTransferCallback = void (*)(UsbTransfer* transfer);

class UsbDriver {
public:
    static void initialize();
    
    static bool registerController(UsbControllerType type, u64 baseAddr, u8 irq);
    static void unregisterController(u32 controllerId);
    
    static bool enumerateDevices();
    static UsbDevice* getDevice(u32 deviceId);
    static UsbDevice* findDevice(u16 vendorId, u16 productId);
    static u32 deviceCount();
    
    static bool controlTransfer(UsbDevice* device, UsbSetupPacket* setup, void* data, usize length);
    static bool bulkTransfer(UsbDevice* device, u8 endpoint, void* data, usize length, bool in);
    static bool interruptTransfer(UsbDevice* device, u8 endpoint, void* data, usize length, bool in);
    
    static bool setConfiguration(UsbDevice* device, u8 configuration);
    static bool setInterface(UsbDevice* device, u8 interface, u8 alternateSetting);
    static bool clearHalt(UsbDevice* device, u8 endpoint);
    static bool resetDevice(UsbDevice* device);
    
    static void registerDeviceCallback(UsbDeviceCallback onConnect, UsbDeviceCallback onDisconnect);
    
    static bool isInitialized();

private:
    static bool initXhci(UsbController* controller);
    static bool initEhci(UsbController* controller);
    static bool initOhci(UsbController* controller);
    static bool initUhci(UsbController* controller);
    
    static bool getDeviceDescriptor(UsbDevice* device);
    static bool setDeviceAddress(UsbDevice* device, u8 address);
    static bool getConfigDescriptor(UsbDevice* device, u8 index, void* buffer, usize length);
    
    static u8 allocateAddress();
    static void freeAddress(u8 address);
    
    static UsbController sControllers[MAX_USB_CONTROLLERS];
    static UsbDevice sDevices[MAX_USB_DEVICES];
    static u32 sControllerCount;
    static u32 sDeviceCount;
    static u8 sNextAddress;
    static u64 sAddressBitmap[2];
    static UsbDeviceCallback sOnConnect;
    static UsbDeviceCallback sOnDisconnect;
    static bool sInitialized;
};

}
