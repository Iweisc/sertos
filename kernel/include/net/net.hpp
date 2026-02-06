#pragma once

#include "../types.hpp"

namespace sertos::net {

constexpr u32 MAX_NETWORK_INTERFACES = 8;
constexpr u32 MAX_SOCKETS = 256;
constexpr u32 MAX_PACKET_SIZE = 1518;
constexpr u32 MTU = 1500;
constexpr u32 ETHERNET_HEADER_SIZE = 14;
constexpr u32 IP_HEADER_SIZE = 20;
constexpr u32 TCP_HEADER_SIZE = 20;
constexpr u32 UDP_HEADER_SIZE = 8;
constexpr u32 ARP_CACHE_SIZE = 64;
constexpr u32 TCP_WINDOW_SIZE = 65535;
constexpr u32 TCP_MAX_CONNECTIONS = 128;
constexpr u32 DNS_CACHE_SIZE = 32;

constexpr u16 ETHERTYPE_IPV4 = 0x0800;
constexpr u16 ETHERTYPE_ARP = 0x0806;
constexpr u16 ETHERTYPE_IPV6 = 0x86DD;

constexpr u8 IP_PROTO_ICMP = 1;
constexpr u8 IP_PROTO_TCP = 6;
constexpr u8 IP_PROTO_UDP = 17;

constexpr u16 ARP_HARDWARE_ETHERNET = 1;
constexpr u16 ARP_OP_REQUEST = 1;
constexpr u16 ARP_OP_REPLY = 2;

constexpr u8 ICMP_ECHO_REPLY = 0;
constexpr u8 ICMP_ECHO_REQUEST = 8;

constexpr u8 TCP_FLAG_FIN = 0x01;
constexpr u8 TCP_FLAG_SYN = 0x02;
constexpr u8 TCP_FLAG_RST = 0x04;
constexpr u8 TCP_FLAG_PSH = 0x08;
constexpr u8 TCP_FLAG_ACK = 0x10;
constexpr u8 TCP_FLAG_URG = 0x20;

struct MacAddress {
    u8 bytes[6];
    
    bool operator==(const MacAddress& other) const {
        for (int i = 0; i < 6; i++) {
            if (bytes[i] != other.bytes[i]) return false;
        }
        return true;
    }
    
    bool isZero() const {
        for (int i = 0; i < 6; i++) {
            if (bytes[i] != 0) return false;
        }
        return true;
    }
    
    bool isBroadcast() const {
        for (int i = 0; i < 6; i++) {
            if (bytes[i] != 0xFF) return false;
        }
        return true;
    }
};

struct __attribute__((packed)) IPv4Address {
    union {
        u8 bytes[4];
        u32 value;
    };

    IPv4Address() : value(0) {}
    IPv4Address(u32 v) : value(v) {}
    IPv4Address(u8 a, u8 b, u8 c, u8 d) {
        bytes[0] = a; bytes[1] = b; bytes[2] = c; bytes[3] = d;
    }

    bool operator==(const IPv4Address& other) const {
        return value == other.value;
    }

    bool isZero() const { return value == 0; }
    bool isBroadcast() const { return value == 0xFFFFFFFF; }
    bool isMulticast() const { return (bytes[0] & 0xF0) == 0xE0; }
    bool isLoopback() const { return bytes[0] == 127; }

    bool isInSubnet(IPv4Address network, IPv4Address mask) const {
        return (value & mask.value) == (network.value & mask.value);
    }
};

struct EthernetHeader {
    MacAddress dest;
    MacAddress src;
    u16 ethertype;
} __attribute__((packed));

struct IPv4Header {
    u8 versionIhl;
    u8 tos;
    u16 totalLength;
    u16 identification;
    u16 flagsFragment;
    u8 ttl;
    u8 protocol;
    u16 checksum;
    IPv4Address srcAddr;
    IPv4Address dstAddr;
    
