#include "../include/stdio.hpp"
#include "../include/string.hpp"
#include "../include/syscall.hpp"
#include "../include/unistd.hpp"
#include "../include/stdlib.hpp"

namespace sertos::libc {

namespace {

constexpr i32 FILE_READ = 1;
constexpr i32 FILE_WRITE = 2;
constexpr i32 FILE_APPEND = 4;
constexpr i32 FILE_BINARY = 8;
constexpr i32 FILE_PLUS = 16;

FILE stdinFile = {0, FILE_READ, 0, 0, 0, _IOLBF, nullptr, 0, 0, 0, false};
FILE stdoutFile = {1, FILE_WRITE, 0, 0, 0, _IOLBF, nullptr, 0, 0, 0, false};
FILE stderrFile = {2, FILE_WRITE, 0, 0, 0, _IONBF, nullptr, 0, 0, 0, false};

FILE* openFiles[FOPEN_MAX] = {&stdinFile, &stdoutFile, &stderrFile};
i32 openFileCount = 3;

i32 parseMode(const char* mode) {
    i32 flags = 0;
    
    switch (*mode) {
        case 'r': flags = FILE_READ; break;
        case 'w': flags = FILE_WRITE; break;
        case 'a': flags = FILE_WRITE | FILE_APPEND; break;
        default: return -1;
    }
    mode++;
    
    while (*mode) {
        switch (*mode) {
            case 'b': flags |= FILE_BINARY; break;
            case '+': flags |= FILE_PLUS | FILE_READ | FILE_WRITE; break;
            default: break;
        }
        mode++;
    }
    
    return flags;
}

i32 modeToOpenFlags(i32 mode) {
    i32 flags = 0;
    
    if ((mode & FILE_READ) && (mode & FILE_WRITE)) {
        flags = O_RDWR;
    } else if (mode & FILE_WRITE) {
        flags = O_WRONLY;
    } else {
        flags = O_RDONLY;
    }
    
    if (mode & FILE_APPEND) {
        flags |= O_APPEND;
    }
    
    if ((mode & FILE_WRITE) && !(mode & FILE_READ)) {
        flags |= O_CREAT | O_TRUNC;
    }
    
    return flags;
}

void printString(char** buf, const char* s, usize* remaining) {
    while (*s && *remaining > 1) {
        **buf = *s++;
        (*buf)++;
        (*remaining)--;
    }
}

void printChar(char** buf, char c, usize* remaining) {
    if (*remaining > 1) {
        **buf = c;
        (*buf)++;
        (*remaining)--;
    }
}

void printInt(char** buf, i64 value, i32 base, bool uppercase, i32 width, char pad, usize* remaining) {
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
        printChar(buf, pad, remaining);
        totalWidth++;
    }
    
    if (negative) {
        printChar(buf, '-', remaining);
    }
    
    while (i > 0) {
        printChar(buf, digits[--i], remaining);
    }
}

void printUint(char** buf, u64 value, i32 base, bool uppercase, i32 width, char pad, usize* remaining) {
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
    
    i32 numDigits = i;
    while (numDigits < width) {
        printChar(buf, pad, remaining);
        numDigits++;
    }
    
    while (i > 0) {
        printChar(buf, digits[--i], remaining);
    }
}

}

FILE* stdin = &stdinFile;
FILE* stdout = &stdoutFile;
FILE* stderr = &stderrFile;

FILE* fopen(const char* pathname, const char* mode) {
    i32 flags = parseMode(mode);
    if (flags < 0) return nullptr;
    
    i32 openFlags = modeToOpenFlags(flags);
    i32 fd = open(pathname, openFlags, 0644);
    if (fd < 0) return nullptr;
    
    FILE* file = static_cast<FILE*>(malloc(sizeof(FILE)));
    if (!file) {
        close(fd);
        return nullptr;
    }
    
    file->fd = fd;
    file->flags = flags;
    file->mode = 0;
    file->error = 0;
    file->eof = 0;
    file->bufmode = _IOFBF;
    file->buffer = static_cast<char*>(malloc(BUFSIZ));
    file->bufsize = file->buffer ? BUFSIZ : 0;
    file->bufpos = 0;
    file->buflen = 0;
    file->ownsBuffer = true;
    
    if (openFileCount < static_cast<i32>(FOPEN_MAX)) {
        openFiles[openFileCount++] = file;
    }
    
    return file;
}

