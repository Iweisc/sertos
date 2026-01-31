#pragma once

#include "types.hpp"

namespace sertos::libc {

constexpr i32 EOF = -1;
constexpr usize BUFSIZ = 1024;
constexpr usize FILENAME_MAX = 256;
constexpr usize FOPEN_MAX = 64;
constexpr usize L_tmpnam = 20;

constexpr i32 _IOFBF = 0;
constexpr i32 _IOLBF = 1;
constexpr i32 _IONBF = 2;

constexpr i32 SEEK_SET = 0;
constexpr i32 SEEK_CUR = 1;
constexpr i32 SEEK_END = 2;

struct FILE {
    i32 fd;
    i32 flags;
    i32 mode;
    i32 error;
    i32 eof;
    i32 bufmode;
    char* buffer;
    usize bufsize;
    usize bufpos;
    usize buflen;
    bool ownsBuffer;
};

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE* fopen(const char* pathname, const char* mode);
FILE* fdopen(i32 fd, const char* mode);
FILE* freopen(const char* pathname, const char* mode, FILE* stream);
i32 fclose(FILE* stream);
i32 fflush(FILE* stream);

usize fread(void* ptr, usize size, usize nmemb, FILE* stream);
usize fwrite(const void* ptr, usize size, usize nmemb, FILE* stream);

i32 fgetc(FILE* stream);
i32 getc(FILE* stream);
i32 getchar();
char* fgets(char* s, i32 size, FILE* stream);
i32 ungetc(i32 c, FILE* stream);

i32 fputc(i32 c, FILE* stream);
i32 putc(i32 c, FILE* stream);
i32 putchar(i32 c);
i32 fputs(const char* s, FILE* stream);
i32 puts(const char* s);

i32 fprintf(FILE* stream, const char* format, ...);
i32 printf(const char* format, ...);
i32 sprintf(char* str, const char* format, ...);
i32 snprintf(char* str, usize size, const char* format, ...);
i32 vfprintf(FILE* stream, const char* format, __builtin_va_list ap);
i32 vprintf(const char* format, __builtin_va_list ap);
i32 vsprintf(char* str, const char* format, __builtin_va_list ap);
i32 vsnprintf(char* str, usize size, const char* format, __builtin_va_list ap);

i32 fscanf(FILE* stream, const char* format, ...);
i32 scanf(const char* format, ...);
i32 sscanf(const char* str, const char* format, ...);

i32 fseek(FILE* stream, i64 offset, i32 whence);
i64 ftell(FILE* stream);
void rewind(FILE* stream);
i32 fgetpos(FILE* stream, i64* pos);
i32 fsetpos(FILE* stream, const i64* pos);

i32 feof(FILE* stream);
i32 ferror(FILE* stream);
void clearerr(FILE* stream);

i32 fileno(FILE* stream);

void perror(const char* s);

i32 setvbuf(FILE* stream, char* buf, i32 mode, usize size);
void setbuf(FILE* stream, char* buf);
void setbuffer(FILE* stream, char* buf, usize size);
void setlinebuf(FILE* stream);

i32 remove(const char* pathname);
i32 rename(const char* oldpath, const char* newpath);

FILE* tmpfile();
char* tmpnam(char* s);

}
