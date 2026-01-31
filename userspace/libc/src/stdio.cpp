#include "../include/stdio.hpp"
#include "../include/string.hpp"
#include "../include/syscall.hpp"
#include "../include/unistd.hpp"

namespace sertos::libc {

i32 putchar(i32 c) {
    char ch = static_cast<char>(c);
    if (write(STDOUT_FD, &ch, 1) == 1) {
        return c;
    }
    return EOF;
}

i32 puts(const char* s) {
    usize len = strlen(s);
    if (write(STDOUT_FD, s, len) != static_cast<ssize_t>(len)) {
        return EOF;
    }
    if (putchar('\n') == EOF) {
        return EOF;
    }
    return 0;
}

namespace {

void printString(char** buf, const char* s) {
    while (*s) {
        **buf = *s++;
        (*buf)++;
    }
}

void printChar(char** buf, char c) {
    **buf = c;
    (*buf)++;
}

void printInt(char** buf, i64 value, i32 base, bool uppercase, i32 width, char pad) {
    char digits[32];
    i32 i = 0;
    bool negative = false;
    
    if (value < 0 && base == 10) {
        negative = true;
        value = -value;
    }
    
    u64 uvalue = static_cast<u64>(value);
    
    if (uvalue == 0) {
        digits[i++] = '0';
    } else {
        const char* hexchars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        while (uvalue > 0) {
            digits[i++] = hexchars[uvalue % base];
            uvalue /= base;
        }
    }
    
    i32 totalWidth = i + (negative ? 1 : 0);
    while (totalWidth < width) {
        printChar(buf, pad);
        totalWidth++;
    }
    
    if (negative) {
        printChar(buf, '-');
    }
    
    while (i > 0) {
        printChar(buf, digits[--i]);
    }
}

void printUint(char** buf, u64 value, i32 base, bool uppercase, i32 width, char pad) {
    char digits[32];
    i32 i = 0;
    
    if (value == 0) {
        digits[i++] = '0';
    } else {
        const char* hexchars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        while (value > 0) {
            digits[i++] = hexchars[value % base];
            value /= base;
        }
    }
    
    while (i < width) {
        printChar(buf, pad);
        i++;
    }
    
    i32 j = i;
    while (j > 0) {
        printChar(buf, digits[--j]);
    }
}

}

i32 vsprintf(char* str, const char* format, __builtin_va_list ap) {
    char* buf = str;
    
    while (*format) {
        if (*format != '%') {
            printChar(&buf, *format++);
            continue;
        }
        
        format++;
        
        char pad = ' ';
        i32 width = 0;
        
        if (*format == '0') {
            pad = '0';
            format++;
        }
        
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }
        
        bool isLong = false;
        if (*format == 'l') {
            isLong = true;
            format++;
            if (*format == 'l') {
                format++;
            }
        }
        
        switch (*format) {
            case 'd':
            case 'i': {
                i64 val = isLong ? __builtin_va_arg(ap, i64) : __builtin_va_arg(ap, i32);
                printInt(&buf, val, 10, false, width, pad);
                break;
            }
            case 'u': {
                u64 val = isLong ? __builtin_va_arg(ap, u64) : __builtin_va_arg(ap, u32);
                printUint(&buf, val, 10, false, width, pad);
                break;
            }
            case 'x': {
                u64 val = isLong ? __builtin_va_arg(ap, u64) : __builtin_va_arg(ap, u32);
                printUint(&buf, val, 16, false, width, pad);
                break;
            }
            case 'X': {
                u64 val = isLong ? __builtin_va_arg(ap, u64) : __builtin_va_arg(ap, u32);
                printUint(&buf, val, 16, true, width, pad);
                break;
            }
            case 'p': {
                printString(&buf, "0x");
                u64 val = reinterpret_cast<u64>(__builtin_va_arg(ap, void*));
                printUint(&buf, val, 16, false, 16, '0');
                break;
            }
            case 's': {
                const char* s = __builtin_va_arg(ap, const char*);
                if (s == nullptr) s = "(null)";
                printString(&buf, s);
                break;
            }
            case 'c': {
                char c = static_cast<char>(__builtin_va_arg(ap, i32));
                printChar(&buf, c);
                break;
            }
            case '%':
                printChar(&buf, '%');
                break;
            default:
                printChar(&buf, '%');
                printChar(&buf, *format);
                break;
        }
        format++;
    }
    
    *buf = '\0';
    return buf - str;
}

i32 sprintf(char* str, const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    i32 ret = vsprintf(str, format, ap);
    __builtin_va_end(ap);
    return ret;
}

i32 printf(const char* format, ...) {
    char buf[1024];
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    i32 len = vsprintf(buf, format, ap);
    __builtin_va_end(ap);
    
    write(STDOUT_FD, buf, len);
    return len;
}

}