FILE* fdopen(i32 fd, const char* mode) {
    i32 flags = parseMode(mode);
    if (flags < 0) return nullptr;
    
    FILE* file = static_cast<FILE*>(malloc(sizeof(FILE)));
    if (!file) return nullptr;
    
    file->fd = fd;
    file->flags = flags;
    file->mode = 0;
    file->error = 0;
    file->eof = 0;
    file->bufmode = _IOFBF;
    file->buffer = static_cast<char*>(malloc(BUFSIZ));
    file->bufsize = file->buffer ? BUFSIZ : 0;
    file->bufpos = 0;
    file->buflen = 0;
    file->ownsBuffer = true;
    
    if (openFileCount < static_cast<i32>(FOPEN_MAX)) {
        openFiles[openFileCount++] = file;
    }
    
    return file;
}

FILE* freopen(const char* pathname, const char* mode, FILE* stream) {
    if (!stream) return nullptr;
    
    fflush(stream);
    close(stream->fd);
    
    i32 flags = parseMode(mode);
    if (flags < 0) return nullptr;
    
    i32 openFlags = modeToOpenFlags(flags);
    i32 fd = open(pathname, openFlags, 0644);
    if (fd < 0) return nullptr;
    
    stream->fd = fd;
    stream->flags = flags;
    stream->error = 0;
    stream->eof = 0;
    stream->bufpos = 0;
    stream->buflen = 0;
    
    return stream;
}

i32 fclose(FILE* stream) {
    if (!stream) return EOF;
    
    fflush(stream);
    i32 result = close(stream->fd);
    
    if (stream->ownsBuffer && stream->buffer) {
        free(stream->buffer);
    }
    
    for (i32 i = 0; i < openFileCount; i++) {
        if (openFiles[i] == stream) {
            openFiles[i] = openFiles[--openFileCount];
            break;
        }
    }
    
    if (stream != stdin && stream != stdout && stream != stderr) {
        free(stream);
    }
    
    return result < 0 ? EOF : 0;
}

i32 fflush(FILE* stream) {
    if (!stream) {
        for (i32 i = 0; i < openFileCount; i++) {
            if (openFiles[i] && (openFiles[i]->flags & FILE_WRITE)) {
                fflush(openFiles[i]);
            }
        }
        return 0;
    }
    
    if (!(stream->flags & FILE_WRITE) || stream->bufpos == 0) {
        return 0;
    }
    
    ssize_t written = write(stream->fd, stream->buffer, stream->bufpos);
    if (written < 0) {
        stream->error = 1;
        return EOF;
    }
    
    stream->bufpos = 0;
    return 0;
}

usize fread(void* ptr, usize size, usize nmemb, FILE* stream) {
    if (!stream || !ptr || size == 0 || nmemb == 0) return 0;
    
    usize total = size * nmemb;
    usize bytesRead = 0;
    char* dest = static_cast<char*>(ptr);
    
    while (bytesRead < total) {
        if (stream->bufpos < stream->buflen) {
            usize available = stream->buflen - stream->bufpos;
            usize toCopy = (total - bytesRead < available) ? (total - bytesRead) : available;
            memcpy(dest + bytesRead, stream->buffer + stream->bufpos, toCopy);
            stream->bufpos += toCopy;
            bytesRead += toCopy;
        } else {
            if (stream->bufsize > 0) {
                ssize_t n = read(stream->fd, stream->buffer, stream->bufsize);
                if (n <= 0) {
                    if (n == 0) stream->eof = 1;
                    else stream->error = 1;
                    break;
                }
                stream->bufpos = 0;
                stream->buflen = static_cast<usize>(n);
            } else {
                ssize_t n = read(stream->fd, dest + bytesRead, total - bytesRead);
                if (n <= 0) {
                    if (n == 0) stream->eof = 1;
                    else stream->error = 1;
                    break;
                }
                bytesRead += static_cast<usize>(n);
            }
        }
    }
    
    return bytesRead / size;
}

usize fwrite(const void* ptr, usize size, usize nmemb, FILE* stream) {
    if (!stream || !ptr || size == 0 || nmemb == 0) return 0;
    
    usize total = size * nmemb;
    const char* src = static_cast<const char*>(ptr);
    
    if (stream->bufmode == _IONBF || stream->bufsize == 0) {
        ssize_t written = write(stream->fd, src, total);
        if (written < 0) {
            stream->error = 1;
            return 0;
        }
        return static_cast<usize>(written) / size;
    }
    
    usize bytesWritten = 0;
    while (bytesWritten < total) {
        usize space = stream->bufsize - stream->bufpos;
        usize toWrite = (total - bytesWritten < space) ? (total - bytesWritten) : space;
        
        memcpy(stream->buffer + stream->bufpos, src + bytesWritten, toWrite);
        stream->bufpos += toWrite;
        bytesWritten += toWrite;
        
        bool shouldFlush = (stream->bufpos >= stream->bufsize);
        if (stream->bufmode == _IOLBF) {
            for (usize i = 0; i < toWrite; i++) {
                if (src[bytesWritten - toWrite + i] == '\n') {
                    shouldFlush = true;
                    break;
                }
            }
        }
        
        if (shouldFlush) {
            if (fflush(stream) == EOF) {
                return bytesWritten / size;
            }
        }
    }
    
    return bytesWritten / size;
}

