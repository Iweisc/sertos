#include "../../include/net/socket.hpp"
#include "../../include/process/process.hpp"

namespace sertos::net {

Socket SocketManager::sSockets[MAX_SOCKETS];
u32 SocketManager::sSocketCount = 0;
bool SocketManager::sInitialized = false;

static void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

void SocketManager::initialize() {
    if (sInitialized) {
        return;
    }
    
    for (u32 i = 0; i < MAX_SOCKETS; i++) {
        sSockets[i].id = 0;
        sSockets[i].state = SocketState::Invalid;
        sSockets[i].active = false;
    }
    
    sSocketCount = 0;
    sInitialized = true;
}

i32 SocketManager::socket(i32 domain, i32 type, i32 protocol) {
    if (!sInitialized) {
        return -1;
    }
    
    if (domain != AF_INET) {
        return -97;
    }
    
    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        return -94;
    }
    
    i32 sockfd = allocateSocket();
    if (sockfd < 0) {
        return -23;
    }
    
    Socket* sock = &sSockets[sockfd];
    
    memset(sock, 0, sizeof(Socket));
    sock->id = sockfd + 1;
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;
    sock->state = SocketState::Created;
    sock->active = true;
    sock->blocking = true;
    sock->rxBufferSize = 65536;
    sock->txBufferSize = 65536;
    
    process::Process* proc = process::PM::currentProcess();
    if (proc) {
        sock->ownerPid = proc->pid;
    }
    
    if (type == SOCK_STREAM) {
        sock->tcpConn = Tcp::createConnection();
        if (!sock->tcpConn) {
            sock->active = false;
            return -23;
        }
        sock->tcpConn->socketId = sock->id;
    }
    
    sSocketCount++;
    return sockfd;
}

i32 SocketManager::bind(i32 sockfd, const SockAddr* addr, u32 addrlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (!addr || addrlen < sizeof(SockAddrIn)) {
        return -22;
    }
    
    IPv4Address ip;
    u16 port;
    sockAddrToIpPort(addr, &ip, &port);
    
    sock->localAddr = ip;
    sock->localPort = port;
    sock->state = SocketState::Bound;
    
    if (sock->tcpConn) {
        return Tcp::bind(sock->tcpConn, ip, port);
    }
    
    return 0;
}

i32 SocketManager::listen(i32 sockfd, i32 backlog) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (sock->type != SOCK_STREAM) {
        return -95;
    }
    
    if (!sock->tcpConn) {
        return -1;
    }
    
    i64 result = Tcp::listen(sock->tcpConn, backlog);
    if (result == 0) {
        sock->state = SocketState::Listening;
    }
    
    return result;
}

i32 SocketManager::accept(i32 sockfd, SockAddr* addr, u32* addrlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (sock->state != SocketState::Listening) {
        return -22;
    }
    
    if (!sock->tcpConn) {
        return -1;
    }
    
    TcpConnection* newConn = Tcp::accept(sock->tcpConn);
    if (!newConn) {
        return -11;
    }
    
    i32 newSockfd = allocateSocket();
    if (newSockfd < 0) {
        return -23;
    }
    
    Socket* newSock = &sSockets[newSockfd];
    
    memset(newSock, 0, sizeof(Socket));
    newSock->id = newSockfd + 1;
    newSock->domain = sock->domain;
    newSock->type = sock->type;
    newSock->protocol = sock->protocol;
    newSock->state = SocketState::Connected;
    newSock->active = true;
    newSock->blocking = sock->blocking;
    newSock->tcpConn = newConn;
    newSock->localAddr = newConn->localAddr;
    newSock->localPort = newConn->localPort;
    newSock->remoteAddr = newConn->remoteAddr;
    newSock->remotePort = newConn->remotePort;
    newSock->ownerPid = sock->ownerPid;
    
    newConn->socketId = newSock->id;
    
    if (addr && addrlen && *addrlen >= sizeof(SockAddrIn)) {
        ipPortToSockAddr(newConn->remoteAddr, newConn->remotePort, addr);
        *addrlen = sizeof(SockAddrIn);
    }
    
    sSocketCount++;
    return newSockfd;
}

