#include "../include/string.hpp"
#include "../include/stdlib.hpp"

namespace sertos::libc {

namespace {

char toLower(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

char* strtokState = nullptr;

}

usize strlen(const char* s) {
    usize len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

usize strnlen(const char* s, usize maxlen) {
    usize len = 0;
    while (len < maxlen && s[len]) {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, usize n) {
    usize i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

usize strlcpy(char* dest, const char* src, usize size) {
    usize srcLen = strlen(src);
    
    if (size > 0) {
        usize copyLen = (srcLen >= size) ? size - 1 : srcLen;
        memcpy(dest, src, copyLen);
        dest[copyLen] = '\0';
    }
    
    return srcLen;
}

i32 strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *reinterpret_cast<const u8*>(s1) - *reinterpret_cast<const u8*>(s2);
}

i32 strncmp(const char* s1, const char* s2, usize n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *reinterpret_cast<const u8*>(s1) - *reinterpret_cast<const u8*>(s2);
}

i32 strcasecmp(const char* s1, const char* s2) {
    while (*s1 && toLower(*s1) == toLower(*s2)) {
        s1++;
        s2++;
    }
    return static_cast<u8>(toLower(*s1)) - static_cast<u8>(toLower(*s2));
}

i32 strncasecmp(const char* s1, const char* s2, usize n) {
    while (n && *s1 && toLower(*s1) == toLower(*s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return static_cast<u8>(toLower(*s1)) - static_cast<u8>(toLower(*s2));
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) {
        d++;
    }
    while ((*d++ = *src++));
    return dest;
}

char* strncat(char* dest, const char* src, usize n) {
    char* d = dest;
    while (*d) {
        d++;
    }
    while (n-- && *src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}

usize strlcat(char* dest, const char* src, usize size) {
    usize destLen = strnlen(dest, size);
    usize srcLen = strlen(src);
    
    if (destLen >= size) {
        return size + srcLen;
    }
    
    usize copyLen = (srcLen >= size - destLen) ? size - destLen - 1 : srcLen;
    memcpy(dest + destLen, src, copyLen);
    dest[destLen + copyLen] = '\0';
    
    return destLen + srcLen;
}

char* strchr(const char* s, i32 c) {
    while (*s) {
        if (*s == static_cast<char>(c)) {
            return const_cast<char*>(s);
        }
        s++;
    }
    if (c == '\0') {
        return const_cast<char*>(s);
    }
    return nullptr;
}

char* strrchr(const char* s, i32 c) {
    const char* last = nullptr;
    while (*s) {
        if (*s == static_cast<char>(c)) {
            last = s;
        }
        s++;
    }
    if (c == '\0') {
        return const_cast<char*>(s);
    }
    return const_cast<char*>(last);
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) {
        return const_cast<char*>(haystack);
    }
    
    usize needleLen = strlen(needle);
    
    while (*haystack) {
        if (strncmp(haystack, needle, needleLen) == 0) {
            return const_cast<char*>(haystack);
        }
        haystack++;
    }
    
    return nullptr;
}

char* strtok(char* str, const char* delim) {
    return strtok_r(str, delim, &strtokState);
}

char* strtok_r(char* str, const char* delim, char** saveptr) {
    if (str) {
        *saveptr = str;
    }
    
    if (!*saveptr) {
        return nullptr;
    }
    
    char* start = *saveptr;
    while (*start && strchr(delim, *start)) {
        start++;
    }
    
    if (!*start) {
        *saveptr = nullptr;
        return nullptr;
    }
    
    char* end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }
    
    if (*end) {
        *end = '\0';
        *saveptr = end + 1;
    } else {
        *saveptr = nullptr;
    }
    
    return start;
}

usize strspn(const char* s, const char* accept) {
    usize count = 0;
    while (*s && strchr(accept, *s)) {
        count++;
        s++;
    }
    return count;
}

usize strcspn(const char* s, const char* reject) {
    usize count = 0;
    while (*s && !strchr(reject, *s)) {
        count++;
        s++;
    }
    return count;
}

char* strpbrk(const char* s, const char* accept) {
    while (*s) {
        if (strchr(accept, *s)) {
            return const_cast<char*>(s);
        }
        s++;
    }
    return nullptr;
}

char* strdup(const char* s) {
    usize len = strlen(s) + 1;
    char* dup = static_cast<char*>(malloc(len));
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

char* strndup(const char* s, usize n) {
    usize len = strnlen(s, n);
    char* dup = static_cast<char*>(malloc(len + 1));
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}

namespace {

const char* errorMessages[] = {
    "Success",
    "Operation not permitted",
    "No such file or directory",
    "No such process",
    "Interrupted system call",
    "I/O error",
    "No such device or address",
    "Argument list too long",
    "Exec format error",
    "Bad file number",
    "No child processes",
    "Try again",
    "Out of memory",
    "Permission denied",
    "Bad address",
    "Block device required",
    "Device or resource busy",
    "File exists",
    "Cross-device link",
    "No such device",
    "Not a directory",
    "Is a directory",
    "Invalid argument",
    "File table overflow",
    "Too many open files",
    "Not a typewriter",
    "Text file busy",
    "File too large",
    "No space left on device",
    "Illegal seek",
    "Read-only file system",
    "Too many links",
    "Broken pipe",
    "Math argument out of domain",
    "Math result not representable",
    "Resource deadlock would occur",
    "File name too long",
    "No record locks available",
    "Function not implemented",
    "Directory not empty",
    "Too many symbolic links encountered",
    "Unknown error"
};

constexpr i32 MAX_ERRNO = 40;

}

char* strerror(i32 errnum) {
    if (errnum < 0 || errnum > MAX_ERRNO) {
        return const_cast<char*>(errorMessages[MAX_ERRNO + 1]);
    }
    return const_cast<char*>(errorMessages[errnum]);
}

i32 strerror_r(i32 errnum, char* buf, usize buflen) {
    const char* msg = strerror(errnum);
    usize len = strlen(msg);
    
    if (len >= buflen) {
        if (buflen > 0) {
            memcpy(buf, msg, buflen - 1);
            buf[buflen - 1] = '\0';
        }
        return 34;
    }
    
    memcpy(buf, msg, len + 1);
    return 0;
}

void* memcpy(void* dest, const void* src, usize n) {
    auto* d = static_cast<u8*>(dest);
    const auto* s = static_cast<const u8*>(src);
    
    if (n >= 8 && (reinterpret_cast<usize>(d) & 7) == 0 && 
        (reinterpret_cast<usize>(s) & 7) == 0) {
        auto* d64 = reinterpret_cast<u64*>(d);
        const auto* s64 = reinterpret_cast<const u64*>(s);
        
        while (n >= 8) {
            *d64++ = *s64++;
            n -= 8;
        }
        
        d = reinterpret_cast<u8*>(d64);
        s = reinterpret_cast<const u8*>(s64);
    }
    
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memmove(void* dest, const void* src, usize n) {
    auto* d = static_cast<u8*>(dest);
    const auto* s = static_cast<const u8*>(src);
    
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

void* memset(void* s, i32 c, usize n) {
    auto* p = static_cast<u8*>(s);
    u8 val = static_cast<u8>(c);
    
    if (n >= 8 && (reinterpret_cast<usize>(p) & 7) == 0) {
        u64 val64 = val;
        val64 |= val64 << 8;
        val64 |= val64 << 16;
        val64 |= val64 << 32;
        
        auto* p64 = reinterpret_cast<u64*>(p);
        while (n >= 8) {
            *p64++ = val64;
            n -= 8;
        }
        p = reinterpret_cast<u8*>(p64);
    }
    
    while (n--) {
        *p++ = val;
    }
    return s;
}

i32 memcmp(const void* s1, const void* s2, usize n) {
    const auto* p1 = static_cast<const u8*>(s1);
    const auto* p2 = static_cast<const u8*>(s2);
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

void* memchr(const void* s, i32 c, usize n) {
    const auto* p = static_cast<const u8*>(s);
    u8 val = static_cast<u8>(c);
    
    while (n--) {
        if (*p == val) {
            return const_cast<void*>(static_cast<const void*>(p));
        }
        p++;
    }
    return nullptr;
}

void* memrchr(const void* s, i32 c, usize n) {
    const auto* p = static_cast<const u8*>(s) + n;
    u8 val = static_cast<u8>(c);
    
    while (n--) {
        p--;
        if (*p == val) {
            return const_cast<void*>(static_cast<const void*>(p));
        }
    }
    return nullptr;
}

void* memmem(const void* haystack, usize haystacklen, const void* needle, usize needlelen) {
    if (needlelen == 0) {
        return const_cast<void*>(haystack);
    }
    
    if (haystacklen < needlelen) {
        return nullptr;
    }
    
    const auto* h = static_cast<const u8*>(haystack);
    const auto* n = static_cast<const u8*>(needle);
    
    usize limit = haystacklen - needlelen + 1;
    for (usize i = 0; i < limit; i++) {
        if (memcmp(h + i, n, needlelen) == 0) {
            return const_cast<void*>(static_cast<const void*>(h + i));
        }
    }
    
    return nullptr;
}

void bzero(void* s, usize n) {
    memset(s, 0, n);
}

void bcopy(const void* src, void* dest, usize n) {
    memmove(dest, src, n);
}

i32 bcmp(const void* s1, const void* s2, usize n) {
    return memcmp(s1, s2, n);
}

}
