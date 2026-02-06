#include "../../include/net/dns.hpp"
#include "../../include/net/socket.hpp"
#include "../../include/drivers/virtio_net.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::net {

DnsCacheEntry DnsResolver::sCache[DNS_CACHE_SIZE];
IPv4Address DnsResolver::sDnsServer;
u16 DnsResolver::sNextId = 1;
bool DnsResolver::sInitialized = false;

static bool streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == *b;
}

static u32 slen(const char* s) {
    u32 len = 0;
    while (s[len]) len++;
    return len;
}

static void scpy(char* dst, const char* src, u32 maxLen) {
    u32 i = 0;
    while (src[i] && i < maxLen - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void DnsResolver::initialize() {
    for (u32 i = 0; i < DNS_CACHE_SIZE; i++) {
        sCache[i].valid = false;
    }
    sDnsServer = IPv4Address(10, 0, 2, 3);
    sNextId = 1;
    sInitialized = true;
}

void DnsResolver::setServer(IPv4Address server) {
    sDnsServer = server;
}

bool DnsResolver::parseIpLiteral(const char* str, IPv4Address* outAddr) {
    u8 parts[4];
    u32 partIdx = 0;
    u32 val = 0;
    bool hasDigit = false;

    for (u32 i = 0; ; i++) {
        char c = str[i];
        if (c >= '0' && c <= '9') {
            val = val * 10 + (c - '0');
            if (val > 255) return false;
            hasDigit = true;
        } else if (c == '.' || c == '\0') {
            if (!hasDigit || partIdx >= 4) return false;
            parts[partIdx++] = static_cast<u8>(val);
            val = 0;
            hasDigit = false;
            if (c == '\0') break;
        } else {
            return false;
        }
    }

    if (partIdx != 4) return false;
    *outAddr = IPv4Address(parts[0], parts[1], parts[2], parts[3]);
    return true;
}

bool DnsResolver::resolve(const char* hostname, IPv4Address* outAddr) {
    if (!hostname || !outAddr) return false;

    // Check if it's an IP literal
    if (parseIpLiteral(hostname, outAddr)) return true;

    // Check cache
    DnsCacheEntry* cached = cacheLookup(hostname);
    if (cached) {
        *outAddr = cached->address;
        return true;
    }

    // Send DNS query
    for (u32 retry = 0; retry < DNS_MAX_RETRIES; retry++) {
        if (sendQuery(hostname, outAddr)) {
            cacheInsert(hostname, *outAddr);
            return true;
        }
    }

    return false;
}

bool DnsResolver::sendQuery(const char* hostname, IPv4Address* outAddr) {
    cpu::serialPuts("[DNS] sendQuery: ");
    cpu::serialPuts(hostname);
    cpu::serialPutc('\n');

    i32 sockfd = SocketManager::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        cpu::serialPuts("[DNS] socket() failed\n");
        return false;
    }

    // Bind to an ephemeral port
    SockAddrIn bindAddr;
    bindAddr.family = AF_INET;
    bindAddr.port = 0;
    bindAddr.addr = 0;
    for (int i = 0; i < 8; i++) bindAddr.zero[i] = 0;
    SocketManager::bind(sockfd, reinterpret_cast<SockAddr*>(&bindAddr), sizeof(SockAddrIn));

    // Build DNS query
    u8 queryBuf[512];
    u32 offset = 0;

    // Header
    auto* hdr = reinterpret_cast<DnsHeader*>(queryBuf);
    hdr->id = htons(sNextId++);
    hdr->flags = htons(DNS_FLAG_RD);
    hdr->qdCount = htons(1);
    hdr->anCount = 0;
    hdr->nsCount = 0;
    hdr->arCount = 0;
    offset = sizeof(DnsHeader);

    // Encode hostname
    offset += encodeName(hostname, queryBuf + offset);

    // Type A, Class IN
    queryBuf[offset++] = 0; queryBuf[offset++] = DNS_TYPE_A;
    queryBuf[offset++] = 0; queryBuf[offset++] = DNS_CLASS_IN;

    // Send to DNS server
    SockAddrIn dnsAddr;
    dnsAddr.family = AF_INET;
    dnsAddr.port = htons(DNS_PORT);
    dnsAddr.addr = sDnsServer.value;
    for (int i = 0; i < 8; i++) dnsAddr.zero[i] = 0;

    i64 sendResult = SocketManager::sendto(sockfd, queryBuf, offset, 0,
                          reinterpret_cast<SockAddr*>(&dnsAddr), sizeof(SockAddrIn));
    cpu::serialPuts("[DNS] sendto returned: ");
    if (sendResult > 0) cpu::serialPuts("OK");
    else cpu::serialPuts("FAIL");
    cpu::serialPutc('\n');

    // Poll for response
    u8 respBuf[512];
    bool resolved = false;
    u32 waited = 0;

    while (waited < DNS_TIMEOUT_MS) {
        drivers::VirtioNet::receivePackets(0);

        Socket* sock = SocketManager::getSocket(sockfd);
        if (sock && sock->udpRxCount > 0) {
            SockAddrIn srcAddr;
            u32 srcLen = sizeof(SockAddrIn);
            i64 n = SocketManager::recvfrom(sockfd, respBuf, sizeof(respBuf), 0,
                                             reinterpret_cast<SockAddr*>(&srcAddr), &srcLen);
            if (n > 0) {
                resolved = parseResponse(respBuf, static_cast<usize>(n), outAddr);
                break;
            }
        }

        // Small delay (~1ms worth of spins)
        for (volatile int d = 0; d < 50000; d++);
        waited += 5;
    }

    cpu::serialPuts("[DNS] resolved=");
    cpu::serialPutc(resolved ? '1' : '0');
    cpu::serialPutc('\n');

    SocketManager::close(sockfd);
    return resolved;
}

u32 DnsResolver::encodeName(const char* hostname, u8* buffer) {
    u32 pos = 0;
    u32 len = slen(hostname);
    u32 labelStart = 0;

    for (u32 i = 0; i <= len; i++) {
        if (hostname[i] == '.' || hostname[i] == '\0') {
            u32 labelLen = i - labelStart;
            if (labelLen > 63) labelLen = 63;
            buffer[pos++] = static_cast<u8>(labelLen);
            for (u32 j = 0; j < labelLen; j++) {
                buffer[pos++] = hostname[labelStart + j];
            }
            labelStart = i + 1;
        }
    }

    buffer[pos++] = 0; // Terminator
    return pos;
}

u32 DnsResolver::skipName(const u8* data, u32 offset, u32 length) {
    while (offset < length) {
        u8 labelLen = data[offset];
        if (labelLen == 0) {
            return offset + 1;
        }
        if ((labelLen & 0xC0) == 0xC0) {
            // Compression pointer
            return offset + 2;
        }
        offset += labelLen + 1;
    }
    return offset;
}

bool DnsResolver::parseResponse(const u8* data, usize length, IPv4Address* outAddr) {
    if (length < sizeof(DnsHeader)) return false;

    auto* hdr = reinterpret_cast<const DnsHeader*>(data);
    u16 flags = ntohs(hdr->flags);
    if (!(flags & DNS_FLAG_QR)) return false; // Not a response

    u16 qdCount = ntohs(hdr->qdCount);
    u16 anCount = ntohs(hdr->anCount);
    if (anCount == 0) return false;

    // Skip question section
    u32 offset = sizeof(DnsHeader);
    for (u16 i = 0; i < qdCount; i++) {
        offset = skipName(data, offset, static_cast<u32>(length));
        offset += 4; // type + class
    }

    // Parse answer section
    for (u16 i = 0; i < anCount && offset < length; i++) {
        offset = skipName(data, offset, static_cast<u32>(length));
        if (offset + 10 > length) return false;

        u16 type = (static_cast<u16>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        // u16 cls = ...
        offset += 2; // class
        offset += 4; // TTL
        u16 rdLength = (static_cast<u16>(data[offset]) << 8) | data[offset + 1];
        offset += 2;

        if (type == DNS_TYPE_A && rdLength == 4 && offset + 4 <= length) {
            *outAddr = IPv4Address(data[offset], data[offset + 1],
                                    data[offset + 2], data[offset + 3]);
            return true;
        }

        offset += rdLength;
    }

    return false;
}

DnsCacheEntry* DnsResolver::cacheLookup(const char* hostname) {
    for (u32 i = 0; i < DNS_CACHE_SIZE; i++) {
        if (sCache[i].valid && streq(sCache[i].hostname, hostname)) {
            return &sCache[i];
        }
    }
    return nullptr;
}

void DnsResolver::cacheInsert(const char* hostname, IPv4Address addr) {
    // Find an empty slot or overwrite oldest
    u32 slot = 0;
    for (u32 i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!sCache[i].valid) {
            slot = i;
            break;
        }
        slot = i; // Will overwrite last slot if all full
    }

    scpy(sCache[slot].hostname, hostname, 128);
    sCache[slot].address = addr;
    sCache[slot].timestamp = 0;
    sCache[slot].valid = true;
}

}
