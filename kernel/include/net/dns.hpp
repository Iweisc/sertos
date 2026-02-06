#pragma once

#include "net.hpp"

namespace sertos::net {

constexpr u16 DNS_PORT = 53;
constexpr u32 DNS_TIMEOUT_MS = 3000;
constexpr u32 DNS_MAX_RETRIES = 2;

struct DnsHeader {
    u16 id;
    u16 flags;
    u16 qdCount;
    u16 anCount;
    u16 nsCount;
    u16 arCount;
} __attribute__((packed));

constexpr u16 DNS_FLAG_QR       = 0x8000;
constexpr u16 DNS_FLAG_RD       = 0x0100;
constexpr u16 DNS_FLAG_RA       = 0x0080;
constexpr u16 DNS_TYPE_A        = 1;
constexpr u16 DNS_CLASS_IN      = 1;

struct DnsCacheEntry {
    char hostname[128];
    IPv4Address address;
    u64 timestamp;
    bool valid;
};

class DnsResolver {
public:
    static void initialize();
    static bool resolve(const char* hostname, IPv4Address* outAddr);
    static void setServer(IPv4Address server);

private:
    static bool parseIpLiteral(const char* str, IPv4Address* outAddr);
    static bool sendQuery(const char* hostname, IPv4Address* outAddr);
    static u32  encodeName(const char* hostname, u8* buffer);
    static bool parseResponse(const u8* data, usize length, IPv4Address* outAddr);
    static u32  skipName(const u8* data, u32 offset, u32 length);
    static DnsCacheEntry* cacheLookup(const char* hostname);
    static void cacheInsert(const char* hostname, IPv4Address addr);

    static DnsCacheEntry sCache[DNS_CACHE_SIZE];
    static IPv4Address sDnsServer;
    static u16 sNextId;
    static bool sInitialized;
};

}
