#include "../include/string.hpp"

namespace sertos::libc {

usize strlen(const char* s) {
    usize len = 0;
    while (s[len]) {
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

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) {
        d++;
    }
    while ((*d++ = *src++));
    return dest;
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

void* memcpy(void* dest, const void* src, usize n) {
    auto* d = static_cast<u8*>(dest);
    const auto* s = static_cast<const u8*>(src);
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
    while (n--) {
        *p++ = static_cast<u8>(c);
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

}