i32 fgetc(FILE* stream) {
    if (!stream) return EOF;
    
    unsigned char c;
    if (fread(&c, 1, 1, stream) != 1) {
        return EOF;
    }
    return c;
}

i32 getc(FILE* stream) {
    return fgetc(stream);
}

i32 getchar() {
    return fgetc(stdin);
}

char* fgets(char* s, i32 size, FILE* stream) {
    if (!s || size <= 0 || !stream) return nullptr;
    
    i32 i = 0;
    while (i < size - 1) {
        i32 c = fgetc(stream);
        if (c == EOF) {
            if (i == 0) return nullptr;
            break;
        }
        s[i++] = static_cast<char>(c);
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

i32 ungetc(i32 c, FILE* stream) {
    if (!stream || c == EOF) return EOF;
    
    if (stream->bufpos > 0) {
        stream->bufpos--;
        stream->buffer[stream->bufpos] = static_cast<char>(c);
        stream->eof = 0;
        return c;
    }
    
    return EOF;
}

i32 fputc(i32 c, FILE* stream) {
    if (!stream) return EOF;
    
    unsigned char ch = static_cast<unsigned char>(c);
    if (fwrite(&ch, 1, 1, stream) != 1) {
        return EOF;
    }
    return c;
}

i32 putc(i32 c, FILE* stream) {
    return fputc(c, stream);
}

i32 putchar(i32 c) {
    return fputc(c, stdout);
}

i32 fputs(const char* s, FILE* stream) {
    if (!s || !stream) return EOF;
    
    usize len = strlen(s);
    if (fwrite(s, 1, len, stream) != len) {
        return EOF;
    }
    return 0;
}

i32 puts(const char* s) {
    if (fputs(s, stdout) == EOF) return EOF;
    if (fputc('\n', stdout) == EOF) return EOF;
    return 0;
}

i32 vsnprintf(char* str, usize size, const char* format, __builtin_va_list ap) {
    if (!str || size == 0) return 0;
    
    char* buf = str;
    usize remaining = size;
    
    while (*format && remaining > 1) {
        if (*format != '%') {
            printChar(&buf, *format++, &remaining);
            continue;
        }
        
        format++;
        
        char pad = ' ';
        i32 width = 0;
        bool leftAlign = false;
        
        if (*format == '-') {
            leftAlign = true;
            format++;
        }
        
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
        
        (void)leftAlign;
        
        switch (*format) {
            case 'd':
            case 'i': {
                i64 val = isLong ? __builtin_va_arg(ap, i64) : __builtin_va_arg(ap, i32);
                printInt(&buf, val, 10, false, width, pad, &remaining);
                break;
            }
            case 'u': {
                u64 val = isLong ? __builtin_va_arg(ap, u64) : __builtin_va_arg(ap, u32);
                printUint(&buf, val, 10, false, width, pad, &remaining);
                break;
            }
            case 'x': {
                u64 val = isLong ? __builtin_va_arg(ap, u64) : __builtin_va_arg(ap, u32);
                printUint(&buf, val, 16, false, width, pad, &remaining);
                break;
            }
            case 'X': {
                u64 val = isLong ? __builtin_va_arg(ap, u64) : __builtin_va_arg(ap, u32);
                printUint(&buf, val, 16, true, width, pad, &remaining);
                break;
            }
            case 'p': {
                printString(&buf, "0x", &remaining);
                u64 val = reinterpret_cast<u64>(__builtin_va_arg(ap, void*));
                printUint(&buf, val, 16, false, 16, '0', &remaining);
                break;
            }
            case 's': {
                const char* s = __builtin_va_arg(ap, const char*);
                if (s == nullptr) s = "(null)";
                printString(&buf, s, &remaining);
                break;
            }
            case 'c': {
                char c = static_cast<char>(__builtin_va_arg(ap, i32));
                printChar(&buf, c, &remaining);
                break;
            }
            case '%':
                printChar(&buf, '%', &remaining);
                break;
            default:
                printChar(&buf, '%', &remaining);
                printChar(&buf, *format, &remaining);
                break;
        }
        format++;
    }
    
    *buf = '\0';
    return static_cast<i32>(buf - str);
}

i32 vsprintf(char* str, const char* format, __builtin_va_list ap) {
    return vsnprintf(str, 0x7FFFFFFF, format, ap);
}

i32 vfprintf(FILE* stream, const char* format, __builtin_va_list ap) {
    char buf[1024];
    i32 len = vsnprintf(buf, sizeof(buf), format, ap);
    if (len > 0) {
        fwrite(buf, 1, static_cast<usize>(len), stream);
    }
    return len;
}

i32 vprintf(const char* format, __builtin_va_list ap) {
    return vfprintf(stdout, format, ap);
}

i32 sprintf(char* str, const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    i32 ret = vsprintf(str, format, ap);
    __builtin_va_end(ap);
    return ret;
}

i32 snprintf(char* str, usize size, const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    i32 ret = vsnprintf(str, size, format, ap);
    __builtin_va_end(ap);
    return ret;
}

i32 fprintf(FILE* stream, const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    i32 ret = vfprintf(stream, format, ap);
    __builtin_va_end(ap);
    return ret;
}

i32 printf(const char* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    i32 ret = vprintf(format, ap);
    __builtin_va_end(ap);
    return ret;
}

i32 fscanf(FILE*, const char*, ...) {
    return EOF;
}

i32 scanf(const char*, ...) {
    return EOF;
}

i32 sscanf(const char*, const char*, ...) {
    return EOF;
}

i32 fseek(FILE* stream, i64 offset, i32 whence) {
    if (!stream) return -1;
    
    fflush(stream);
    stream->bufpos = 0;
    stream->buflen = 0;
    
    off_t result = lseek(stream->fd, offset, whence);
    if (result < 0) {
        stream->error = 1;
        return -1;
    }
    
    stream->eof = 0;
    return 0;
}

i64 ftell(FILE* stream) {
    if (!stream) return -1;
    
    off_t pos = lseek(stream->fd, 0, SEEK_CUR);
    if (pos < 0) return -1;
    
    if (stream->flags & FILE_READ) {
        pos -= static_cast<off_t>(stream->buflen - stream->bufpos);
    } else if (stream->flags & FILE_WRITE) {
        pos += static_cast<off_t>(stream->bufpos);
    }
    
    return pos;
}

void rewind(FILE* stream) {
    if (stream) {
        fseek(stream, 0, SEEK_SET);
        stream->error = 0;
    }
}

i32 fgetpos(FILE* stream, i64* pos) {
    if (!stream || !pos) return -1;
    *pos = ftell(stream);
    return (*pos < 0) ? -1 : 0;
}

i32 fsetpos(FILE* stream, const i64* pos) {
    if (!stream || !pos) return -1;
    return fseek(stream, *pos, SEEK_SET);
}

i32 feof(FILE* stream) {
    return stream ? stream->eof : 0;
}

i32 ferror(FILE* stream) {
    return stream ? stream->error : 0;
}

void clearerr(FILE* stream) {
    if (stream) {
        stream->error = 0;
        stream->eof = 0;
    }
}

i32 fileno(FILE* stream) {
    return stream ? stream->fd : -1;
}

void perror(const char* s) {
    if (s && *s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs("Error\n", stderr);
}

i32 setvbuf(FILE* stream, char* buf, i32 mode, usize size) {
    if (!stream) return -1;
    
    fflush(stream);
    
    if (stream->ownsBuffer && stream->buffer) {
        free(stream->buffer);
    }
    
    stream->bufmode = mode;
    
    if (mode == _IONBF) {
        stream->buffer = nullptr;
        stream->bufsize = 0;
        stream->ownsBuffer = false;
    } else if (buf) {
        stream->buffer = buf;
        stream->bufsize = size;
        stream->ownsBuffer = false;
    } else {
        stream->buffer = static_cast<char*>(malloc(size));
        stream->bufsize = stream->buffer ? size : 0;
        stream->ownsBuffer = true;
    }
    
    stream->bufpos = 0;
    stream->buflen = 0;
    
    return 0;
}

void setbuf(FILE* stream, char* buf) {
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

void setbuffer(FILE* stream, char* buf, usize size) {
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, size);
}

void setlinebuf(FILE* stream) {
    setvbuf(stream, nullptr, _IOLBF, BUFSIZ);
}

i32 remove(const char* pathname) {
    return unlink(pathname);
}

i32 rename(const char* oldpath, const char* newpath) {
    return static_cast<i32>(syscall2(SYS_RENAME, 
        reinterpret_cast<u64>(oldpath), 
        reinterpret_cast<u64>(newpath)));
}

FILE* tmpfile() {
    return nullptr;
}

char* tmpnam(char*) {
    return nullptr;
}

}
