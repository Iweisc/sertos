#pragma once

#include "types.hpp"

namespace sertos::libc {

using uid_t = u32;
using gid_t = u32;
using off_t = i64;
using mode_t = u32;

struct stat {
    u64 st_dev;
    u64 st_ino;
    mode_t st_mode;
    u32 st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    u64 st_rdev;
    off_t st_size;
    u64 st_blksize;
    u64 st_blocks;
    u64 st_atime;
    u64 st_mtime;
    u64 st_ctime;
};

struct dirent {
    u64 d_ino;
    off_t d_off;
    u16 d_reclen;
    u8 d_type;
    char d_name[256];
};

constexpr u8 DT_UNKNOWN = 0;
constexpr u8 DT_FIFO = 1;
constexpr u8 DT_CHR = 2;
constexpr u8 DT_DIR = 4;
constexpr u8 DT_BLK = 6;
constexpr u8 DT_REG = 8;
constexpr u8 DT_LNK = 10;
constexpr u8 DT_SOCK = 12;

constexpr mode_t S_IFMT = 0170000;
constexpr mode_t S_IFSOCK = 0140000;
constexpr mode_t S_IFLNK = 0120000;
constexpr mode_t S_IFREG = 0100000;
constexpr mode_t S_IFBLK = 0060000;
constexpr mode_t S_IFDIR = 0040000;
constexpr mode_t S_IFCHR = 0020000;
constexpr mode_t S_IFIFO = 0010000;

constexpr mode_t S_ISUID = 04000;
constexpr mode_t S_ISGID = 02000;
constexpr mode_t S_ISVTX = 01000;

constexpr mode_t S_IRWXU = 00700;
constexpr mode_t S_IRUSR = 00400;
constexpr mode_t S_IWUSR = 00200;
constexpr mode_t S_IXUSR = 00100;

constexpr mode_t S_IRWXG = 00070;
constexpr mode_t S_IRGRP = 00040;
constexpr mode_t S_IWGRP = 00020;
constexpr mode_t S_IXGRP = 00010;

constexpr mode_t S_IRWXO = 00007;
constexpr mode_t S_IROTH = 00004;
constexpr mode_t S_IWOTH = 00002;
constexpr mode_t S_IXOTH = 00001;

inline bool S_ISREG(mode_t m) { return (m & S_IFMT) == S_IFREG; }
inline bool S_ISDIR(mode_t m) { return (m & S_IFMT) == S_IFDIR; }
inline bool S_ISCHR(mode_t m) { return (m & S_IFMT) == S_IFCHR; }
inline bool S_ISBLK(mode_t m) { return (m & S_IFMT) == S_IFBLK; }
inline bool S_ISFIFO(mode_t m) { return (m & S_IFMT) == S_IFIFO; }
inline bool S_ISLNK(mode_t m) { return (m & S_IFMT) == S_IFLNK; }
inline bool S_ISSOCK(mode_t m) { return (m & S_IFMT) == S_IFSOCK; }

pid_t getpid();
pid_t getppid();
pid_t fork();
i32 execve(const char* pathname, char* const argv[], char* const envp[]);
i32 execv(const char* pathname, char* const argv[]);
i32 execvp(const char* file, char* const argv[]);
pid_t wait(i32* status);
pid_t waitpid(pid_t pid, i32* status, i32 options);
[[noreturn]] void _exit(i32 status);
u32 sleep(u32 seconds);
i32 usleep(u64 usec);
i32 nanosleep(const struct timespec* req, struct timespec* rem);
void yield();

i32 open(const char* pathname, i32 flags, mode_t mode = 0);
ssize_t read(i32 fd, void* buf, usize count);
ssize_t write(i32 fd, const void* buf, usize count);
i32 close(i32 fd);
off_t lseek(i32 fd, off_t offset, i32 whence);
i32 dup(i32 oldfd);
i32 dup2(i32 oldfd, i32 newfd);
i32 pipe(i32 pipefd[2]);
i32 fcntl(i32 fd, i32 cmd, ...);
i32 ioctl(i32 fd, u64 request, void* arg);

i32 stat(const char* pathname, struct stat* statbuf);
i32 fstat(i32 fd, struct stat* statbuf);
i32 lstat(const char* pathname, struct stat* statbuf);
i32 access(const char* pathname, i32 mode);

i32 mkdir(const char* pathname, mode_t mode);
i32 rmdir(const char* pathname);
i32 unlink(const char* pathname);
i32 link(const char* oldpath, const char* newpath);
i32 symlink(const char* target, const char* linkpath);
ssize_t readlink(const char* pathname, char* buf, usize bufsiz);
i32 rename(const char* oldpath, const char* newpath);
i32 chdir(const char* path);
char* getcwd(char* buf, usize size);
i32 chmod(const char* pathname, mode_t mode);
i32 fchmod(i32 fd, mode_t mode);
i32 chown(const char* pathname, uid_t owner, gid_t group);
i32 fchown(i32 fd, uid_t owner, gid_t group);
mode_t umask(mode_t mask);
i32 truncate(const char* path, off_t length);
i32 ftruncate(i32 fd, off_t length);
void sync();
i32 fsync(i32 fd);
ssize_t getdents(i32 fd, void* dirp, usize count);

uid_t getuid();
uid_t geteuid();
gid_t getgid();
gid_t getegid();
i32 setuid(uid_t uid);
i32 seteuid(uid_t euid);
i32 setgid(gid_t gid);
i32 setegid(gid_t egid);
i32 setreuid(uid_t ruid, uid_t euid);
i32 setregid(gid_t rgid, gid_t egid);

void* brk(void* addr);
void* sbrk(ssize_t increment);
void* mmap(void* addr, usize length, i32 prot, i32 flags, i32 fd, off_t offset);
i32 munmap(void* addr, usize length);
i32 mprotect(void* addr, usize len, i32 prot);

constexpr i32 PROT_NONE = 0;
constexpr i32 PROT_READ = 1;
constexpr i32 PROT_WRITE = 2;
constexpr i32 PROT_EXEC = 4;

constexpr i32 MAP_SHARED = 0x01;
constexpr i32 MAP_PRIVATE = 0x02;
constexpr i32 MAP_FIXED = 0x10;
constexpr i32 MAP_ANONYMOUS = 0x20;
constexpr i32 MAP_FAILED_VAL = -1;

inline void* MAP_FAILED() { return reinterpret_cast<void*>(MAP_FAILED_VAL); }

i32 kill(pid_t pid, i32 sig);
i32 raise(i32 sig);

constexpr i32 SIGHUP = 1;
constexpr i32 SIGINT = 2;
constexpr i32 SIGQUIT = 3;
constexpr i32 SIGILL = 4;
constexpr i32 SIGTRAP = 5;
constexpr i32 SIGABRT = 6;
constexpr i32 SIGBUS = 7;
constexpr i32 SIGFPE = 8;
constexpr i32 SIGKILL = 9;
constexpr i32 SIGUSR1 = 10;
constexpr i32 SIGSEGV = 11;
constexpr i32 SIGUSR2 = 12;
constexpr i32 SIGPIPE = 13;
constexpr i32 SIGALRM = 14;
constexpr i32 SIGTERM = 15;
constexpr i32 SIGCHLD = 17;
constexpr i32 SIGCONT = 18;
constexpr i32 SIGSTOP = 19;
constexpr i32 SIGTSTP = 20;
constexpr i32 SIGTTIN = 21;
constexpr i32 SIGTTOU = 22;

constexpr i32 WNOHANG = 1;
constexpr i32 WUNTRACED = 2;

inline bool WIFEXITED(i32 status) { return (status & 0x7f) == 0; }
inline i32 WEXITSTATUS(i32 status) { return (status >> 8) & 0xff; }
inline bool WIFSIGNALED(i32 status) { return (status & 0x7f) != 0 && (status & 0x7f) != 0x7f; }
inline i32 WTERMSIG(i32 status) { return status & 0x7f; }
inline bool WIFSTOPPED(i32 status) { return (status & 0xff) == 0x7f; }
inline i32 WSTOPSIG(i32 status) { return (status >> 8) & 0xff; }

struct timespec {
    i64 tv_sec;
    i64 tv_nsec;
};

struct timeval {
    i64 tv_sec;
    i64 tv_usec;
};

i32 gettimeofday(struct timeval* tv, void* tz);
i32 clock_gettime(i32 clk_id, struct timespec* tp);

constexpr i32 CLOCK_REALTIME = 0;
constexpr i32 CLOCK_MONOTONIC = 1;
constexpr i32 CLOCK_PROCESS_CPUTIME_ID = 2;
constexpr i32 CLOCK_THREAD_CPUTIME_ID = 3;

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

i32 uname(struct utsname* buf);

i32 isatty(i32 fd);
char* ttyname(i32 fd);

i32 getopt(i32 argc, char* const argv[], const char* optstring);
extern char* optarg;
extern i32 optind;
extern i32 opterr;
extern i32 optopt;

i32 getenv_r(const char* name, char* buf, usize bufsize);
i32 setenv(const char* name, const char* value, i32 overwrite);
i32 unsetenv(const char* name);

i32 reboot(i32 cmd);
i32 poweroff();

constexpr i32 RB_AUTOBOOT = 0x01234567;
constexpr i32 RB_HALT_SYSTEM = 0xcdef0123;
constexpr i32 RB_POWER_OFF = 0x4321fedc;

}
