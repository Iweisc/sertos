#include "../../include/net/tcp.hpp"
#include "../../include/net/net.hpp"
#include "../../include/process/scheduler.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::net {

TcpConnection Tcp::sConnections[TCP_MAX_CONNECTIONS];
u32 Tcp::sConnectionCount = 0;
u16 Tcp::sNextPort = 49152;
u32 Tcp::sIsnCounter = 0;
bool Tcp::sInitialized = false;

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

void Tcp::initialize() {
    if (sInitialized) {
        return;
    }
    
    for (u32 i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        sConnections[i].id = 0;
        sConnections[i].state = TcpState::Closed;
        sConnections[i].active = false;
    }
    
    sConnectionCount = 0;
    sNextPort = 49152;
    sIsnCounter = 0;
    sInitialized = true;
}

TcpConnection* Tcp::createConnection() {
    if (!sInitialized || sConnectionCount >= TCP_MAX_CONNECTIONS) {
        return nullptr;
    }
    
    for (u32 i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (!sConnections[i].active) {
            TcpConnection* conn = &sConnections[i];
            
            memset(conn, 0, sizeof(TcpConnection));
            conn->id = i + 1;
            conn->state = TcpState::Closed;
            conn->active = true;
            conn->sendWindow = TCP_WINDOW_SIZE;
            conn->recvWindow = TCP_WINDOW_SIZE;
            conn->rto = TCP_RETRANSMIT_TIMEOUT;
            conn->cwnd = TCP_MSS;
            conn->ssthresh = TCP_WINDOW_SIZE;
            
            sConnectionCount++;
            return conn;
        }
    }
    
    return nullptr;
}

void Tcp::destroyConnection(TcpConnection* conn) {
    if (!conn || !conn->active) {
        return;
    }
    
    TcpSegment* seg = conn->retransmitQueue;
    while (seg) {
        TcpSegment* next = seg->next;
        seg = next;
    }
    
    seg = conn->outOfOrderQueue;
    while (seg) {
        TcpSegment* next = seg->next;
        seg = next;
    }
    
    conn->active = false;
    conn->state = TcpState::Closed;
    sConnectionCount--;
}

TcpConnection* Tcp::getConnection(u32 id) {
    if (id == 0 || id > TCP_MAX_CONNECTIONS) {
        return nullptr;
    }
    
    TcpConnection* conn = &sConnections[id - 1];
    if (conn->active && conn->id == id) {
        return conn;
    }
    
    return nullptr;
}

TcpConnection* Tcp::findConnection(IPv4Address localAddr, u16 localPort, 
                                   IPv4Address remoteAddr, u16 remotePort) {
    for (u32 i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        TcpConnection* conn = &sConnections[i];
        if (conn->active && conn->state != TcpState::Closed && conn->state != TcpState::Listen) {
            if (conn->localPort == localPort && conn->remotePort == remotePort) {
                if ((conn->localAddr.isZero() || conn->localAddr == localAddr) &&
                    conn->remoteAddr == remoteAddr) {
                    return conn;
                }
            }
        }
    }
    return nullptr;
}

TcpConnection* Tcp::findListeningConnection(IPv4Address localAddr, u16 localPort) {
    for (u32 i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        TcpConnection* conn = &sConnections[i];
        if (conn->active && conn->state == TcpState::Listen) {
            if (conn->localPort == localPort) {
                if (conn->localAddr.isZero() || conn->localAddr == localAddr) {
                    return conn;
                }
            }
        }
    }
    return nullptr;
}

i64 Tcp::bind(TcpConnection* conn, IPv4Address addr, u16 port) {
    if (!conn || conn->state != TcpState::Closed) {
        return -1;
    }
    
    for (u32 i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (sConnections[i].active && sConnections[i].localPort == port) {
            if (sConnections[i].localAddr.isZero() || addr.isZero() ||
                sConnections[i].localAddr == addr) {
                return -98;
            }
        }
    }
    
    conn->localAddr = addr;
    conn->localPort = port;
    
    return 0;
}

