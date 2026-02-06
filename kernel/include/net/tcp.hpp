#pragma once

#include "net.hpp"

namespace sertos::net {

constexpr u32 TCP_RX_BUFFER_SIZE = 65536;
constexpr u32 TCP_TX_BUFFER_SIZE = 65536;
constexpr u32 TCP_MSS = 1460;
constexpr u32 TCP_RETRANSMIT_TIMEOUT = 1000;
constexpr u32 TCP_MAX_RETRIES = 5;
constexpr u32 TCP_TIME_WAIT_TIMEOUT = 60000;
constexpr u32 TCP_KEEPALIVE_TIMEOUT = 7200000;

enum class TcpState : u8 {
    Closed = 0,
    Listen,
    SynSent,
    SynReceived,
    Established,
    FinWait1,
    FinWait2,
    CloseWait,
    Closing,
    LastAck,
    TimeWait
};

struct TcpSegment {
    u32 seqNum;
    u32 length;
    u8 flags;
    u64 timestamp;
    u8 retries;
    u8* data;
    TcpSegment* next;
};

struct TcpConnection {
    u32 id;
    TcpState state;
    
    IPv4Address localAddr;
    u16 localPort;
    IPv4Address remoteAddr;
    u16 remotePort;
    
    u32 sendUnack;
    u32 sendNext;
    u32 sendWindow;
    u32 sendWl1;
    u32 sendWl2;
    u32 iss;
    
    u32 recvNext;
    u32 recvWindow;
    u32 irs;
    
    u8 rxBuffer[TCP_RX_BUFFER_SIZE];
    u32 rxHead;
    u32 rxTail;
    u32 rxCount;
    
    u8 txBuffer[TCP_TX_BUFFER_SIZE];
    u32 txHead;
    u32 txTail;
    u32 txCount;
    
    TcpSegment* retransmitQueue;
    TcpSegment* outOfOrderQueue;
    
    u64 lastActivity;
    u64 rto;
    u64 srtt;
    u64 rttvar;
    
    u32 cwnd;
    u32 ssthresh;
    
    u32 socketId;
    bool active;
    bool acceptPending;
    
    TcpConnection* listenBacklog;
    TcpConnection* backlogNext;
    u32 backlogCount;
    u32 backlogMax;
};

class Tcp {
public:
    static void initialize();
    
    static TcpConnection* createConnection();
    static void destroyConnection(TcpConnection* conn);
    static TcpConnection* getConnection(u32 id);
    static TcpConnection* findConnection(IPv4Address localAddr, u16 localPort, 
                                         IPv4Address remoteAddr, u16 remotePort);
    static TcpConnection* findListeningConnection(IPv4Address localAddr, u16 localPort);
    
    static i64 bind(TcpConnection* conn, IPv4Address addr, u16 port);
    static i64 listen(TcpConnection* conn, u32 backlog);
    static TcpConnection* accept(TcpConnection* conn);
    static i64 connect(TcpConnection* conn, IPv4Address addr, u16 port);
    static i64 send(TcpConnection* conn, const void* data, usize length);
    static i64 recv(TcpConnection* conn, void* buffer, usize length);
    static i64 close(TcpConnection* conn);
    
    static void processSegment(NetworkInterface* iface, const IPv4Header* ip, 
                               const TcpHeader* tcp, const void* data, usize dataLen);
    
    static void timerTick();
    
    static bool isInitialized();

private:
    static void sendSegment(TcpConnection* conn, u8 flags, const void* data, usize length);
    static void sendRst(NetworkInterface* iface, const IPv4Header* ip, const TcpHeader* tcp);
    static void sendAck(TcpConnection* conn);
    
    static void handleSynSent(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleSynReceived(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleEstablished(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleFinWait1(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleFinWait2(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleCloseWait(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleClosing(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleLastAck(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleTimeWait(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen);
    static void handleListen(TcpConnection* conn, NetworkInterface* iface, const IPv4Header* ip,
                             const TcpHeader* tcp, const void* data, usize dataLen);
    
    static void processAck(TcpConnection* conn, u32 ackNum);
    static void processData(TcpConnection* conn, const void* data, usize length, u32 seqNum);
    static void retransmit(TcpConnection* conn);
    static void updateRtt(TcpConnection* conn, u64 rtt);
    
    static u32 generateIsn();
    static u16 allocatePort();
    
    static void rxBufferWrite(TcpConnection* conn, const void* data, usize length);
    static usize rxBufferRead(TcpConnection* conn, void* buffer, usize length);
    static usize rxBufferAvailable(TcpConnection* conn);
    
    static void txBufferWrite(TcpConnection* conn, const void* data, usize length);
    static usize txBufferRead(TcpConnection* conn, void* buffer, usize length);
    static usize txBufferAvailable(TcpConnection* conn);
    
    static TcpConnection sConnections[TCP_MAX_CONNECTIONS];
    static u32 sConnectionCount;
    static u16 sNextPort;
    static u32 sIsnCounter;
    static bool sInitialized;
};

}
