#pragma once

#include "../types.hpp"
#include "../net/net.hpp"

namespace sertos::drivers {

constexpr u32 VIRTIO_NET_VENDOR_ID = 0x1AF4;
constexpr u32 VIRTIO_NET_DEVICE_ID = 0x1000;
constexpr u32 VIRTIO_NET_SUBSYSTEM_ID = 0x0001;

constexpr u64 VIRTIO_F_VERSION_1 = 1ULL << 32;
constexpr u32 VIRTIO_NET_F_MAC = 1 << 5;
constexpr u32 VIRTIO_NET_F_STATUS = 1 << 16;
constexpr u32 VIRTIO_NET_F_MRG_RXBUF = 1 << 15;
constexpr u32 VIRTIO_NET_F_CSUM = 1 << 0;
constexpr u32 VIRTIO_NET_F_GUEST_CSUM = 1 << 1;

constexpr u32 VIRTIO_STATUS_ACKNOWLEDGE = 1;
constexpr u32 VIRTIO_STATUS_DRIVER = 2;
constexpr u32 VIRTIO_STATUS_DRIVER_OK = 4;
constexpr u32 VIRTIO_STATUS_FEATURES_OK = 8;
constexpr u32 VIRTIO_STATUS_FAILED = 128;

constexpr u32 VIRTQ_DESC_F_NEXT = 1;
constexpr u32 VIRTQ_DESC_F_WRITE = 2;
constexpr u32 VIRTQ_DESC_F_INDIRECT = 4;

constexpr u32 VIRTQ_AVAIL_F_NO_INTERRUPT = 1;
constexpr u32 VIRTQ_USED_F_NO_NOTIFY = 1;

constexpr u32 VIRTIO_NET_RX_QUEUE = 0;
constexpr u32 VIRTIO_NET_TX_QUEUE = 1;
constexpr u32 VIRTIO_NET_CTRL_QUEUE = 2;

constexpr u32 VIRTIO_NET_QUEUE_SIZE = 256;
constexpr u32 VIRTIO_NET_RX_BUFFER_SIZE = 2048;

constexpr u32 VIRTIO_PCI_CAP_COMMON_CFG = 1;
constexpr u32 VIRTIO_PCI_CAP_NOTIFY_CFG = 2;
constexpr u32 VIRTIO_PCI_CAP_ISR_CFG = 3;
constexpr u32 VIRTIO_PCI_CAP_DEVICE_CFG = 4;

struct VirtqDesc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed));

struct VirtqAvail {
    u16 flags;
    u16 idx;
    u16 ring[VIRTIO_NET_QUEUE_SIZE];
    u16 usedEvent;
} __attribute__((packed));

struct VirtqUsedElem {
    u32 id;
    u32 len;
} __attribute__((packed));

struct VirtqUsed {
    u16 flags;
    u16 idx;
    VirtqUsedElem ring[VIRTIO_NET_QUEUE_SIZE];
    u16 availEvent;
} __attribute__((packed));

struct Virtqueue {
    u32 size;
    u32 freeHead;
    u32 numFree;
    u16 lastUsedIdx;
    
    VirtqDesc* desc;
    VirtqAvail* avail;
    VirtqUsed* used;
    
    void** buffers;
    bool* bufferOwned;
};

struct VirtioNetHeader {
    u8 flags;
    u8 gsoType;
    u16 hdrLen;
    u16 gsoSize;
    u16 csumStart;
    u16 csumOffset;
    u16 numBuffers;
} __attribute__((packed));

struct VirtioNetConfig {
    net::MacAddress mac;
    u16 status;
    u16 maxVirtqueuePairs;
    u16 mtu;
} __attribute__((packed));

struct VirtioPciCommonCfg {
    u32 deviceFeatureSelect;
    u32 deviceFeature;
    u32 driverFeatureSelect;
    u32 driverFeature;
    u16 msixConfig;
    u16 numQueues;
    u8 deviceStatus;
    u8 configGeneration;
    u16 queueSelect;
    u16 queueSize;
    u16 queueMsixVector;
    u16 queueEnable;
    u16 queueNotifyOff;
    u64 queueDesc;
    u64 queueDriver;
    u64 queueDevice;
} __attribute__((packed));

class VirtioNet {
public:
    static void initialize();
    
    static bool probe(u32 bus, u32 device, u32 function);
    static bool init(u32 deviceIndex);
    
    static bool sendPacket(u32 deviceIndex, const void* data, usize length);
    static void receivePackets(u32 deviceIndex);
    
    static void handleInterrupt(u32 deviceIndex);
    
    static net::MacAddress getMacAddress(u32 deviceIndex);
    static bool isLinkUp(u32 deviceIndex);
    
    static bool isInitialized();

private:
    static bool setupQueues(u32 deviceIndex);
    static bool allocateQueue(Virtqueue* vq, u32 size);
    static void freeQueue(Virtqueue* vq);
    
    static void addRxBuffers(u32 deviceIndex);
    static i32 allocateDescriptor(Virtqueue* vq);
    static void freeDescriptor(Virtqueue* vq, u32 idx);
    
    static void notifyQueue(u32 deviceIndex, u32 queueIndex);
    static void processUsedRx(u32 deviceIndex);
    static void processUsedTx(u32 deviceIndex);
    
    static u32 readRegister(u32 deviceIndex, u32 offset);
    static void writeRegister(u32 deviceIndex, u32 offset, u32 value);
    static u8 readConfig8(u32 deviceIndex, u32 offset);
    static u16 readConfig16(u32 deviceIndex, u32 offset);
    
    struct VirtioNetDevice {
        u64 ioBase;
        u64 mmioBase;
        u64 commonCfgBase;
        u64 notifyBase;
        u64 isrBase;
        u64 deviceCfgBase;
        u32 notifyOffMultiplier;
        
        Virtqueue rxQueue;
        Virtqueue txQueue;
        
        net::MacAddress mac;
        u32 features;
        u8 irq;
        
        u32 interfaceId;
        bool active;
        bool useMmio;
    };
    
    static constexpr u32 MAX_DEVICES = 4;
    static VirtioNetDevice sDevices[MAX_DEVICES];
    static u32 sDeviceCount;
    static bool sInitialized;
};

}