i64 Tcp::listen(TcpConnection* conn, u32 backlog) {
    if (!conn || conn->state != TcpState::Closed) {
        return -1;
    }
    
    if (conn->localPort == 0) {
        return -1;
    }
    
    conn->state = TcpState::Listen;
    conn->backlogMax = backlog > 0 ? backlog : 5;
    conn->backlogCount = 0;
    conn->listenBacklog = nullptr;
    
    return 0;
}

TcpConnection* Tcp::accept(TcpConnection* conn) {
    if (!conn || conn->state != TcpState::Listen) {
        return nullptr;
    }
    
    if (!conn->listenBacklog) {
        return nullptr;
    }
    
    TcpConnection* newConn = conn->listenBacklog;
    conn->listenBacklog = newConn->backlogNext;
    conn->backlogCount--;
    
    newConn->backlogNext = nullptr;
    newConn->acceptPending = false;
    
    return newConn;
}

i64 Tcp::connect(TcpConnection* conn, IPv4Address addr, u16 port) {
    if (!conn || conn->state != TcpState::Closed) {
        return -1;
    }
    
    NetworkInterface* iface = NetworkStack::getDefaultInterface();
    if (!iface) {
        return -101;
    }
    
    if (conn->localPort == 0) {
        conn->localPort = allocatePort();
    }
    
    if (conn->localAddr.isZero()) {
        conn->localAddr = iface->ip;
    }
    
    conn->remoteAddr = addr;
    conn->remotePort = port;
    
    conn->iss = generateIsn();
    conn->sendUnack = conn->iss;
    conn->sendNext = conn->iss;

    conn->state = TcpState::SynSent;

    sendSegment(conn, TCP_FLAG_SYN, nullptr, 0);
    conn->sendNext = conn->iss + 1;

    return 0;
}

i64 Tcp::send(TcpConnection* conn, const void* data, usize length) {
    if (!conn || conn->state != TcpState::Established) {
        return -1;
    }
    
    if (length == 0) {
        return 0;
    }
    
    usize available = TCP_TX_BUFFER_SIZE - conn->txCount;
    if (available == 0) {
        return -11;
    }
    
    usize toSend = length < available ? length : available;
    txBufferWrite(conn, data, toSend);
    
    while (conn->txCount > 0 && conn->sendNext - conn->sendUnack < conn->sendWindow) {
        usize segmentSize = conn->txCount < TCP_MSS ? conn->txCount : TCP_MSS;
        u8 segmentData[TCP_MSS];
        txBufferRead(conn, segmentData, segmentSize);
        
        sendSegment(conn, TCP_FLAG_ACK | TCP_FLAG_PSH, segmentData, segmentSize);
        conn->sendNext += segmentSize;
    }
    
    return toSend;
}

i64 Tcp::recv(TcpConnection* conn, void* buffer, usize length) {
    if (!conn) {
        return -1;
    }
    
    if (conn->state != TcpState::Established && 
        conn->state != TcpState::FinWait1 &&
        conn->state != TcpState::FinWait2 &&
        conn->state != TcpState::CloseWait) {
        return -1;
    }
    
    if (conn->rxCount == 0) {
        if (conn->state == TcpState::CloseWait) {
            return 0;
        }
        return -11;
    }
    
    usize toRead = length < conn->rxCount ? length : conn->rxCount;
    rxBufferRead(conn, buffer, toRead);
    
    return toRead;
}

