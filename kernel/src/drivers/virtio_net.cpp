#include "../../include/drivers/virtio_net.hpp"
#include "../../include/drivers/pci.hpp"
#include "../../include/cpu/io.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/net/net.hpp"

namespace sertos::drivers {

VirtioNet::VirtioNetDevice VirtioNet::sDevices[MAX_DEVICES];
u32 VirtioNet::sDeviceCount = 0;
bool VirtioNet::sInitialized = false;

// Legacy VirtIO I/O register offsets
constexpr u32 VIRTIO_IO_DEVICE_FEATURES  = 0x00;
constexpr u32 VIRTIO_IO_DRIVER_FEATURES  = 0x04;
constexpr u32 VIRTIO_IO_QUEUE_ADDRESS    = 0x08;
constexpr u32 VIRTIO_IO_QUEUE_SIZE       = 0x0C;
constexpr u32 VIRTIO_IO_QUEUE_SELECT     = 0x0E;
constexpr u32 VIRTIO_IO_QUEUE_NOTIFY     = 0x10;
constexpr u32 VIRTIO_IO_DEVICE_STATUS    = 0x12;
constexpr u32 VIRTIO_IO_ISR_STATUS       = 0x13;
constexpr u32 VIRTIO_IO_MAC_BASE         = 0x14;

static void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) d[i] = value;
}

static void memcpy(void* dest, const void* src, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    const u8* s = reinterpret_cast<const u8*>(src);
    for (usize i = 0; i < size; i++) d[i] = s[i];
}

void VirtioNet::initialize() {
    for (u32 i = 0; i < MAX_DEVICES; i++) {
        sDevices[i].active = false;
    }
    sDeviceCount = 0;
    sInitialized = true;
}

bool VirtioNet::isInitialized() {
    return sInitialized;
}

u32 VirtioNet::readRegister(u32 deviceIndex, u32 offset) {
    auto& dev = sDevices[deviceIndex];
    return cpu::inl(static_cast<u16>(dev.ioBase + offset));
}

void VirtioNet::writeRegister(u32 deviceIndex, u32 offset, u32 value) {
    auto& dev = sDevices[deviceIndex];
    cpu::outl(static_cast<u16>(dev.ioBase + offset), value);
}

u8 VirtioNet::readConfig8(u32 deviceIndex, u32 offset) {
    auto& dev = sDevices[deviceIndex];
    return cpu::inb(static_cast<u16>(dev.ioBase + offset));
}

u16 VirtioNet::readConfig16(u32 deviceIndex, u32 offset) {
    auto& dev = sDevices[deviceIndex];
    return cpu::inw(static_cast<u16>(dev.ioBase + offset));
}

bool VirtioNet::probe(u32 bus, u32 device, u32 function) {
    if (sDeviceCount >= MAX_DEVICES) return false;

    u32 bar0 = Pci::configRead(static_cast<u8>(bus), static_cast<u8>(device),
                                static_cast<u8>(function), 0x10);

    // Legacy VirtIO uses I/O BAR (bit 0 = 1)
    if (!(bar0 & 1)) return false;

    auto& dev = sDevices[sDeviceCount];
    memset(&dev, 0, sizeof(VirtioNetDevice));
    dev.ioBase = bar0 & ~0x3u;
    dev.useMmio = false;
    dev.irq = Pci::configRead8(static_cast<u8>(bus), static_cast<u8>(device),
                                static_cast<u8>(function), 0x3C);

    // Enable bus mastering for DMA
    Pci::enableBusMastering(static_cast<u8>(bus), static_cast<u8>(device),
                            static_cast<u8>(function));

    sDeviceCount++;
    return true;
}