    u8 version() const { return versionIhl >> 4; }
    u8 headerLength() const { return (versionIhl & 0x0F) * 4; }
} __attribute__((packed));

struct ArpPacket {
    u16 hardwareType;
    u16 protocolType;
    u8 hardwareSize;
    u8 protocolSize;
    u16 opcode;
    MacAddress senderMac;
    IPv4Address senderIp;
    MacAddress targetMac;
    IPv4Address targetIp;
} __attribute__((packed));

struct IcmpHeader {
    u8 type;
    u8 code;
    u16 checksum;
    u16 identifier;
    u16 sequence;
} __attribute__((packed));

struct TcpHeader {
    u16 srcPort;
    u16 dstPort;
    u32 seqNum;
    u32 ackNum;
    u8 dataOffset;
    u8 flags;
    u16 window;
    u16 checksum;
    u16 urgentPtr;
    
    u8 headerLength() const { return (dataOffset >> 4) * 4; }
} __attribute__((packed));

struct UdpHeader {
    u16 srcPort;
    u16 dstPort;
    u16 length;
    u16 checksum;
} __attribute__((packed));

struct PseudoHeader {
    IPv4Address srcAddr;
    IPv4Address dstAddr;
    u8 zero;
    u8 protocol;
    u16 length;
} __attribute__((packed));

enum class InterfaceState : u8 {
    Down = 0,
    Up,
    Error
};

struct NetworkInterface {
    u32 id;
    char name[16];
    MacAddress mac;
    IPv4Address ip;
    IPv4Address netmask;
    IPv4Address gateway;
    IPv4Address dns;
    InterfaceState state;
    u64 rxPackets;
    u64 txPackets;
    u64 rxBytes;
    u64 txBytes;
    u64 rxErrors;
    u64 txErrors;
    bool active;
    void* driverData;
};

struct ArpEntry {
    IPv4Address ip;
    MacAddress mac;
    u64 timestamp;
    bool valid;
    bool pending;
};

inline u16 htons(u16 value) {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

inline u16 ntohs(u16 value) {
    return htons(value);
}

inline u32 htonl(u32 value) {
    return ((value & 0xFF) << 24) |
           ((value & 0xFF00) << 8) |
           ((value >> 8) & 0xFF00) |
           ((value >> 24) & 0xFF);
}

inline u32 ntohl(u32 value) {
    return htonl(value);
}

u16 calculateChecksum(const void* data, usize length);
u16 calculateTcpChecksum(const IPv4Header* ip, const TcpHeader* tcp, const void* data, usize dataLen);
u16 calculateUdpChecksum(const IPv4Header* ip, const UdpHeader* udp, const void* data, usize dataLen);

class NetworkStack {
public:
    static void initialize();
    
    static NetworkInterface* registerInterface(const char* name, MacAddress mac);
    static void unregisterInterface(u32 id);
    static NetworkInterface* getInterface(u32 id);
    static NetworkInterface* getDefaultInterface();
    
    static bool configureInterface(u32 id, IPv4Address ip, IPv4Address netmask, IPv4Address gateway);
    static bool setInterfaceUp(u32 id);
    static bool setInterfaceDown(u32 id);
    
    static void receivePacket(u32 interfaceId, const void* data, usize length);
    static bool sendPacket(u32 interfaceId, const void* data, usize length);
    
    static bool isInitialized();

private:
    static void processEthernet(NetworkInterface* iface, const EthernetHeader* eth, usize length);
    static void processArp(NetworkInterface* iface, const ArpPacket* arp);
    static void processIpv4(NetworkInterface* iface, const IPv4Header* ip, usize length);
    static void processIcmp(NetworkInterface* iface, const IPv4Header* ip, const IcmpHeader* icmp, usize length);
    static void processTcp(NetworkInterface* iface, const IPv4Header* ip, const TcpHeader* tcp, usize length);
    static void processUdp(NetworkInterface* iface, const IPv4Header* ip, const UdpHeader* udp, usize length);
    
    static NetworkInterface sInterfaces[MAX_NETWORK_INTERFACES];
    static u32 sInterfaceCount;
    static u32 sDefaultInterface;
    static bool sInitialized;
};

}
