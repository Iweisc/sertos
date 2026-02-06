#include "../../include/net/http.hpp"
#include "../../include/net/dns.hpp"
#include "../../include/net/socket.hpp"
#include "../../include/net/tcp.hpp"
#include "../../include/drivers/virtio_net.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::net {

bool HttpClient::sInitialized = false;

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

static void sncpy(char* dst, const char* src, u32 n, u32 maxLen) {
    u32 i = 0;
    u32 limit = n < maxLen - 1 ? n : maxLen - 1;
    while (i < limit && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void scat(char* dst, const char* src, u32 maxLen) {
    u32 dstLen = slen(dst);
    u32 i = 0;
    while (src[i] && dstLen + i < maxLen - 1) {
        dst[dstLen + i] = src[i];
        i++;
    }
    dst[dstLen + i] = '\0';
}

static bool streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; a++; b++; }
    return *a == *b;
}

static bool strieq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

static bool startsWith(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return false;
        str++; prefix++;
    }
    return true;
}

static bool startsWithI(const char* str, const char* prefix) {
    while (*prefix) {
        char cs = *str, cp = *prefix;
        if (cs >= 'A' && cs <= 'Z') cs += 32;
        if (cp >= 'A' && cp <= 'Z') cp += 32;
        if (cs != cp) return false;
        str++; prefix++;
    }
    return true;
}