bool VirtioNet::init(u32 deviceIndex) {
    if (deviceIndex >= sDeviceCount) return false;

    auto& dev = sDevices[deviceIndex];
    u16 ioBase = static_cast<u16>(dev.ioBase);

    // 1. Reset
    cpu::outb(ioBase + VIRTIO_IO_DEVICE_STATUS, 0);
    // Small delay
    for (volatile int i = 0; i < 10000; i++);

    // 2. Acknowledge
    cpu::outb(ioBase + VIRTIO_IO_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);

    // 3. Driver
    cpu::outb(ioBase + VIRTIO_IO_DEVICE_STATUS,
              VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // 4. Read device features
    u32 deviceFeatures = cpu::inl(ioBase + VIRTIO_IO_DEVICE_FEATURES);
    dev.features = deviceFeatures;

    // Negotiate features: MAC + mergeable RX buffers (for 12-byte header)
    u32 driverFeatures = deviceFeatures & (VIRTIO_NET_F_MAC | VIRTIO_NET_F_MRG_RXBUF);
    cpu::outl(ioBase + VIRTIO_IO_DRIVER_FEATURES, driverFeatures);

    // 5. Features OK
    cpu::outb(ioBase + VIRTIO_IO_DEVICE_STATUS,
              VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

    // Check features OK
    u8 status = cpu::inb(ioBase + VIRTIO_IO_DEVICE_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        cpu::outb(ioBase + VIRTIO_IO_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return false;
    }

    // 6. Read MAC address
    if (deviceFeatures & VIRTIO_NET_F_MAC) {
        for (u32 i = 0; i < 6; i++) {
            dev.mac.bytes[i] = cpu::inb(ioBase + VIRTIO_IO_MAC_BASE + i);
        }
    }

    // 7. Setup virtqueues
    if (!setupQueues(deviceIndex)) {
        cpu::outb(ioBase + VIRTIO_IO_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return false;
    }

    // 8. Driver OK
    cpu::outb(ioBase + VIRTIO_IO_DEVICE_STATUS,
              VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
              VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    // 9. Add RX buffers
    addRxBuffers(deviceIndex);

    // 10. Register network interface
    auto* iface = net::NetworkStack::registerInterface("eth0", dev.mac);
    if (iface) {
        dev.interfaceId = iface->id;
    }

    dev.active = true;

    // Clear ISR
    cpu::inb(ioBase + VIRTIO_IO_ISR_STATUS);

    return true;
}

bool VirtioNet::setupQueues(u32 deviceIndex) {
    auto& dev = sDevices[deviceIndex];
    u16 ioBase = static_cast<u16>(dev.ioBase);

    // Setup RX queue (index 0)
    cpu::outw(ioBase + VIRTIO_IO_QUEUE_SELECT, VIRTIO_NET_RX_QUEUE);
    u16 rxSize = cpu::inw(ioBase + VIRTIO_IO_QUEUE_SIZE);
    if (rxSize == 0) return false;

    if (!allocateQueue(&dev.rxQueue, rxSize)) return false;

    // Write queue physical address (page-aligned, divided by 4096)
    u64 rxPhysAddr = reinterpret_cast<u64>(dev.rxQueue.desc);
    cpu::outl(ioBase + VIRTIO_IO_QUEUE_ADDRESS, static_cast<u32>(rxPhysAddr / 4096));

    // Setup TX queue (index 1)
    cpu::outw(ioBase + VIRTIO_IO_QUEUE_SELECT, VIRTIO_NET_TX_QUEUE);
    u16 txSize = cpu::inw(ioBase + VIRTIO_IO_QUEUE_SIZE);
    if (txSize == 0) return false;

    if (!allocateQueue(&dev.txQueue, txSize)) return false;

    u64 txPhysAddr = reinterpret_cast<u64>(dev.txQueue.desc);
    cpu::outl(ioBase + VIRTIO_IO_QUEUE_ADDRESS, static_cast<u32>(txPhysAddr / 4096));

    return true;
}

bool VirtioNet::allocateQueue(Virtqueue* vq, u32 size) {
    vq->size = size;
    vq->lastUsedIdx = 0;

    // Legacy layout: descriptors, then avail ring, then used ring (page-aligned)
    usize descSize = sizeof(VirtqDesc) * size;
    usize availSize = sizeof(u16) * (3 + size);
    usize usedSize = sizeof(u16) * 3 + sizeof(VirtqUsedElem) * size;

    usize availOffset = descSize;
    usize usedOffset = align_up(availOffset + availSize, static_cast<usize>(4096));
    usize totalSize = usedOffset + usedSize;
    usize pages = (totalSize + 4095) / 4096;

    void* mem = memory::PMM::allocatePages(pages);
    if (!mem) return false;

    memset(mem, 0, pages * 4096);

    u8* base = reinterpret_cast<u8*>(mem);
    vq->desc = reinterpret_cast<VirtqDesc*>(base);
    vq->avail = reinterpret_cast<VirtqAvail*>(base + availOffset);
    vq->used = reinterpret_cast<VirtqUsed*>(base + usedOffset);

    // Initialize free descriptor chain
    vq->freeHead = 0;
    vq->numFree = size;
    for (u32 i = 0; i < size - 1; i++) {
        vq->desc[i].next = i + 1;
    }
    vq->desc[size - 1].next = 0xFFFF;

    // Allocate buffer tracking arrays
    usize bufPages = (sizeof(void*) * size + sizeof(bool) * size + 4095) / 4096;
    void* bufMem = memory::PMM::allocatePages(bufPages);
    if (!bufMem) return false;
    memset(bufMem, 0, bufPages * 4096);

    vq->buffers = reinterpret_cast<void**>(bufMem);
    vq->bufferOwned = reinterpret_cast<bool*>(
        reinterpret_cast<u8*>(bufMem) + sizeof(void*) * size);

    return true;
}

void VirtioNet::freeQueue(Virtqueue* vq) {
    (void)vq;
}

i32 VirtioNet::allocateDescriptor(Virtqueue* vq) {
    if (vq->numFree == 0) return -1;

    u32 idx = vq->freeHead;
    vq->freeHead = vq->desc[idx].next;
    vq->numFree--;
    return static_cast<i32>(idx);
}

void VirtioNet::freeDescriptor(Virtqueue* vq, u32 idx) {
    vq->desc[idx].next = vq->freeHead;
    vq->desc[idx].flags = 0;
    vq->freeHead = idx;
    vq->numFree++;
}

void VirtioNet::addRxBuffers(u32 deviceIndex) {
    auto& dev = sDevices[deviceIndex];
    auto& rxq = dev.rxQueue;

    // Fill RX queue with buffers
    while (rxq.numFree > 0) {
        i32 descIdx = allocateDescriptor(&rxq);
        if (descIdx < 0) break;

        // Allocate a page for the RX buffer
        void* buf = memory::PMM::allocatePage();
        if (!buf) {
            freeDescriptor(&rxq, static_cast<u32>(descIdx));
            break;
        }

        rxq.buffers[descIdx] = buf;
        rxq.bufferOwned[descIdx] = true;

        // Setup descriptor: device writes to this buffer
        rxq.desc[descIdx].addr = reinterpret_cast<u64>(buf);
        rxq.desc[descIdx].len = VIRTIO_NET_RX_BUFFER_SIZE;
        rxq.desc[descIdx].flags = VIRTQ_DESC_F_WRITE;
        rxq.desc[descIdx].next = 0;

        // Add to available ring
        rxq.avail->ring[rxq.avail->idx % rxq.size] = static_cast<u16>(descIdx);
        // Memory barrier
        asm volatile("mfence" ::: "memory");
        rxq.avail->idx++;
    }

    // Notify device that RX buffers are available
    notifyQueue(deviceIndex, VIRTIO_NET_RX_QUEUE);
}

void VirtioNet::notifyQueue(u32 deviceIndex, u32 queueIndex) {
    auto& dev = sDevices[deviceIndex];
    cpu::outw(static_cast<u16>(dev.ioBase + VIRTIO_IO_QUEUE_NOTIFY),
              static_cast<u16>(queueIndex));
}

bool VirtioNet::sendPacket(u32 deviceIndex, const void* data, usize length) {
    if (deviceIndex >= sDeviceCount || !sDevices[deviceIndex].active) {
        cpu::serialPuts("[VIO] sendPacket: device not active\n");
        return false;
    }
    cpu::serialPuts("[VIO] TX len=");
    { u32 n = static_cast<u32>(length); char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
    cpu::serialPutc('\n');

    auto& dev = sDevices[deviceIndex];
    auto& txq = dev.txQueue;

    // Need a descriptor for the header + data
    i32 descIdx = allocateDescriptor(&txq);
    if (descIdx < 0) {
        // Try to reclaim used TX descriptors
        processUsedTx(deviceIndex);
        descIdx = allocateDescriptor(&txq);
        if (descIdx < 0) return false;
    }

    // Allocate buffer for VirtioNetHeader + packet data
    void* buf = txq.buffers[descIdx];
    if (!buf) {
        buf = memory::PMM::allocatePage();
        if (!buf) {
            freeDescriptor(&txq, static_cast<u32>(descIdx));
            return false;
        }
        txq.buffers[descIdx] = buf;
        txq.bufferOwned[descIdx] = true;
    }

    // Write VirtioNetHeader (all zeros) + packet data
    usize headerSize = sizeof(VirtioNetHeader);
    if (length + headerSize > 4096) {
        length = 4096 - headerSize;
    }

    memset(buf, 0, headerSize);
    memcpy(reinterpret_cast<u8*>(buf) + headerSize, data, length);

    txq.desc[descIdx].addr = reinterpret_cast<u64>(buf);
    txq.desc[descIdx].len = static_cast<u32>(headerSize + length);
    txq.desc[descIdx].flags = 0;  // Device reads this (no WRITE flag)
    txq.desc[descIdx].next = 0;

    // Add to available ring
    txq.avail->ring[txq.avail->idx % txq.size] = static_cast<u16>(descIdx);
    asm volatile("mfence" ::: "memory");
    txq.avail->idx++;

    // Notify device
    notifyQueue(deviceIndex, VIRTIO_NET_TX_QUEUE);

    return true;
}

void VirtioNet::receivePackets(u32 deviceIndex) {
    if (deviceIndex >= sDeviceCount || !sDevices[deviceIndex].active) return;

    auto& dev = sDevices[deviceIndex];
    auto& rxq = dev.rxQueue;

    // Clear ISR to acknowledge interrupts
    cpu::inb(static_cast<u16>(dev.ioBase + VIRTIO_IO_ISR_STATUS));

    // Process received packets
    processUsedRx(deviceIndex);

    // Also reclaim TX descriptors
    processUsedTx(deviceIndex);
}

void VirtioNet::processUsedRx(u32 deviceIndex) {
    auto& dev = sDevices[deviceIndex];
    auto& rxq = dev.rxQueue;

    while (rxq.lastUsedIdx != rxq.used->idx) {
        asm volatile("mfence" ::: "memory");

        u32 usedIdx = rxq.lastUsedIdx % rxq.size;
        u32 descIdx = rxq.used->ring[usedIdx].id;
        u32 len = rxq.used->ring[usedIdx].len;

        if (descIdx < rxq.size && rxq.buffers[descIdx] && len > sizeof(VirtioNetHeader)) {
            u8* packetData = reinterpret_cast<u8*>(rxq.buffers[descIdx]);
            usize headerSize = sizeof(VirtioNetHeader);
            usize packetLen = len - headerSize;

            cpu::serialPuts("[VIO] RX len=");
            { u32 n = static_cast<u32>(packetLen); char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
            cpu::serialPutc('\n');

            // Feed to network stack (skip VirtioNetHeader)
            net::NetworkStack::receivePacket(dev.interfaceId,
                                              packetData + headerSize, packetLen);
        }

        // Recycle the buffer - put it back in the available ring
        rxq.desc[descIdx].addr = reinterpret_cast<u64>(rxq.buffers[descIdx]);
        rxq.desc[descIdx].len = VIRTIO_NET_RX_BUFFER_SIZE;
        rxq.desc[descIdx].flags = VIRTQ_DESC_F_WRITE;

        rxq.avail->ring[rxq.avail->idx % rxq.size] = static_cast<u16>(descIdx);
        asm volatile("mfence" ::: "memory");
        rxq.avail->idx++;

        rxq.lastUsedIdx++;
    }

    // Notify device that we've replenished RX buffers
    notifyQueue(deviceIndex, VIRTIO_NET_RX_QUEUE);
}

void VirtioNet::processUsedTx(u32 deviceIndex) {
    auto& dev = sDevices[deviceIndex];
    auto& txq = dev.txQueue;

    while (txq.lastUsedIdx != txq.used->idx) {
        asm volatile("mfence" ::: "memory");

        u32 usedIdx = txq.lastUsedIdx % txq.size;
        u32 descIdx = txq.used->ring[usedIdx].id;

        if (descIdx < txq.size) {
            // Don't free the buffer - reuse it for next TX
            freeDescriptor(&txq, descIdx);
        }

        txq.lastUsedIdx++;
    }
}

void VirtioNet::handleInterrupt(u32 deviceIndex) {
    if (deviceIndex >= sDeviceCount) return;
    receivePackets(deviceIndex);
}

net::MacAddress VirtioNet::getMacAddress(u32 deviceIndex) {
    if (deviceIndex < sDeviceCount) {
        return sDevices[deviceIndex].mac;
    }
    return net::MacAddress{{0, 0, 0, 0, 0, 0}};
}

bool VirtioNet::isLinkUp(u32 deviceIndex) {
    if (deviceIndex >= sDeviceCount || !sDevices[deviceIndex].active) return false;
    return true;
}

}