i32 SocketManager::connect(i32 sockfd, const SockAddr* addr, u32 addrlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (!addr || addrlen < sizeof(SockAddrIn)) {
        return -22;
    }
    
    IPv4Address ip;
    u16 port;
    sockAddrToIpPort(addr, &ip, &port);

    sock->remoteAddr = ip;
    sock->remotePort = port;

    if (sock->type == SOCK_DGRAM) {
        // For UDP, connect just stores the remote address
        sock->state = SocketState::Connected;
        return 0;
    }

    if (sock->type != SOCK_STREAM) {
        return -95;
    }

    if (!sock->tcpConn) {
        return -1;
    }

    sock->state = SocketState::Connecting;

    i64 result = Tcp::connect(sock->tcpConn, ip, port);
    if (result == 0) {
        sock->state = SocketState::Connected;
        sock->localAddr = sock->tcpConn->localAddr;
        sock->localPort = sock->tcpConn->localPort;
    }

    return result;
}

i64 SocketManager::send(i32 sockfd, const void* buf, usize len, i32 flags) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (sock->state != SocketState::Connected) {
        return -107;
    }
    
    if (!buf || len == 0) {
        return 0;
    }
    
    (void)flags;
    
    if (sock->tcpConn) {
        return Tcp::send(sock->tcpConn, buf, len);
    }
    
    return -1;
}

i64 SocketManager::recv(i32 sockfd, void* buf, usize len, i32 flags) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (sock->state != SocketState::Connected && sock->state != SocketState::Closing) {
        return -107;
    }
    
    if (!buf || len == 0) {
        return 0;
    }
    
    (void)flags;
    
    if (sock->tcpConn) {
        return Tcp::recv(sock->tcpConn, buf, len);
    }
    
    return -1;
}

i64 SocketManager::sendto(i32 sockfd, const void* buf, usize len, i32 flags, 
                          const SockAddr* destAddr, u32 addrlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (sock->type == SOCK_STREAM) {
        return send(sockfd, buf, len, flags);
    }

    // UDP sendto
    IPv4Address dstIp;
    u16 dstPort;
    if (destAddr && addrlen >= sizeof(SockAddrIn)) {
        sockAddrToIpPort(destAddr, &dstIp, &dstPort);
    } else if (sock->state == SocketState::Connected) {
        dstIp = sock->remoteAddr;
        dstPort = sock->remotePort;
    } else {
        return -89; // EDESTADDRREQ
    }

    // Get network interface
    NetworkInterface* iface = NetworkStack::getDefaultInterface();
    if (!iface) return -100; // ENETDOWN

    // Assign ephemeral local port if not bound
    if (sock->localPort == 0) {
        static u16 nextEphemeral = 49152;
        sock->localPort = nextEphemeral++;
        if (nextEphemeral > 65500) nextEphemeral = 49152;
    }

    // Build packet: Ethernet + IP + UDP + payload
    if (len > 1472) len = 1472; // MTU - IP - UDP headers
    u8 pktBuf[1518];
    usize pktLen = ETHERNET_HEADER_SIZE + IP_HEADER_SIZE + UDP_HEADER_SIZE + len;

    // Ethernet header
    auto* eth = reinterpret_cast<EthernetHeader*>(pktBuf);
    // Use broadcast MAC for simplicity (QEMU user-mode accepts this)
    for (int i = 0; i < 6; i++) eth->dest.bytes[i] = 0xFF;
    eth->src = iface->mac;
    eth->ethertype = htons(ETHERTYPE_IPV4);

    // IP header
    auto* iphdr = reinterpret_cast<IPv4Header*>(pktBuf + ETHERNET_HEADER_SIZE);
    static u16 ipId = 1;
    u8* ipBytes = reinterpret_cast<u8*>(iphdr);
    for (usize i = 0; i < IP_HEADER_SIZE; i++) ipBytes[i] = 0;
    iphdr->versionIhl = 0x45;
    iphdr->ttl = 64;
    iphdr->protocol = IP_PROTO_UDP;
    iphdr->totalLength = htons(static_cast<u16>(IP_HEADER_SIZE + UDP_HEADER_SIZE + len));
    iphdr->identification = htons(ipId++);
    iphdr->srcAddr = iface->ip;
    iphdr->dstAddr = dstIp;
    iphdr->checksum = 0;
    iphdr->checksum = calculateChecksum(iphdr, IP_HEADER_SIZE);

    // UDP header
    auto* udphdr = reinterpret_cast<UdpHeader*>(pktBuf + ETHERNET_HEADER_SIZE + IP_HEADER_SIZE);
    udphdr->srcPort = htons(sock->localPort);
    udphdr->dstPort = htons(dstPort);
    udphdr->length = htons(static_cast<u16>(UDP_HEADER_SIZE + len));
    udphdr->checksum = 0; // Optional for IPv4

    // Copy payload
    u8* payload = pktBuf + ETHERNET_HEADER_SIZE + IP_HEADER_SIZE + UDP_HEADER_SIZE;
    const u8* srcBuf = reinterpret_cast<const u8*>(buf);
    for (usize i = 0; i < len; i++) payload[i] = srcBuf[i];

    NetworkStack::sendPacket(iface->id, pktBuf, pktLen);
    return static_cast<i64>(len);
}

