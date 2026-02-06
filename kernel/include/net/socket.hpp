#pragma once

#include "net.hpp"
#include "tcp.hpp"

namespace sertos::net {

constexpr i32 AF_UNSPEC = 0;
constexpr i32 AF_UNIX = 1;
constexpr i32 AF_INET = 2;
constexpr i32 AF_INET6 = 10;

constexpr i32 SOCK_STREAM = 1;
constexpr i32 SOCK_DGRAM = 2;
constexpr i32 SOCK_RAW = 3;

constexpr i32 IPPROTO_IP = 0;
constexpr i32 IPPROTO_ICMP = 1;
constexpr i32 IPPROTO_TCP = 6;
constexpr i32 IPPROTO_UDP = 17;

constexpr i32 SOL_SOCKET = 1;
constexpr i32 SOL_TCP = 6;
constexpr i32 SOL_UDP = 17;

constexpr i32 SO_REUSEADDR = 2;
constexpr i32 SO_KEEPALIVE = 9;
constexpr i32 SO_BROADCAST = 6;
constexpr i32 SO_LINGER = 13;
constexpr i32 SO_RCVBUF = 8;
constexpr i32 SO_SNDBUF = 7;
constexpr i32 SO_RCVTIMEO = 20;
constexpr i32 SO_SNDTIMEO = 21;
constexpr i32 SO_ERROR = 4;
constexpr i32 SO_TYPE = 3;
constexpr i32 SO_ACCEPTCONN = 30;

constexpr i32 TCP_NODELAY = 1;
constexpr i32 TCP_MAXSEG = 2;
constexpr i32 TCP_KEEPIDLE = 4;
constexpr i32 TCP_KEEPINTVL = 5;
constexpr i32 TCP_KEEPCNT = 6;

constexpr i32 MSG_PEEK = 0x02;
constexpr i32 MSG_WAITALL = 0x100;
constexpr i32 MSG_DONTWAIT = 0x40;
constexpr i32 MSG_NOSIGNAL = 0x4000;

constexpr i32 SHUT_RD = 0;
constexpr i32 SHUT_WR = 1;
constexpr i32 SHUT_RDWR = 2;

constexpr i32 INADDR_ANY = 0;
constexpr i32 INADDR_BROADCAST = 0xFFFFFFFF;
constexpr i32 INADDR_LOOPBACK = 0x7F000001;

struct SockAddrIn {
    u16 family;
    u16 port;
    u32 addr;
    u8 zero[8];
} __attribute__((packed));

struct SockAddr {
    u16 family;
    u8 data[14];
} __attribute__((packed));

enum class SocketState : u8 {
    Invalid = 0,
    Created,
    Bound,
    Listening,
    Connecting,
    Connected,
    Closing,
    Closed
};

enum class SocketType : u8 {
    None = 0,
    Stream,
    Datagram,
    Raw
};

struct Socket {
    u32 id;
    i32 domain;
    i32 type;
    i32 protocol;
    SocketState state;
    
    IPv4Address localAddr;
    u16 localPort;
    IPv4Address remoteAddr;
    u16 remotePort;
    
    TcpConnection* tcpConn;

    // UDP receive buffer (ring buffer)
    static constexpr u32 UDP_RX_BUF_SIZE = 8192;
    u8  udpRxBuffer[UDP_RX_BUF_SIZE];
    u32 udpRxHead;
    u32 udpRxTail;
    u32 udpRxCount;

    u32 rxBufferSize;
    u32 txBufferSize;
    u64 rxTimeout;
    u64 txTimeout;
    
    i32 lastError;
    
    bool blocking;
    bool reuseAddr;
    bool keepAlive;
    bool noDelay;
    bool broadcast;
    
    u32 ownerPid;
    bool active;
};

class SocketManager {
public:
    static void initialize();
    
    static i32 create(i32 domain, i32 type, i32 protocol);
    static i32 socket(i32 domain, i32 type, i32 protocol);
    static i32 bind(i32 sockfd, const SockAddr* addr, u32 addrlen);
    static i32 listen(i32 sockfd, i32 backlog);
    static i32 accept(i32 sockfd, SockAddr* addr, u32* addrlen);
    static i32 connect(i32 sockfd, const SockAddr* addr, u32 addrlen);
    static i64 send(i32 sockfd, const void* buf, usize len, i32 flags);
    static i64 recv(i32 sockfd, void* buf, usize len, i32 flags);
    static i64 sendto(i32 sockfd, const void* buf, usize len, i32 flags,
                      const SockAddr* destAddr, u32 addrlen);
    static i64 recvfrom(i32 sockfd, void* buf, usize len, i32 flags,
                        SockAddr* srcAddr, u32* addrlen);
    static i32 shutdown(i32 sockfd, i32 how);
    static i32 close(i32 sockfd);
    
    static i32 getsockopt(i32 sockfd, i32 level, i32 optname, void* optval, u32* optlen);
    static i32 setsockopt(i32 sockfd, i32 level, i32 optname, const void* optval, u32 optlen);
    
    static i32 getsockname(i32 sockfd, SockAddr* addr, u32* addrlen);
    static i32 getpeername(i32 sockfd, SockAddr* addr, u32* addrlen);
    static i32 socketpair(i32 domain, i32 type, i32 protocol, i32* sv);
    
    static Socket* getSocket(i32 sockfd);
    static i32 allocateSocket();
    static void freeSocket(i32 sockfd);
    
    static bool isInitialized();

private:
    static void sockAddrToIpPort(const SockAddr* addr, IPv4Address* ip, u16* port);
    static void ipPortToSockAddr(IPv4Address ip, u16 port, SockAddr* addr);
    
    static Socket sSockets[MAX_SOCKETS];
    static u32 sSocketCount;
    static bool sInitialized;
};

}
