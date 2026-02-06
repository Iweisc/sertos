#pragma once

#include "net.hpp"
#include "dns.hpp"
#include "socket.hpp"

namespace sertos::net {

constexpr u32 HTTP_MAX_URL_LENGTH = 512;
constexpr u32 HTTP_MAX_HEADER_SIZE = 4096;
constexpr u32 HTTP_MAX_BODY_SIZE = 65536;

struct HttpUrl {
    char host[256];
    char path[256];
    u16  port;
};

struct HttpResponse {
    u32  statusCode;
    char body[HTTP_MAX_BODY_SIZE];
    u32  bodyLength;
    char contentType[64];
    char location[HTTP_MAX_URL_LENGTH];
    char errorMessage[128];
    bool chunked;
    bool success;
};

class HttpClient {
public:
    static void initialize();
    static bool get(const char* url, HttpResponse* response);

private:
    static bool parseUrl(const char* url, HttpUrl* parsed);
    static bool sendRequest(i32 sockfd, const HttpUrl* url);
    static bool readResponse(i32 sockfd, HttpResponse* response);
    static bool parseHeaders(const char* data, u32 length, HttpResponse* response, u32* headerEnd);
    static u32  parseChunked(const char* data, u32 length, char* output, u32 maxOutput);
    static i64  pollRecv(i32 sockfd, void* buffer, usize maxLen, u32 timeoutMs);
    static void pumpNetwork();

    static bool sInitialized;
};

}