i64 Tcp::close(TcpConnection* conn) {
    if (!conn) {
        return -1;
    }
    
    switch (conn->state) {
        case TcpState::Closed:
            return 0;
            
        case TcpState::Listen:
        case TcpState::SynSent:
            conn->state = TcpState::Closed;
            return 0;
            
        case TcpState::SynReceived:
        case TcpState::Established:
            sendSegment(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, nullptr, 0);
            conn->sendNext++;
            conn->state = TcpState::FinWait1;
            break;
            
        case TcpState::CloseWait:
            sendSegment(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, nullptr, 0);
            conn->sendNext++;
            conn->state = TcpState::LastAck;
            break;
            
        default:
            break;
    }
    
    return 0;
}

void Tcp::processSegment(NetworkInterface* iface, const IPv4Header* ip, 
                         const TcpHeader* tcp, const void* data, usize dataLen) {
    u16 srcPort = ntohs(tcp->srcPort);
    u16 dstPort = ntohs(tcp->dstPort);
    
    TcpConnection* conn = findConnection(ip->dstAddr, dstPort, ip->srcAddr, srcPort);
    
    if (!conn) {
        conn = findListeningConnection(ip->dstAddr, dstPort);
        if (conn) {
            handleListen(conn, iface, ip, tcp, data, dataLen);
            return;
        }
        
        if (!(tcp->flags & TCP_FLAG_RST)) {
            sendRst(iface, ip, tcp);
        }
        return;
    }
    
    conn->lastActivity = process::Scheduler::systemTime();
    
    switch (conn->state) {
        case TcpState::SynSent:
            handleSynSent(conn, tcp, data, dataLen);
            break;
        case TcpState::SynReceived:
            handleSynReceived(conn, tcp, data, dataLen);
            break;
        case TcpState::Established:
            handleEstablished(conn, tcp, data, dataLen);
            break;
        case TcpState::FinWait1:
            handleFinWait1(conn, tcp, data, dataLen);
            break;
        case TcpState::FinWait2:
            handleFinWait2(conn, tcp, data, dataLen);
            break;
        case TcpState::CloseWait:
            handleCloseWait(conn, tcp, data, dataLen);
            break;
        case TcpState::Closing:
            handleClosing(conn, tcp, data, dataLen);
            break;
        case TcpState::LastAck:
            handleLastAck(conn, tcp, data, dataLen);
            break;
        case TcpState::TimeWait:
            handleTimeWait(conn, tcp, data, dataLen);
            break;
        default:
            break;
    }
}

void Tcp::timerTick() {
    u64 now = process::Scheduler::systemTime();
    
    for (u32 i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        TcpConnection* conn = &sConnections[i];
        if (!conn->active) {
            continue;
        }
        
        if (conn->state == TcpState::TimeWait) {
            if (now - conn->lastActivity >= TCP_TIME_WAIT_TIMEOUT) {
                conn->state = TcpState::Closed;
            }
        }
        
        if (conn->retransmitQueue) {
            TcpSegment* seg = conn->retransmitQueue;
            while (seg) {
                if (now - seg->timestamp >= conn->rto) {
                    if (seg->retries >= TCP_MAX_RETRIES) {
                        conn->state = TcpState::Closed;
                        break;
                    }
                    retransmit(conn);
                    break;
                }
                seg = seg->next;
            }
        }
    }
}

bool Tcp::isInitialized() {
    return sInitialized;
}