i64 SocketManager::recvfrom(i32 sockfd, void* buf, usize len, i32 flags,
                            SockAddr* srcAddr, u32* addrlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (sock->type == SOCK_STREAM) {
        return recv(sockfd, buf, len, flags);
    }

    (void)flags;

    // UDP recvfrom
    if (sock->udpRxCount == 0) {
        return -11; // EAGAIN
    }

    // Read header from ring buffer: [u32 srcIp][u16 srcPort][u16 dataLen]
    u32 pos = sock->udpRxTail;
    u32 srcIp = 0;
    for (u32 b = 0; b < 4; b++) {
        reinterpret_cast<u8*>(&srcIp)[b] = sock->udpRxBuffer[pos % Socket::UDP_RX_BUF_SIZE];
        pos++;
    }

    u16 srcPortN = 0;
    reinterpret_cast<u8*>(&srcPortN)[0] = sock->udpRxBuffer[pos % Socket::UDP_RX_BUF_SIZE]; pos++;
    reinterpret_cast<u8*>(&srcPortN)[1] = sock->udpRxBuffer[pos % Socket::UDP_RX_BUF_SIZE]; pos++;

    u16 dataLen = 0;
    reinterpret_cast<u8*>(&dataLen)[0] = sock->udpRxBuffer[pos % Socket::UDP_RX_BUF_SIZE]; pos++;
    reinterpret_cast<u8*>(&dataLen)[1] = sock->udpRxBuffer[pos % Socket::UDP_RX_BUF_SIZE]; pos++;

    // Read payload
    u32 toCopy = dataLen;
    if (toCopy > len) toCopy = static_cast<u32>(len);
    u8* dst = reinterpret_cast<u8*>(buf);
    for (u32 i = 0; i < toCopy; i++) {
        dst[i] = sock->udpRxBuffer[(pos + i) % Socket::UDP_RX_BUF_SIZE];
    }

    // Advance tail past the full entry (even if we didn't copy all data)
    sock->udpRxTail = (pos + dataLen) % Socket::UDP_RX_BUF_SIZE;
    sock->udpRxCount--;

    // Fill source address if requested
    if (srcAddr && addrlen && *addrlen >= sizeof(SockAddrIn)) {
        IPv4Address ip;
        ip.value = srcIp;
        ipPortToSockAddr(ip, ntohs(srcPortN), srcAddr);
        *addrlen = sizeof(SockAddrIn);
    }

    return static_cast<i64>(toCopy);
}

i32 SocketManager::shutdown(i32 sockfd, i32 how) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    (void)how;
    
    if (sock->tcpConn) {
        Tcp::close(sock->tcpConn);
    }
    
    sock->state = SocketState::Closing;
    
    return 0;
}