static u32 parseDecimal(const char* s) {
    u32 val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

static u32 parseHex(const char* s, u32 maxChars) {
    u32 val = 0;
    for (u32 i = 0; i < maxChars && s[i]; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val = val * 16 + (c - 'A' + 10);
        else break;
    }
    return val;
}

static void memset(void* dst, u8 val, usize n) {
    u8* d = reinterpret_cast<u8*>(dst);
    for (usize i = 0; i < n; i++) d[i] = val;
}

void HttpClient::initialize() {
    sInitialized = true;
}

void HttpClient::pumpNetwork() {
    drivers::VirtioNet::receivePackets(0);
    Tcp::timerTick();
}

bool HttpClient::parseUrl(const char* url, HttpUrl* parsed) {
    memset(parsed, 0, sizeof(HttpUrl));
    parsed->port = 80;

    const char* p = url;

    // Skip scheme
    if (startsWith(p, "http://")) {
        p += 7;
    } else if (startsWith(p, "https://")) {
        p += 8;
        parsed->port = 443; // Won't work without TLS, but parse correctly
    }

    // Find end of host (first / or end of string)
    u32 hostLen = 0;
    while (p[hostLen] && p[hostLen] != '/' && p[hostLen] != '?' && p[hostLen] != '#') {
        hostLen++;
    }

    if (hostLen == 0 || hostLen >= 255) return false;

    // Check for port
    u32 colonPos = hostLen;
    for (u32 i = 0; i < hostLen; i++) {
        if (p[i] == ':') { colonPos = i; break; }
    }

    if (colonPos < hostLen) {
        sncpy(parsed->host, p, colonPos, 256);
        parsed->port = static_cast<u16>(parseDecimal(p + colonPos + 1));
    } else {
        sncpy(parsed->host, p, hostLen, 256);
    }

    // Path
    p += hostLen;
    if (*p == '\0') {
        scpy(parsed->path, "/", 256);
    } else {
        scpy(parsed->path, p, 256);
    }

    return true;
}

bool HttpClient::sendRequest(i32 sockfd, const HttpUrl* url) {
    char request[1024];
    request[0] = '\0';

    scat(request, "GET ", 1024);
    scat(request, url->path, 1024);
    scat(request, " HTTP/1.1\r\nHost: ", 1024);
    scat(request, url->host, 1024);
    scat(request, "\r\nConnection: close\r\nUser-Agent: SertOS/1.0\r\nAccept: text/html, text/*\r\n\r\n", 1024);

    u32 reqLen = slen(request);
    usize sent = 0;
    while (sent < reqLen) {
        i64 n = SocketManager::send(sockfd, request + sent, reqLen - sent, 0);
        if (n > 0) {
            sent += static_cast<usize>(n);
        } else {
            pumpNetwork();
            for (volatile int d = 0; d < 10000; d++);
        }
    }

    return true;
}

i64 HttpClient::pollRecv(i32 sockfd, void* buffer, usize maxLen, u32 timeoutMs) {
    u32 waited = 0;
    while (waited < timeoutMs) {
        pumpNetwork();

        i64 n = SocketManager::recv(sockfd, buffer, maxLen, 0);
        if (n > 0) return n;
        if (n == 0) return 0; // Connection closed

        // -11 = EAGAIN, keep polling
        for (volatile int d = 0; d < 10000; d++);
        waited += 2;
    }
    return -1;
}

bool HttpClient::readResponse(i32 sockfd, HttpResponse* response) {
    memset(response, 0, sizeof(HttpResponse));

    // Read all data from connection
    static char recvBuf[HTTP_MAX_HEADER_SIZE + HTTP_MAX_BODY_SIZE];
    u32 totalReceived = 0;

    while (totalReceived < sizeof(recvBuf) - 1) {
        i64 n = pollRecv(sockfd, recvBuf + totalReceived,
                         sizeof(recvBuf) - 1 - totalReceived, 5000);
        if (n > 0) {
            totalReceived += static_cast<u32>(n);
            recvBuf[totalReceived] = '\0';

            // Check if we have complete headers
            bool headersComplete = false;
            for (u32 i = 0; i + 3 < totalReceived; i++) {
                if (recvBuf[i] == '\r' && recvBuf[i+1] == '\n' &&
                    recvBuf[i+2] == '\r' && recvBuf[i+3] == '\n') {
                    headersComplete = true;
                    break;
                }
            }

            if (!headersComplete) continue;

            // Parse headers
            u32 headerEnd = 0;
            parseHeaders(recvBuf, totalReceived, response, &headerEnd);

            if (response->chunked) {
                // For chunked, keep reading until we get the final 0-length chunk
                // or timeout
                bool foundEnd = false;
                u32 retries = 0;
                while (!foundEnd && retries < 500) {
                    // Check for "0\r\n" in the data after headers
                    const char* bodyStart = recvBuf + headerEnd;
                    u32 bodyLen = totalReceived - headerEnd;
                    // Simple check: look for "\r\n0\r\n"
                    for (u32 j = 0; j + 4 < bodyLen; j++) {
                        if (bodyStart[j] == '\r' && bodyStart[j+1] == '\n' &&
                            bodyStart[j+2] == '0' && bodyStart[j+3] == '\r' &&
                            bodyStart[j+4] == '\n') {
                            foundEnd = true;
                            break;
                        }
                    }
                    // Also check beginning for "0\r\n"
                    if (bodyLen >= 3 && bodyStart[0] == '0' && bodyStart[1] == '\r' && bodyStart[2] == '\n') {
                        foundEnd = true;
                    }
                    if (!foundEnd) {
                        i64 more = pollRecv(sockfd, recvBuf + totalReceived,
                                            sizeof(recvBuf) - 1 - totalReceived, 1000);
                        if (more > 0) {
                            totalReceived += static_cast<u32>(more);
                            recvBuf[totalReceived] = '\0';
                        } else if (more == 0) {
                            break; // Connection closed
                        }
                        retries++;
                    }
                }

                // Decode chunked body
                response->bodyLength = parseChunked(
                    recvBuf + headerEnd, totalReceived - headerEnd,
                    response->body, HTTP_MAX_BODY_SIZE);
                response->success = true;

                cpu::serialPuts("[HTTP] chunked totalRecv=");
                { u32 n = totalReceived; char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
                cpu::serialPuts(" bodyLen=");
                { u32 n = response->bodyLength; char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
                cpu::serialPutc('\n');
                cpu::serialPuts("[HTTP] body: ");
                for (u32 i = 0; i < response->bodyLength && i < 80; i++) {
                    char c = response->body[i];
                    if (c >= 32 && c < 127) cpu::serialPutc(c);
                    else cpu::serialPutc('.');
                }
                cpu::serialPutc('\n');

                return true;
            } else {
                // Content-Length or connection-close
                u32 contentLen = 0;

                // Find Content-Length in headers
                for (u32 i = 0; i + 15 < headerEnd; i++) {
                    if (startsWithI(recvBuf + i, "content-length:")) {
                        const char* val = recvBuf + i + 15;
                        while (*val == ' ') val++;
                        contentLen = parseDecimal(val);
                        break;
                    }
                }

                if (contentLen > 0) {
                    // Read until we have contentLen bytes of body
                    while (totalReceived - headerEnd < contentLen && totalReceived < sizeof(recvBuf) - 1) {
                        i64 more = pollRecv(sockfd, recvBuf + totalReceived,
                                            sizeof(recvBuf) - 1 - totalReceived, 3000);
                        if (more > 0) {
                            totalReceived += static_cast<u32>(more);
                        } else {
                            break;
                        }
                    }
                } else {
                    // Read until connection closes
                    while (totalReceived < sizeof(recvBuf) - 1) {
                        i64 more = pollRecv(sockfd, recvBuf + totalReceived,
                                            sizeof(recvBuf) - 1 - totalReceived, 2000);
                        if (more > 0) {
                            totalReceived += static_cast<u32>(more);
                        } else {
                            break;
                        }
                    }
                }

                // Copy body
                u32 bodyLen = totalReceived - headerEnd;
                if (bodyLen > HTTP_MAX_BODY_SIZE - 1) bodyLen = HTTP_MAX_BODY_SIZE - 1;
                for (u32 i = 0; i < bodyLen; i++) {
                    response->body[i] = recvBuf[headerEnd + i];
                }
                response->body[bodyLen] = '\0';
                response->bodyLength = bodyLen;
                response->success = true;

                // Debug: print HTTP response info
                cpu::serialPuts("[HTTP] totalRecv=");
                { u32 n = totalReceived; char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
                cpu::serialPuts(" hdrEnd=");
                { u32 n = headerEnd; char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
                cpu::serialPuts(" bodyLen=");
                { u32 n = bodyLen; char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
                cpu::serialPuts(" cLen=");
                { u32 n = contentLen; char b[12]; int i=0; if(n==0)b[i++]='0'; else{char t[12];int j=0;while(n>0){t[j++]='0'+(n%10);n/=10;}while(j>0)b[i++]=t[--j];}b[i]=0;cpu::serialPuts(b); }
                cpu::serialPuts(" chunked=");
                cpu::serialPutc(response->chunked ? '1' : '0');
                cpu::serialPutc('\n');
                // Print first 80 chars of body
                cpu::serialPuts("[HTTP] body: ");
                for (u32 i = 0; i < bodyLen && i < 80; i++) {
                    char c = response->body[i];
                    if (c >= 32 && c < 127) cpu::serialPutc(c);
                    else cpu::serialPutc('.');
                }
                cpu::serialPutc('\n');

                return true;
            }
        } else if (n == 0) {
            // Connection closed - parse what we have
            if (totalReceived > 0) {
                recvBuf[totalReceived] = '\0';
                u32 headerEnd = 0;
                parseHeaders(recvBuf, totalReceived, response, &headerEnd);
                u32 bodyLen = totalReceived - headerEnd;
                if (bodyLen > HTTP_MAX_BODY_SIZE - 1) bodyLen = HTTP_MAX_BODY_SIZE - 1;
                for (u32 i = 0; i < bodyLen; i++) {
                    response->body[i] = recvBuf[headerEnd + i];
                }
                response->body[bodyLen] = '\0';
                response->bodyLength = bodyLen;
                response->success = response->statusCode >= 200;
                return true;
            }
            return false;
        } else {
            return false; // Timeout
        }
    }

    return false;
}

bool HttpClient::parseHeaders(const char* data, u32 length, HttpResponse* response, u32* headerEnd) {
    // Find status code from "HTTP/1.x NNN"
    u32 pos = 0;
    // Skip "HTTP/x.x "
    while (pos < length && data[pos] != ' ') pos++;
    if (pos < length) pos++; // skip space
    response->statusCode = parseDecimal(data + pos);

    // Find header end
    *headerEnd = 0;
    for (u32 i = 0; i + 3 < length; i++) {
        if (data[i] == '\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n') {
            *headerEnd = i + 4;
            break;
        }
    }
    if (*headerEnd == 0) {
        *headerEnd = length;
        return false;
    }

    // Parse individual headers
    pos = 0;
    // Skip status line
    while (pos < *headerEnd && data[pos] != '\n') pos++;
    pos++; // skip \n

    while (pos < *headerEnd) {
        // Find end of this header line
        u32 lineEnd = pos;
        while (lineEnd < *headerEnd && data[lineEnd] != '\r' && data[lineEnd] != '\n') lineEnd++;

        u32 lineLen = lineEnd - pos;
        if (lineLen == 0) break;

        const char* line = data + pos;

        if (startsWithI(line, "content-type:")) {
            const char* val = line + 13;
            while (*val == ' ') val++;
            sncpy(response->contentType, val, lineEnd - static_cast<u32>(val - data), 64);
        } else if (startsWithI(line, "transfer-encoding:")) {
            const char* val = line + 18;
            while (*val == ' ') val++;
            if (startsWithI(val, "chunked")) {
                response->chunked = true;
            }
        } else if (startsWithI(line, "location:")) {
            const char* val = line + 9;
            while (*val == ' ') val++;
            sncpy(response->location, val, lineEnd - static_cast<u32>(val - data), HTTP_MAX_URL_LENGTH);
        }

        // Move to next line
        pos = lineEnd;
        if (pos < *headerEnd && data[pos] == '\r') pos++;
        if (pos < *headerEnd && data[pos] == '\n') pos++;
    }

    return true;
}

u32 HttpClient::parseChunked(const char* data, u32 length, char* output, u32 maxOutput) {
    u32 inPos = 0;
    u32 outPos = 0;

    while (inPos < length && outPos < maxOutput - 1) {
        // Parse chunk size (hex)
        u32 chunkSize = 0;
        while (inPos < length) {
            char c = data[inPos];
            if (c >= '0' && c <= '9') { chunkSize = chunkSize * 16 + (c - '0'); inPos++; }
            else if (c >= 'a' && c <= 'f') { chunkSize = chunkSize * 16 + (c - 'a' + 10); inPos++; }
            else if (c >= 'A' && c <= 'F') { chunkSize = chunkSize * 16 + (c - 'A' + 10); inPos++; }
            else break;
        }

        // Skip \r\n after chunk size
        if (inPos < length && data[inPos] == '\r') inPos++;
        if (inPos < length && data[inPos] == '\n') inPos++;

        if (chunkSize == 0) break; // End of chunks

        // Copy chunk data
        u32 toCopy = chunkSize;
        if (outPos + toCopy > maxOutput - 1) toCopy = maxOutput - 1 - outPos;
        for (u32 i = 0; i < toCopy && inPos < length; i++) {
            output[outPos++] = data[inPos++];
        }

        // Skip remaining chunk data if we couldn't copy all
        u32 remaining = chunkSize - toCopy;
        inPos += remaining;

        // Skip \r\n after chunk data
        if (inPos < length && data[inPos] == '\r') inPos++;
        if (inPos < length && data[inPos] == '\n') inPos++;
    }

    output[outPos] = '\0';
    return outPos;
}

bool HttpClient::get(const char* url, HttpResponse* response) {
    if (!url || !response) return false;
    memset(response, 0, sizeof(HttpResponse));

    // Reject HTTPS URLs
    if (startsWith(url, "https://")) {
        scpy(response->errorMessage, "HTTPS is not supported. This browser only supports HTTP.", 128);
        return false;
    }

    HttpUrl parsed;
    if (!parseUrl(url, &parsed)) {
        scpy(response->errorMessage, "Invalid URL.", 128);
        return false;
    }

    // Resolve hostname
    IPv4Address serverIp;
    if (!DnsResolver::resolve(parsed.host, &serverIp)) {
        scpy(response->errorMessage, "DNS lookup failed for: ", 128);
        scat(response->errorMessage, parsed.host, 128);
        return false;
    }

    // Create TCP socket
    i32 sockfd = SocketManager::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        scpy(response->errorMessage, "Failed to create socket.", 128);
        return false;
    }

    // Connect
    SockAddrIn addr;
    addr.family = AF_INET;
    addr.port = htons(parsed.port);
    addr.addr = serverIp.value;
    for (int i = 0; i < 8; i++) addr.zero[i] = 0;

    i32 result = SocketManager::connect(sockfd, reinterpret_cast<SockAddr*>(&addr), sizeof(SockAddrIn));

    // Pump network to complete TCP handshake
    if (result != 0) {
        // Connection initiation - poll until connected or timeout
        u32 waited = 0;
        Socket* sock = SocketManager::getSocket(sockfd);
        while (waited < 5000 && sock && sock->state == SocketState::Connecting) {
            pumpNetwork();
            for (volatile int d = 0; d < 10000; d++);
            waited += 2;
        }
        if (!sock || sock->state != SocketState::Connected) {
            SocketManager::close(sockfd);
            scpy(response->errorMessage, "Connection timed out.", 128);
            return false;
        }
    }

    // Wait for TCP connection to be fully established
    u32 waited = 0;
    Socket* sock = SocketManager::getSocket(sockfd);
    while (waited < 5000) {
        pumpNetwork();
        if (sock && sock->tcpConn &&
            sock->tcpConn->state == TcpState::Established) {
            break;
        }
        for (volatile int d = 0; d < 10000; d++);
        waited += 2;
    }

    if (!sock || !sock->tcpConn ||
        sock->tcpConn->state != TcpState::Established) {
        SocketManager::close(sockfd);
        scpy(response->errorMessage, "TCP handshake failed.", 128);
        return false;
    }

    // Send HTTP request
    if (!sendRequest(sockfd, &parsed)) {
        SocketManager::close(sockfd);
        scpy(response->errorMessage, "Failed to send request.", 128);
        return false;
    }

    // Read response
    bool success = readResponse(sockfd, response);

    SocketManager::close(sockfd);

    if (!success) {
        if (response->errorMessage[0] == '\0') {
            scpy(response->errorMessage, "No response from server.", 128);
        }
        return false;
    }

    // Handle redirects
    if ((response->statusCode == 301 || response->statusCode == 302 ||
         response->statusCode == 303 || response->statusCode == 307) &&
        response->location[0] != '\0') {

        // Check if redirect goes to HTTPS
        if (startsWith(response->location, "https://")) {
            scpy(response->errorMessage, "Site redirected to HTTPS which is not supported.", 128);
            response->success = false;
            return false;
        }

        static u32 redirectCount = 0;
        if (redirectCount < 3) {
            redirectCount++;
            // Handle relative URLs
            char fullUrl[HTTP_MAX_URL_LENGTH];
            if (startsWith(response->location, "http://")) {
                scpy(fullUrl, response->location, HTTP_MAX_URL_LENGTH);
            } else if (response->location[0] == '/') {
                fullUrl[0] = '\0';
                scat(fullUrl, "http://", HTTP_MAX_URL_LENGTH);
                scat(fullUrl, parsed.host, HTTP_MAX_URL_LENGTH);
                scat(fullUrl, response->location, HTTP_MAX_URL_LENGTH);
            } else {
                scpy(fullUrl, response->location, HTTP_MAX_URL_LENGTH);
            }
            success = get(fullUrl, response);
            redirectCount--;
        } else {
            scpy(response->errorMessage, "Too many redirects.", 128);
            return false;
        }
    }

    return success;
}

}