void Tcp::sendSegment(TcpConnection* conn, u8 flags, const void* data, usize length) {
    NetworkInterface* iface = NetworkStack::getDefaultInterface();
    if (!iface) {
        return;
    }
    
    u8 packet[MAX_PACKET_SIZE];
    
    EthernetHeader* eth = reinterpret_cast<EthernetHeader*>(packet);
    IPv4Header* ip = reinterpret_cast<IPv4Header*>(packet + ETHERNET_HEADER_SIZE);
    TcpHeader* tcp = reinterpret_cast<TcpHeader*>(packet + ETHERNET_HEADER_SIZE + IP_HEADER_SIZE);
    u8* payload = packet + ETHERNET_HEADER_SIZE + IP_HEADER_SIZE + TCP_HEADER_SIZE;
    
    memset(eth->dest.bytes, 0xFF, 6);
    eth->src = iface->mac;
    eth->ethertype = htons(ETHERTYPE_IPV4);
    
    ip->versionIhl = 0x45;
    ip->tos = 0;
    ip->totalLength = htons(IP_HEADER_SIZE + TCP_HEADER_SIZE + length);
    ip->identification = 0;
    ip->flagsFragment = htons(0x4000);
    ip->ttl = 64;
    ip->protocol = IP_PROTO_TCP;
    ip->checksum = 0;
    ip->srcAddr = conn->localAddr;
    ip->dstAddr = conn->remoteAddr;
    ip->checksum = calculateChecksum(ip, IP_HEADER_SIZE);
    
    tcp->srcPort = htons(conn->localPort);
    tcp->dstPort = htons(conn->remotePort);
    tcp->seqNum = htonl(conn->sendNext);
    tcp->ackNum = htonl(conn->recvNext);
    tcp->dataOffset = (TCP_HEADER_SIZE / 4) << 4;
    tcp->flags = flags;
    tcp->window = htons(conn->recvWindow);
    tcp->checksum = 0;
    tcp->urgentPtr = 0;
    
    if (data && length > 0) {
        memcpy(payload, data, length);
    }
    
    tcp->checksum = calculateTcpChecksum(ip, tcp, data, length);
    
    usize packetLen = ETHERNET_HEADER_SIZE + IP_HEADER_SIZE + TCP_HEADER_SIZE + length;
    NetworkStack::sendPacket(iface->id, packet, packetLen);
}

void Tcp::sendRst(NetworkInterface* iface, const IPv4Header* ip, const TcpHeader* tcp) {
    u8 packet[ETHERNET_HEADER_SIZE + IP_HEADER_SIZE + TCP_HEADER_SIZE];
    
    EthernetHeader* ethReply = reinterpret_cast<EthernetHeader*>(packet);
    IPv4Header* ipReply = reinterpret_cast<IPv4Header*>(packet + ETHERNET_HEADER_SIZE);
    TcpHeader* tcpReply = reinterpret_cast<TcpHeader*>(packet + ETHERNET_HEADER_SIZE + IP_HEADER_SIZE);
    
    memset(ethReply->dest.bytes, 0xFF, 6);
    ethReply->src = iface->mac;
    ethReply->ethertype = htons(ETHERTYPE_IPV4);
    
    ipReply->versionIhl = 0x45;
    ipReply->tos = 0;
    ipReply->totalLength = htons(IP_HEADER_SIZE + TCP_HEADER_SIZE);
    ipReply->identification = 0;
    ipReply->flagsFragment = htons(0x4000);
    ipReply->ttl = 64;
    ipReply->protocol = IP_PROTO_TCP;
    ipReply->checksum = 0;
    ipReply->srcAddr = ip->dstAddr;
    ipReply->dstAddr = ip->srcAddr;
    ipReply->checksum = calculateChecksum(ipReply, IP_HEADER_SIZE);
    
    tcpReply->srcPort = tcp->dstPort;
    tcpReply->dstPort = tcp->srcPort;
    
    if (tcp->flags & TCP_FLAG_ACK) {
        tcpReply->seqNum = tcp->ackNum;
        tcpReply->ackNum = 0;
        tcpReply->flags = TCP_FLAG_RST;
    } else {
        tcpReply->seqNum = 0;
        tcpReply->ackNum = htonl(ntohl(tcp->seqNum) + 1);
        tcpReply->flags = TCP_FLAG_RST | TCP_FLAG_ACK;
    }
    
    tcpReply->dataOffset = (TCP_HEADER_SIZE / 4) << 4;
    tcpReply->window = 0;
    tcpReply->checksum = 0;
    tcpReply->urgentPtr = 0;
    
    tcpReply->checksum = calculateTcpChecksum(ipReply, tcpReply, nullptr, 0);
    
    NetworkStack::sendPacket(iface->id, packet, sizeof(packet));
}