i32 SocketManager::close(i32 sockfd) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (sock->tcpConn) {
        Tcp::close(sock->tcpConn);
        Tcp::destroyConnection(sock->tcpConn);
        sock->tcpConn = nullptr;
    }
    
    sock->state = SocketState::Closed;
    sock->active = false;
    sSocketCount--;
    
    return 0;
}

i32 SocketManager::getsockopt(i32 sockfd, i32 level, i32 optname, void* optval, u32* optlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (!optval || !optlen) {
        return -22;
    }
    
    if (level == SOL_SOCKET) {
        switch (optname) {
            case SO_ERROR:
                if (*optlen >= sizeof(i32)) {
                    *reinterpret_cast<i32*>(optval) = sock->lastError;
                    *optlen = sizeof(i32);
                    sock->lastError = 0;
                    return 0;
                }
                break;
                
            case SO_TYPE:
                if (*optlen >= sizeof(i32)) {
                    *reinterpret_cast<i32*>(optval) = sock->type;
                    *optlen = sizeof(i32);
                    return 0;
                }
                break;
                
            case SO_REUSEADDR:
                if (*optlen >= sizeof(i32)) {
                    *reinterpret_cast<i32*>(optval) = sock->reuseAddr ? 1 : 0;
                    *optlen = sizeof(i32);
                    return 0;
                }
                break;
                
            case SO_KEEPALIVE:
                if (*optlen >= sizeof(i32)) {
                    *reinterpret_cast<i32*>(optval) = sock->keepAlive ? 1 : 0;
                    *optlen = sizeof(i32);
                    return 0;
                }
                break;
                
            case SO_RCVBUF:
                if (*optlen >= sizeof(i32)) {
                    *reinterpret_cast<i32*>(optval) = sock->rxBufferSize;
                    *optlen = sizeof(i32);
                    return 0;
                }
                break;
                
            case SO_SNDBUF:
                if (*optlen >= sizeof(i32)) {
                    *reinterpret_cast<i32*>(optval) = sock->txBufferSize;
                    *optlen = sizeof(i32);
                    return 0;
                }
                break;
        }
    } else if (level == SOL_TCP) {
        switch (optname) {
            case TCP_NODELAY:
                if (*optlen >= sizeof(i32)) {
                    *reinterpret_cast<i32*>(optval) = sock->noDelay ? 1 : 0;
                    *optlen = sizeof(i32);
                    return 0;
                }
                break;
        }
    }
    
    return -92;
}

i32 SocketManager::setsockopt(i32 sockfd, i32 level, i32 optname, const void* optval, u32 optlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (!optval) {
        return -22;
    }
    
    if (level == SOL_SOCKET) {
        switch (optname) {
            case SO_REUSEADDR:
                if (optlen >= sizeof(i32)) {
                    sock->reuseAddr = *reinterpret_cast<const i32*>(optval) != 0;
                    return 0;
                }
                break;
                
            case SO_KEEPALIVE:
                if (optlen >= sizeof(i32)) {
                    sock->keepAlive = *reinterpret_cast<const i32*>(optval) != 0;
                    return 0;
                }
                break;
                
            case SO_BROADCAST:
                if (optlen >= sizeof(i32)) {
                    sock->broadcast = *reinterpret_cast<const i32*>(optval) != 0;
                    return 0;
                }
                break;
                
            case SO_RCVBUF:
                if (optlen >= sizeof(i32)) {
                    sock->rxBufferSize = *reinterpret_cast<const i32*>(optval);
                    return 0;
                }
                break;
                
            case SO_SNDBUF:
                if (optlen >= sizeof(i32)) {
                    sock->txBufferSize = *reinterpret_cast<const i32*>(optval);
                    return 0;
                }
                break;
                
            case SO_RCVTIMEO:
                if (optlen >= sizeof(u64)) {
                    sock->rxTimeout = *reinterpret_cast<const u64*>(optval);
                    return 0;
                }
                break;
                
            case SO_SNDTIMEO:
                if (optlen >= sizeof(u64)) {
                    sock->txTimeout = *reinterpret_cast<const u64*>(optval);
                    return 0;
                }
                break;
        }
    } else if (level == SOL_TCP) {
        switch (optname) {
            case TCP_NODELAY:
                if (optlen >= sizeof(i32)) {
                    sock->noDelay = *reinterpret_cast<const i32*>(optval) != 0;
                    return 0;
                }
                break;
        }
    }
    
    return -92;
}

