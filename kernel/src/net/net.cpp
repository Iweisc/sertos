#include "../../include/net/net.hpp"
#include "../../include/net/tcp.hpp"
#include "../../include/net/socket.hpp"
#include "../../include/drivers/virtio_net.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::net {

NetworkInterface NetworkStack::sInterfaces[MAX_NETWORK_INTERFACES];
u32 NetworkStack::sInterfaceCount = 0;
u32 NetworkStack::sDefaultInterface = 0;
bool NetworkStack::sInitialized = false;

static ArpEntry sArpCache[ARP_CACHE_SIZE];
static u32 sArpCacheCount = 0;

static void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

static void memcpy(void* dest, const void* src, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    const u8* s = reinterpret_cast<const u8*>(src);
    for (usize i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

static void strcpy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

u16 calculateChecksum(const void* data, usize length) {
    const u16* ptr = reinterpret_cast<const u16*>(data);
    u32 sum = 0;
    
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    
    if (length > 0) {
        sum += *reinterpret_cast<const u8*>(ptr);
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~static_cast<u16>(sum);
}

u16 calculateTcpChecksum(const IPv4Header* ip, const TcpHeader* tcp, const void* data, usize dataLen) {
    PseudoHeader pseudo;
    pseudo.srcAddr = ip->srcAddr;
    pseudo.dstAddr = ip->dstAddr;
    pseudo.zero = 0;
    pseudo.protocol = IP_PROTO_TCP;
    pseudo.length = htons(tcp->headerLength() + dataLen);
    
    u32 sum = 0;
    const u16* ptr = reinterpret_cast<const u16*>(&pseudo);
    for (usize i = 0; i < sizeof(PseudoHeader) / 2; i++) {
        sum += ptr[i];
    }
    
    ptr = reinterpret_cast<const u16*>(tcp);
    usize tcpLen = tcp->headerLength();
    while (tcpLen > 1) {
        sum += *ptr++;
        tcpLen -= 2;
    }
    
    ptr = reinterpret_cast<const u16*>(data);
    while (dataLen > 1) {
        sum += *ptr++;
        dataLen -= 2;
    }
    
    if (dataLen > 0) {
        sum += *reinterpret_cast<const u8*>(ptr);
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~static_cast<u16>(sum);
}

u16 calculateUdpChecksum(const IPv4Header* ip, const UdpHeader* udp, const void* data, usize dataLen) {
    PseudoHeader pseudo;
    pseudo.srcAddr = ip->srcAddr;
    pseudo.dstAddr = ip->dstAddr;
    pseudo.zero = 0;
    pseudo.protocol = IP_PROTO_UDP;
    pseudo.length = udp->length;
    
    u32 sum = 0;
    const u16* ptr = reinterpret_cast<const u16*>(&pseudo);
    for (usize i = 0; i < sizeof(PseudoHeader) / 2; i++) {
        sum += ptr[i];
    }
    
    ptr = reinterpret_cast<const u16*>(udp);
    for (usize i = 0; i < sizeof(UdpHeader) / 2; i++) {
        sum += ptr[i];
    }
    
    ptr = reinterpret_cast<const u16*>(data);
    while (dataLen > 1) {
        sum += *ptr++;
        dataLen -= 2;
    }
    
    if (dataLen > 0) {
        sum += *reinterpret_cast<const u8*>(ptr);
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~static_cast<u16>(sum);
}

void NetworkStack::initialize() {
    if (sInitialized) {
        return;
    }
    
    for (u32 i = 0; i < MAX_NETWORK_INTERFACES; i++) {
        sInterfaces[i].id = 0;
        sInterfaces[i].state = InterfaceState::Down;
        sInterfaces[i].active = false;
    }
    
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        sArpCache[i].valid = false;
        sArpCache[i].pending = false;
    }
    
    sInterfaceCount = 0;
    sDefaultInterface = 0;
    sArpCacheCount = 0;
    sInitialized = true;
    
    Tcp::initialize();
}

NetworkInterface* NetworkStack::registerInterface(const char* name, MacAddress mac) {
    if (!sInitialized || sInterfaceCount >= MAX_NETWORK_INTERFACES) {
        return nullptr;
    }
    
    for (u32 i = 0; i < MAX_NETWORK_INTERFACES; i++) {
        if (!sInterfaces[i].active) {
            NetworkInterface* iface = &sInterfaces[i];
            
            memset(iface, 0, sizeof(NetworkInterface));
            iface->id = i + 1;
            strcpy(iface->name, name, 16);
            iface->mac = mac;
            iface->state = InterfaceState::Down;
            iface->active = true;
            
            sInterfaceCount++;
            
            if (sDefaultInterface == 0) {
                sDefaultInterface = iface->id;
            }
            
            return iface;
        }
    }
    
    return nullptr;
}

void NetworkStack::unregisterInterface(u32 id) {
    NetworkInterface* iface = getInterface(id);
    if (!iface) {
        return;
    }
    
    iface->active = false;
    iface->state = InterfaceState::Down;
    sInterfaceCount--;
    
    if (sDefaultInterface == id) {
        sDefaultInterface = 0;
        for (u32 i = 0; i < MAX_NETWORK_INTERFACES; i++) {
            if (sInterfaces[i].active) {
                sDefaultInterface = sInterfaces[i].id;
                break;
            }
        }
    }
}

NetworkInterface* NetworkStack::getInterface(u32 id) {
    if (id == 0 || id > MAX_NETWORK_INTERFACES) {
        return nullptr;
    }
    
    NetworkInterface* iface = &sInterfaces[id - 1];
    if (iface->active && iface->id == id) {
        return iface;
    }
    
    return nullptr;
}

NetworkInterface* NetworkStack::getDefaultInterface() {
    return getInterface(sDefaultInterface);
}

bool NetworkStack::configureInterface(u32 id, IPv4Address ip, IPv4Address netmask, IPv4Address gateway) {
    NetworkInterface* iface = getInterface(id);
    if (!iface) {
        return false;
    }
    
    iface->ip = ip;
    iface->netmask = netmask;
    iface->gateway = gateway;
    
    return true;
}

bool NetworkStack::setInterfaceUp(u32 id) {
    NetworkInterface* iface = getInterface(id);
    if (!iface) {
        return false;
    }
    
    iface->state = InterfaceState::Up;
    return true;
}

bool NetworkStack::setInterfaceDown(u32 id) {
    NetworkInterface* iface = getInterface(id);
    if (!iface) {
        return false;
    }
    
    iface->state = InterfaceState::Down;
    return true;
}

void NetworkStack::receivePacket(u32 interfaceId, const void* data, usize length) {
    NetworkInterface* iface = getInterface(interfaceId);
    if (!iface || iface->state != InterfaceState::Up) {
        cpu::serialPuts("[NET] receivePacket: iface down/null\n");
        return;
    }
    cpu::serialPuts("[NET] RX pkt len=");
    // Print length as decimal
    {
        char buf[12]; u32 n = static_cast<u32>(length); int i = 0;
        if (n == 0) buf[i++] = '0';
        else { char tmp[12]; int t = 0; while (n > 0) { tmp[t++] = '0' + (n % 10); n /= 10; }
               while (t > 0) buf[i++] = tmp[--t]; }
        buf[i] = 0; cpu::serialPuts(buf);
    }
    cpu::serialPutc('\n');
    
    if (length < ETHERNET_HEADER_SIZE) {
        iface->rxErrors++;
        return;
    }
    
    iface->rxPackets++;
    iface->rxBytes += length;
    
    const EthernetHeader* eth = reinterpret_cast<const EthernetHeader*>(data);
    processEthernet(iface, eth, length);
}

bool NetworkStack::sendPacket(u32 interfaceId, const void* data, usize length) {
    NetworkInterface* iface = getInterface(interfaceId);
    if (!iface || iface->state != InterfaceState::Up) {
        return false;
    }
    
    iface->txPackets++;
    iface->txBytes += length;

    // Forward packet to VirtIO driver
    drivers::VirtioNet::sendPacket(0, data, length);

    return true;
}

bool NetworkStack::isInitialized() {
    return sInitialized;
}

void NetworkStack::processEthernet(NetworkInterface* iface, const EthernetHeader* eth, usize length) {
    if (!eth->dest.isBroadcast() && !(eth->dest == iface->mac)) {
        cpu::serialPuts("[NET] ETH: MAC mismatch, dropped\n");
        return;
    }
    cpu::serialPuts("[NET] ETH: type=0x");
    { u16 et = ntohs(eth->ethertype); const char h[] = "0123456789ABCDEF";
      cpu::serialPutc(h[(et>>12)&0xF]); cpu::serialPutc(h[(et>>8)&0xF]);
      cpu::serialPutc(h[(et>>4)&0xF]); cpu::serialPutc(h[et&0xF]); }
    cpu::serialPutc('\n');
    
    u16 ethertype = ntohs(eth->ethertype);
    const u8* payload = reinterpret_cast<const u8*>(eth) + ETHERNET_HEADER_SIZE;
    usize payloadLen = length - ETHERNET_HEADER_SIZE;
    
    switch (ethertype) {
        case ETHERTYPE_IPV4:
            if (payloadLen >= IP_HEADER_SIZE) {
                processIpv4(iface, reinterpret_cast<const IPv4Header*>(payload), payloadLen);
            }
            break;
            
        case ETHERTYPE_ARP:
            if (payloadLen >= sizeof(ArpPacket)) {
                processArp(iface, reinterpret_cast<const ArpPacket*>(payload));
            }
            break;
            
        default:
            break;
    }
}

void NetworkStack::processArp(NetworkInterface* iface, const ArpPacket* arp) {
    if (ntohs(arp->hardwareType) != ARP_HARDWARE_ETHERNET) {
        return;
    }
    
    if (ntohs(arp->protocolType) != ETHERTYPE_IPV4) {
        return;
    }
    
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        if (sArpCache[i].valid && sArpCache[i].ip == arp->senderIp) {
            sArpCache[i].mac = arp->senderMac;
            sArpCache[i].pending = false;
            break;
        }
    }
    
    if (!(arp->targetIp == iface->ip)) {
        return;
    }
    
    if (ntohs(arp->opcode) == ARP_OP_REQUEST) {
        u8 replyBuffer[sizeof(EthernetHeader) + sizeof(ArpPacket)];
        
        EthernetHeader* ethReply = reinterpret_cast<EthernetHeader*>(replyBuffer);
        ethReply->dest = arp->senderMac;
        ethReply->src = iface->mac;
        ethReply->ethertype = htons(ETHERTYPE_ARP);
        
        ArpPacket* arpReply = reinterpret_cast<ArpPacket*>(replyBuffer + sizeof(EthernetHeader));
        arpReply->hardwareType = htons(ARP_HARDWARE_ETHERNET);
        arpReply->protocolType = htons(ETHERTYPE_IPV4);
        arpReply->hardwareSize = 6;
        arpReply->protocolSize = 4;
        arpReply->opcode = htons(ARP_OP_REPLY);
        arpReply->senderMac = iface->mac;
        arpReply->senderIp = iface->ip;
        arpReply->targetMac = arp->senderMac;
        arpReply->targetIp = arp->senderIp;
        
        sendPacket(iface->id, replyBuffer, sizeof(replyBuffer));
    }
    
    bool found = false;
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        if (sArpCache[i].valid && sArpCache[i].ip == arp->senderIp) {
            found = true;
            break;
        }
    }
    
    if (!found && sArpCacheCount < ARP_CACHE_SIZE) {
        for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
            if (!sArpCache[i].valid) {
                sArpCache[i].ip = arp->senderIp;
                sArpCache[i].mac = arp->senderMac;
                sArpCache[i].valid = true;
                sArpCache[i].pending = false;
                sArpCacheCount++;
                break;
            }
        }
    }
}

void NetworkStack::processIpv4(NetworkInterface* iface, const IPv4Header* ip, usize length) {
    if (ip->version() != 4) {
        return;
    }
    
    if (length < ip->headerLength()) {
        return;
    }
    
    u16 checksum = calculateChecksum(ip, ip->headerLength());
    if (checksum != 0) {
        iface->rxErrors++;
        return;
    }
    
    if (!(ip->dstAddr == iface->ip) && !ip->dstAddr.isBroadcast()) {
        return;
    }
    
    const u8* payload = reinterpret_cast<const u8*>(ip) + ip->headerLength();
    usize payloadLen = ntohs(ip->totalLength) - ip->headerLength();
    
    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            if (payloadLen >= sizeof(IcmpHeader)) {
                processIcmp(iface, ip, reinterpret_cast<const IcmpHeader*>(payload), payloadLen);
            }
            break;
            
        case IP_PROTO_TCP:
            if (payloadLen >= TCP_HEADER_SIZE) {
                processTcp(iface, ip, reinterpret_cast<const TcpHeader*>(payload), payloadLen);
            }
            break;
            
        case IP_PROTO_UDP:
            if (payloadLen >= UDP_HEADER_SIZE) {
                processUdp(iface, ip, reinterpret_cast<const UdpHeader*>(payload), payloadLen);
            }
            break;
            
        default:
            break;
    }
}