void Tcp::sendAck(TcpConnection* conn) {
    sendSegment(conn, TCP_FLAG_ACK, nullptr, 0);
}

void Tcp::handleSynSent(TcpConnection* conn, const TcpHeader* tcp, const void*, usize) {
    cpu::serialPuts("[TCP] handleSynSent flags=0x");
    { u8 f = tcp->flags; const char h[] = "0123456789ABCDEF";
      cpu::serialPutc(h[(f>>4)&0xF]); cpu::serialPutc(h[f&0xF]); }
    cpu::serialPutc('\n');

    if (tcp->flags & TCP_FLAG_RST) {
        conn->state = TcpState::Closed;
        return;
    }

    if ((tcp->flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
        u32 ackNum = ntohl(tcp->ackNum);
        if (ackNum != conn->sendNext) {
            cpu::serialPuts("[TCP] SYN-ACK ack mismatch\n");
            return;
        }

        conn->irs = ntohl(tcp->seqNum);
        conn->recvNext = conn->irs + 1;
        conn->sendUnack = ackNum;
        conn->sendWindow = ntohs(tcp->window);

        conn->state = TcpState::Established;
        cpu::serialPuts("[TCP] ESTABLISHED\n");
        sendAck(conn);
    }
}

void Tcp::handleSynReceived(TcpConnection* conn, const TcpHeader* tcp, const void*, usize) {
    if (tcp->flags & TCP_FLAG_RST) {
        conn->state = TcpState::Listen;
        return;
    }
    
    if (tcp->flags & TCP_FLAG_ACK) {
        u32 ackNum = ntohl(tcp->ackNum);
        if (ackNum == conn->sendNext) {
            conn->sendUnack = ackNum;
            conn->state = TcpState::Established;
        }
    }
}

void Tcp::handleEstablished(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen) {
    if (tcp->flags & TCP_FLAG_RST) {
        conn->state = TcpState::Closed;
        return;
    }
    
    u32 seqNum = ntohl(tcp->seqNum);
    u32 ackNum = ntohl(tcp->ackNum);
    
    if (tcp->flags & TCP_FLAG_ACK) {
        processAck(conn, ackNum);
    }
    
    if (dataLen > 0) {
        processData(conn, data, dataLen, seqNum);
        sendAck(conn);
    }
    
    if (tcp->flags & TCP_FLAG_FIN) {
        conn->recvNext++;
        conn->state = TcpState::CloseWait;
        sendAck(conn);
    }
}

void Tcp::handleFinWait1(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen) {
    if (tcp->flags & TCP_FLAG_RST) {
        conn->state = TcpState::Closed;
        return;
    }
    
    u32 ackNum = ntohl(tcp->ackNum);
    
    if (tcp->flags & TCP_FLAG_ACK) {
        processAck(conn, ackNum);
        if (ackNum == conn->sendNext) {
            conn->state = TcpState::FinWait2;
        }
    }
    
    if (dataLen > 0) {
        processData(conn, data, dataLen, ntohl(tcp->seqNum));
        sendAck(conn);
    }
    
    if (tcp->flags & TCP_FLAG_FIN) {
        conn->recvNext++;
        if (conn->state == TcpState::FinWait2) {
            conn->state = TcpState::TimeWait;
        } else {
            conn->state = TcpState::Closing;
        }
        sendAck(conn);
    }
}

void Tcp::handleFinWait2(TcpConnection* conn, const TcpHeader* tcp, const void* data, usize dataLen) {
    if (tcp->flags & TCP_FLAG_RST) {
        conn->state = TcpState::Closed;
        return;
    }
    
    if (dataLen > 0) {
        processData(conn, data, dataLen, ntohl(tcp->seqNum));
        sendAck(conn);
    }
    
    if (tcp->flags & TCP_FLAG_FIN) {
        conn->recvNext++;
        conn->state = TcpState::TimeWait;
        sendAck(conn);
    }
}

void Tcp::handleCloseWait(TcpConnection* conn, const TcpHeader* tcp, const void*, usize) {
    if (tcp->flags & TCP_FLAG_RST) {
        conn->state = TcpState::Closed;
        return;
    }
    
    if (tcp->flags & TCP_FLAG_ACK) {
        processAck(conn, ntohl(tcp->ackNum));
    }
}

void Tcp::handleClosing(TcpConnection* conn, const TcpHeader* tcp, const void*, usize) {
    if (tcp->flags & TCP_FLAG_RST) {
        conn->state = TcpState::Closed;
        return;
    }
    
    if (tcp->flags & TCP_FLAG_ACK) {
        u32 ackNum = ntohl(tcp->ackNum);
        if (ackNum == conn->sendNext) {
            conn->state = TcpState::TimeWait;
        }
    }
}

void Tcp::handleLastAck(TcpConnection* conn, const TcpHeader* tcp, const void*, usize) {
    if (tcp->flags & TCP_FLAG_RST) {
        conn->state = TcpState::Closed;
        return;
    }
    
    if (tcp->flags & TCP_FLAG_ACK) {
        u32 ackNum = ntohl(tcp->ackNum);
        if (ackNum == conn->sendNext) {
            conn->state = TcpState::Closed;
        }
    }
}

void Tcp::handleTimeWait(TcpConnection* conn, const TcpHeader* tcp, const void*, usize) {
    if (tcp->flags & TCP_FLAG_FIN) {
        sendAck(conn);
    }
}

void Tcp::handleListen(TcpConnection* conn, NetworkInterface* iface, const IPv4Header* ip,
                       const TcpHeader* tcp, const void*, usize) {
    if (tcp->flags & TCP_FLAG_RST) {
        return;
    }
    
    if (tcp->flags & TCP_FLAG_ACK) {
        sendRst(iface, ip, tcp);
        return;
    }
    
    if (!(tcp->flags & TCP_FLAG_SYN)) {
        return;
    }
    
    if (conn->backlogCount >= conn->backlogMax) {
        return;
    }
    
    TcpConnection* newConn = createConnection();
    if (!newConn) {
        return;
    }
    
    newConn->localAddr = ip->dstAddr;
    newConn->localPort = ntohs(tcp->dstPort);
    newConn->remoteAddr = ip->srcAddr;
    newConn->remotePort = ntohs(tcp->srcPort);
    
    newConn->irs = ntohl(tcp->seqNum);
    newConn->recvNext = newConn->irs + 1;
    newConn->iss = generateIsn();
    newConn->sendUnack = newConn->iss;
    newConn->sendNext = newConn->iss;
    newConn->sendWindow = ntohs(tcp->window);

    newConn->state = TcpState::SynReceived;
    newConn->acceptPending = true;

    newConn->backlogNext = conn->listenBacklog;
    conn->listenBacklog = newConn;
    conn->backlogCount++;

    sendSegment(newConn, TCP_FLAG_SYN | TCP_FLAG_ACK, nullptr, 0);
    newConn->sendNext = newConn->iss + 1;
}

void Tcp::processAck(TcpConnection* conn, u32 ackNum) {
    if (ackNum > conn->sendUnack && ackNum <= conn->sendNext) {
        conn->sendUnack = ackNum;
        
        TcpSegment* seg = conn->retransmitQueue;
        TcpSegment* prev = nullptr;
        
        while (seg) {
            if (seg->seqNum + seg->length <= ackNum) {
                TcpSegment* next = seg->next;
                if (prev) {
                    prev->next = next;
                } else {
                    conn->retransmitQueue = next;
                }
                seg = next;
            } else {
                prev = seg;
                seg = seg->next;
            }
        }
    }
}

void Tcp::processData(TcpConnection* conn, const void* data, usize length, u32 seqNum) {
    if (seqNum == conn->recvNext) {
        rxBufferWrite(conn, data, length);
        conn->recvNext += length;
    }
}

void Tcp::retransmit(TcpConnection* conn) {
    TcpSegment* seg = conn->retransmitQueue;
    if (!seg) {
        return;
    }
    
    seg->retries++;
    seg->timestamp = process::Scheduler::systemTime();
    
    conn->ssthresh = conn->cwnd / 2;
    if (conn->ssthresh < 2 * TCP_MSS) {
        conn->ssthresh = 2 * TCP_MSS;
    }
    conn->cwnd = TCP_MSS;
    
    sendSegment(conn, seg->flags, seg->data, seg->length);
}

void Tcp::updateRtt(TcpConnection* conn, u64 rtt) {
    if (conn->srtt == 0) {
        conn->srtt = rtt;
        conn->rttvar = rtt / 2;
    } else {
        i64 delta = static_cast<i64>(rtt) - static_cast<i64>(conn->srtt);
        if (delta < 0) delta = -delta;
        conn->rttvar = (3 * conn->rttvar + delta) / 4;
        conn->srtt = (7 * conn->srtt + rtt) / 8;
    }
    
    conn->rto = conn->srtt + 4 * conn->rttvar;
    if (conn->rto < 200) conn->rto = 200;
    if (conn->rto > 60000) conn->rto = 60000;
}

u32 Tcp::generateIsn() {
    sIsnCounter += 64000;
    return sIsnCounter + (process::Scheduler::systemTime() & 0xFFFF);
}

u16 Tcp::allocatePort() {
    u16 port = sNextPort;
    sNextPort++;
    if (sNextPort >= 65535) {
        sNextPort = 49152;
    }
    return port;
}

void Tcp::rxBufferWrite(TcpConnection* conn, const void* data, usize length) {
    const u8* src = reinterpret_cast<const u8*>(data);
    
    for (usize i = 0; i < length && conn->rxCount < TCP_RX_BUFFER_SIZE; i++) {
        conn->rxBuffer[conn->rxTail] = src[i];
        conn->rxTail = (conn->rxTail + 1) % TCP_RX_BUFFER_SIZE;
        conn->rxCount++;
    }
}

usize Tcp::rxBufferRead(TcpConnection* conn, void* buffer, usize length) {
    u8* dst = reinterpret_cast<u8*>(buffer);
    usize read = 0;
    
    while (read < length && conn->rxCount > 0) {
        dst[read] = conn->rxBuffer[conn->rxHead];
        conn->rxHead = (conn->rxHead + 1) % TCP_RX_BUFFER_SIZE;
        conn->rxCount--;
        read++;
    }
    
    return read;
}

usize Tcp::rxBufferAvailable(TcpConnection* conn) {
    return conn->rxCount;
}

void Tcp::txBufferWrite(TcpConnection* conn, const void* data, usize length) {
    const u8* src = reinterpret_cast<const u8*>(data);
    
    for (usize i = 0; i < length && conn->txCount < TCP_TX_BUFFER_SIZE; i++) {
        conn->txBuffer[conn->txTail] = src[i];
        conn->txTail = (conn->txTail + 1) % TCP_TX_BUFFER_SIZE;
        conn->txCount++;
    }
}

usize Tcp::txBufferRead(TcpConnection* conn, void* buffer, usize length) {
    u8* dst = reinterpret_cast<u8*>(buffer);
    usize read = 0;
    
    while (read < length && conn->txCount > 0) {
        dst[read] = conn->txBuffer[conn->txHead];
        conn->txHead = (conn->txHead + 1) % TCP_TX_BUFFER_SIZE;
        conn->txCount--;
        read++;
    }
    
    return read;
}

usize Tcp::txBufferAvailable(TcpConnection* conn) {
    return TCP_TX_BUFFER_SIZE - conn->txCount;
}

}