i32 SocketManager::getsockname(i32 sockfd, SockAddr* addr, u32* addrlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (!addr || !addrlen || *addrlen < sizeof(SockAddrIn)) {
        return -22;
    }
    
    ipPortToSockAddr(sock->localAddr, sock->localPort, addr);
    *addrlen = sizeof(SockAddrIn);
    
    return 0;
}

i32 SocketManager::getpeername(i32 sockfd, SockAddr* addr, u32* addrlen) {
    Socket* sock = getSocket(sockfd);
    if (!sock) {
        return -9;
    }
    
    if (sock->state != SocketState::Connected) {
        return -107;
    }
    
    if (!addr || !addrlen || *addrlen < sizeof(SockAddrIn)) {
        return -22;
    }
    
    ipPortToSockAddr(sock->remoteAddr, sock->remotePort, addr);
    *addrlen = sizeof(SockAddrIn);
    
    return 0;
}

Socket* SocketManager::getSocket(i32 sockfd) {
    if (sockfd < 0 || sockfd >= static_cast<i32>(MAX_SOCKETS)) {
        return nullptr;
    }
    
    Socket* sock = &sSockets[sockfd];
    if (sock->active) {
        return sock;
    }
    
    return nullptr;
}

i32 SocketManager::allocateSocket() {
    for (u32 i = 0; i < MAX_SOCKETS; i++) {
        if (!sSockets[i].active) {
            return i;
        }
    }
    return -1;
}

void SocketManager::freeSocket(i32 sockfd) {
    if (sockfd >= 0 && sockfd < static_cast<i32>(MAX_SOCKETS)) {
        sSockets[sockfd].active = false;
    }
}

bool SocketManager::isInitialized() {
    return sInitialized;
}

void SocketManager::sockAddrToIpPort(const SockAddr* addr, IPv4Address* ip, u16* port) {
    const SockAddrIn* sin = reinterpret_cast<const SockAddrIn*>(addr);
    ip->value = sin->addr;
    *port = ntohs(sin->port);
}

void SocketManager::ipPortToSockAddr(IPv4Address ip, u16 port, SockAddr* addr) {
    SockAddrIn* sin = reinterpret_cast<SockAddrIn*>(addr);
    sin->family = AF_INET;
    sin->port = htons(port);
    sin->addr = ip.value;
    for (int i = 0; i < 8; i++) {
        sin->zero[i] = 0;
    }
}

i32 SocketManager::create(i32 domain, i32 type, i32 protocol) {
    return socket(domain, type, protocol);
}

i32 SocketManager::socketpair(i32 domain, i32 type, i32 protocol, i32* sv) {
    if (!sv) return -22;
    
    (void)domain;
    (void)type;
    (void)protocol;
    
    i32 sock1 = allocateSocket();
    if (sock1 < 0) return -23;
    
    i32 sock2 = allocateSocket();
    if (sock2 < 0) {
        freeSocket(sock1);
        return -23;
    }
    
    Socket* s1 = &sSockets[sock1];
    Socket* s2 = &sSockets[sock2];
    
    memset(s1, 0, sizeof(Socket));
    memset(s2, 0, sizeof(Socket));
    
    s1->id = sock1 + 1;
    s1->domain = AF_INET;
    s1->type = SOCK_STREAM;
    s1->protocol = 0;
    s1->state = SocketState::Connected;
    s1->active = true;
    s1->blocking = true;
    
    s2->id = sock2 + 1;
    s2->domain = AF_INET;
    s2->type = SOCK_STREAM;
    s2->protocol = 0;
    s2->state = SocketState::Connected;
    s2->active = true;
    s2->blocking = true;
    
    process::Process* proc = process::PM::currentProcess();
    if (proc) {
        s1->ownerPid = proc->pid;
        s2->ownerPid = proc->pid;
    }
    
    sv[0] = sock1;
    sv[1] = sock2;
    
    sSocketCount += 2;
    return 0;
}

}