void NetworkStack::processIcmp(NetworkInterface* iface, const IPv4Header* ip, const IcmpHeader* icmp, usize length) {
    if (icmp->type == ICMP_ECHO_REQUEST) {
        u8 replyBuffer[MAX_PACKET_SIZE];
        
        EthernetHeader* eth = reinterpret_cast<EthernetHeader*>(replyBuffer);
        IPv4Header* ipReply = reinterpret_cast<IPv4Header*>(replyBuffer + ETHERNET_HEADER_SIZE);
        IcmpHeader* icmpReply = reinterpret_cast<IcmpHeader*>(replyBuffer + ETHERNET_HEADER_SIZE + IP_HEADER_SIZE);
        
        for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
            if (sArpCache[i].valid && sArpCache[i].ip == ip->srcAddr) {
                eth->dest = sArpCache[i].mac;
                break;
            }
        }
        eth->src = iface->mac;
        eth->ethertype = htons(ETHERTYPE_IPV4);
        
        ipReply->versionIhl = 0x45;
        ipReply->tos = 0;
        ipReply->totalLength = htons(IP_HEADER_SIZE + length);
        ipReply->identification = 0;
        ipReply->flagsFragment = 0;
        ipReply->ttl = 64;
        ipReply->protocol = IP_PROTO_ICMP;
        ipReply->checksum = 0;
        ipReply->srcAddr = iface->ip;
        ipReply->dstAddr = ip->srcAddr;
        ipReply->checksum = calculateChecksum(ipReply, IP_HEADER_SIZE);
        
        icmpReply->type = ICMP_ECHO_REPLY;
        icmpReply->code = 0;
        icmpReply->checksum = 0;
        icmpReply->identifier = icmp->identifier;
        icmpReply->sequence = icmp->sequence;
        
        const u8* icmpData = reinterpret_cast<const u8*>(icmp) + sizeof(IcmpHeader);
        usize dataLen = length - sizeof(IcmpHeader);
        memcpy(reinterpret_cast<u8*>(icmpReply) + sizeof(IcmpHeader), icmpData, dataLen);
        
        icmpReply->checksum = calculateChecksum(icmpReply, length);
        
        sendPacket(iface->id, replyBuffer, ETHERNET_HEADER_SIZE + IP_HEADER_SIZE + length);
    }
}

void NetworkStack::processTcp(NetworkInterface* iface, const IPv4Header* ip, const TcpHeader* tcp, usize length) {
    const u8* data = reinterpret_cast<const u8*>(tcp) + tcp->headerLength();
    usize dataLen = length - tcp->headerLength();
    
    Tcp::processSegment(iface, ip, tcp, data, dataLen);
}

void NetworkStack::processUdp(NetworkInterface* iface, const IPv4Header* ip, const UdpHeader* udp, usize length) {
    (void)iface;

    u16 srcPort = ntohs(udp->srcPort);
    u16 dstPort = ntohs(udp->dstPort);
    cpu::serialPuts("[NET] UDP: src=");
    { u32 n = srcPort; char b[8]; int i=0; if(n==0)b[i++]='0'; else{char t[8];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
    cpu::serialPuts(" dst=");
    { u32 n = dstPort; char b[8]; int i=0; if(n==0)b[i++]='0'; else{char t[8];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
    cpu::serialPutc('\n');
    u16 udpLen = ntohs(udp->length);

    if (udpLen < UDP_HEADER_SIZE || udpLen > length) return;

    const u8* payload = reinterpret_cast<const u8*>(udp) + UDP_HEADER_SIZE;
    usize payloadLen = udpLen - UDP_HEADER_SIZE;

    // Find a matching UDP socket bound to this port
    for (u32 i = 0; i < MAX_SOCKETS; i++) {
        Socket* sock = SocketManager::getSocket(static_cast<i32>(i));
        if (!sock || sock->type != SOCK_DGRAM) continue;
        if (sock->localPort != dstPort && sock->localPort != 0) continue;

        // Write datagram into socket's UDP RX buffer:
        // Format: [u32 srcIp][u16 srcPort][u16 dataLen][data...]
        u32 entrySize = 4 + 2 + 2 + static_cast<u32>(payloadLen);
        if (entrySize > Socket::UDP_RX_BUF_SIZE) continue;

        // Check if there's enough space in the ring buffer
        u32 used = (sock->udpRxHead >= sock->udpRxTail)
            ? (sock->udpRxHead - sock->udpRxTail)
            : (Socket::UDP_RX_BUF_SIZE - sock->udpRxTail + sock->udpRxHead);
        u32 avail = Socket::UDP_RX_BUF_SIZE - used - 1;
        if (entrySize > avail) continue;

        // Write source IP
        u32 srcIp = ip->srcAddr.value;
        for (u32 b = 0; b < 4; b++) {
            sock->udpRxBuffer[(sock->udpRxHead + b) % Socket::UDP_RX_BUF_SIZE] =
                reinterpret_cast<const u8*>(&srcIp)[b];
        }
        u32 pos = sock->udpRxHead + 4;

        // Write source port (network byte order)
        u16 srcPortN = htons(srcPort);
        sock->udpRxBuffer[pos % Socket::UDP_RX_BUF_SIZE] = reinterpret_cast<const u8*>(&srcPortN)[0];
        sock->udpRxBuffer[(pos + 1) % Socket::UDP_RX_BUF_SIZE] = reinterpret_cast<const u8*>(&srcPortN)[1];
        pos += 2;

        // Write data length
        u16 dataLenN = static_cast<u16>(payloadLen);
        sock->udpRxBuffer[pos % Socket::UDP_RX_BUF_SIZE] = reinterpret_cast<const u8*>(&dataLenN)[0];
        sock->udpRxBuffer[(pos + 1) % Socket::UDP_RX_BUF_SIZE] = reinterpret_cast<const u8*>(&dataLenN)[1];
        pos += 2;

        // Write payload
        for (usize j = 0; j < payloadLen; j++) {
            sock->udpRxBuffer[(pos + j) % Socket::UDP_RX_BUF_SIZE] = payload[j];
        }

        sock->udpRxHead = (pos + static_cast<u32>(payloadLen)) % Socket::UDP_RX_BUF_SIZE;
        sock->udpRxCount++;
        break;
    }
}

}
