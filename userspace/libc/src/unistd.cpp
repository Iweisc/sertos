#include "../include/unistd.hpp"
#include "../include/syscall.hpp"
#include "../include/string.hpp"

namespace sertos::libc {

char* optarg = nullptr;
i32 optind = 1;
i32 opterr = 1;
i32 optopt = 0;

pid_t getpid() {
    return static_cast<pid_t>(syscall0(SYS_GETPID));
}

pid_t getppid() {
    return static_cast<pid_t>(syscall0(SYS_GETPPID));
}

pid_t fork() {
    return static_cast<pid_t>(syscall0(SYS_FORK));
}

i32 execve(const char* pathname, char* const argv[], char* const envp[]) {
    return static_cast<i32>(syscall3(SYS_EXEC, 
        reinterpret_cast<u64>(pathname), 
        reinterpret_cast<u64>(argv), 
        reinterpret_cast<u64>(envp)));
}

i32 execv(const char* pathname, char* const argv[]) {
    return execve(pathname, argv, nullptr);
}

i32 execvp(const char* file, char* const argv[]) {
    if (file[0] == '/') {
        return execve(file, argv, nullptr);
    }
    
    char path[256];
    const char* searchPaths[] = {"/bin", "/usr/bin", "/sbin", "/usr/sbin", nullptr};
    
    for (i32 i = 0; searchPaths[i] != nullptr; i++) {
        usize pathLen = strlen(searchPaths[i]);
        usize fileLen = strlen(file);
        
        if (pathLen + 1 + fileLen >= 256) continue;
        
        memcpy(path, searchPaths[i], pathLen);
        path[pathLen] = '/';
        memcpy(path + pathLen + 1, file, fileLen + 1);
        
        if (access(path, X_OK) == 0) {
            return execve(path, argv, nullptr);
        }
    }
    
    return -1;
}

pid_t wait(i32* status) {
    return static_cast<pid_t>(syscall1(SYS_WAIT, reinterpret_cast<u64>(status)));
}

pid_t waitpid(pid_t pid, i32* status, i32 options) {
    return static_cast<pid_t>(syscall3(SYS_WAIT, 
        static_cast<u64>(pid), 
        reinterpret_cast<u64>(status), 
        static_cast<u64>(options)));
}

[[noreturn]] void _exit(i32 status) {
    syscall1(SYS_EXIT, status);
    __builtin_unreachable();
}

u32 sleep(u32 seconds) {
    struct timespec req = {static_cast<i64>(seconds), 0};
    struct timespec rem = {0, 0};
    nanosleep(&req, &rem);
    return static_cast<u32>(rem.tv_sec);
}

i32 usleep(u64 usec) {
    struct timespec req = {
        static_cast<i64>(usec / 1000000),
        static_cast<i64>((usec % 1000000) * 1000)
    };
    return nanosleep(&req, nullptr);
}

i32 nanosleep(const struct timespec* req, struct timespec* rem) {
    return static_cast<i32>(syscall2(SYS_NANOSLEEP, 
        reinterpret_cast<u64>(req), 
        reinterpret_cast<u64>(rem)));
}

void yield() {
    syscall0(SYS_YIELD);
}

i32 open(const char* pathname, i32 flags, mode_t mode) {
    return static_cast<i32>(syscall3(SYS_OPEN, 
        reinterpret_cast<u64>(pathname), 
        static_cast<u64>(flags), 
        static_cast<u64>(mode)));
}

ssize_t read(i32 fd, void* buf, usize count) {
    return syscall3(SYS_READ, fd, reinterpret_cast<u64>(buf), count);
}

ssize_t write(i32 fd, const void* buf, usize count) {
    return syscall3(SYS_WRITE, fd, reinterpret_cast<u64>(buf), count);
}

i32 close(i32 fd) {
    return static_cast<i32>(syscall1(SYS_CLOSE, fd));
}

off_t lseek(i32 fd, off_t offset, i32 whence) {
    return static_cast<off_t>(syscall3(SYS_LSEEK, 
        static_cast<u64>(fd), 
        static_cast<u64>(offset), 
        static_cast<u64>(whence)));
}

i32 dup(i32 oldfd) {
    return static_cast<i32>(syscall1(SYS_DUP, static_cast<u64>(oldfd)));
}

i32 dup2(i32 oldfd, i32 newfd) {
    return static_cast<i32>(syscall2(SYS_DUP2, 
        static_cast<u64>(oldfd), 
        static_cast<u64>(newfd)));
}

i32 pipe(i32 pipefd[2]) {
    return static_cast<i32>(syscall1(SYS_PIPE, reinterpret_cast<u64>(pipefd)));
}

i32 fcntl(i32 fd, i32 cmd, ...) {
    return static_cast<i32>(syscall2(SYS_FCNTL, 
        static_cast<u64>(fd), 
        static_cast<u64>(cmd)));
}

i32 ioctl(i32 fd, u64 request, void* arg) {
    return static_cast<i32>(syscall3(SYS_IOCTL, 
        static_cast<u64>(fd), 
        request, 
        reinterpret_cast<u64>(arg)));
}

i32 stat(const char* pathname, struct stat* statbuf) {
    return static_cast<i32>(syscall2(SYS_STAT, 
        reinterpret_cast<u64>(pathname), 
        reinterpret_cast<u64>(statbuf)));
}

i32 fstat(i32 fd, struct stat* statbuf) {
    return static_cast<i32>(syscall2(SYS_FSTAT, 
        static_cast<u64>(fd), 
        reinterpret_cast<u64>(statbuf)));
}

i32 lstat(const char* pathname, struct stat* statbuf) {
    return stat(pathname, statbuf);
}

i32 access(const char* pathname, i32 mode) {
    return static_cast<i32>(syscall2(SYS_ACCESS, 
        reinterpret_cast<u64>(pathname), 
        static_cast<u64>(mode)));
}

i32 mkdir(const char* pathname, mode_t mode) {
    return static_cast<i32>(syscall2(SYS_MKDIR, 
        reinterpret_cast<u64>(pathname), 
        static_cast<u64>(mode)));
}

i32 rmdir(const char* pathname) {
    return static_cast<i32>(syscall1(SYS_RMDIR, reinterpret_cast<u64>(pathname)));
}

i32 unlink(const char* pathname) {
    return static_cast<i32>(syscall1(SYS_UNLINK, reinterpret_cast<u64>(pathname)));
}

i32 link(const char* oldpath, const char* newpath) {
    return static_cast<i32>(syscall2(SYS_LINK, 
        reinterpret_cast<u64>(oldpath), 
        reinterpret_cast<u64>(newpath)));
}

i32 symlink(const char* target, const char* linkpath) {
    return static_cast<i32>(syscall2(SYS_SYMLINK, 
        reinterpret_cast<u64>(target), 
        reinterpret_cast<u64>(linkpath)));
}

ssize_t readlink(const char* pathname, char* buf, usize bufsiz) {
    return syscall3(SYS_READLINK, 
        reinterpret_cast<u64>(pathname), 
        reinterpret_cast<u64>(buf), 
        bufsiz);
}

i32 rename(const char* oldpath, const char* newpath) {
    return static_cast<i32>(syscall2(SYS_RENAME, 
        reinterpret_cast<u64>(oldpath), 
        reinterpret_cast<u64>(newpath)));
}

i32 chdir(const char* path) {
    return static_cast<i32>(syscall1(SYS_CHDIR, reinterpret_cast<u64>(path)));
}

char* getcwd(char* buf, usize size) {
    i64 result = syscall2(SYS_GETCWD, reinterpret_cast<u64>(buf), size);
    if (result < 0) return nullptr;
    return buf;
}

i32 chmod(const char* pathname, mode_t mode) {
    return static_cast<i32>(syscall2(SYS_CHMOD, 
        reinterpret_cast<u64>(pathname), 
        static_cast<u64>(mode)));
}

i32 fchmod(i32 fd, mode_t mode) {
    return static_cast<i32>(syscall2(SYS_CHMOD, 
        static_cast<u64>(fd), 
        static_cast<u64>(mode)));
}

i32 chown(const char* pathname, uid_t owner, gid_t group) {
    return static_cast<i32>(syscall3(SYS_CHOWN, 
        reinterpret_cast<u64>(pathname), 
        static_cast<u64>(owner), 
        static_cast<u64>(group)));
}

i32 fchown(i32 fd, uid_t owner, gid_t group) {
    return static_cast<i32>(syscall3(SYS_CHOWN, 
        static_cast<u64>(fd), 
        static_cast<u64>(owner), 
        static_cast<u64>(group)));
}

mode_t umask(mode_t mask) {
    return static_cast<mode_t>(syscall1(SYS_UMASK, static_cast<u64>(mask)));
}

i32 truncate(const char* path, off_t length) {
    return static_cast<i32>(syscall2(SYS_TRUNCATE, 
        reinterpret_cast<u64>(path), 
        static_cast<u64>(length)));
}

i32 ftruncate(i32 fd, off_t length) {
    return static_cast<i32>(syscall2(SYS_FTRUNCATE, 
        static_cast<u64>(fd), 
        static_cast<u64>(length)));
}

void sync() {
    syscall0(SYS_SYNC);
}

i32 fsync(i32 fd) {
    return static_cast<i32>(syscall1(SYS_FSYNC, static_cast<u64>(fd)));
}

ssize_t getdents(i32 fd, void* dirp, usize count) {
    return syscall3(SYS_GETDENTS, 
        static_cast<u64>(fd), 
        reinterpret_cast<u64>(dirp), 
        count);
}

uid_t getuid() {
    return static_cast<uid_t>(syscall0(SYS_GETUID));
}

uid_t geteuid() {
    return static_cast<uid_t>(syscall0(SYS_GETEUID));
}

gid_t getgid() {
    return static_cast<gid_t>(syscall0(SYS_GETGID));
}

gid_t getegid() {
    return static_cast<gid_t>(syscall0(SYS_GETEGID));
}

i32 setuid(uid_t uid) {
    return static_cast<i32>(syscall1(SYS_SETUID, static_cast<u64>(uid)));
}

i32 seteuid(uid_t euid) {
    return static_cast<i32>(syscall1(SYS_SETEUID, static_cast<u64>(euid)));
}

i32 setgid(gid_t gid) {
    return static_cast<i32>(syscall1(SYS_SETGID, static_cast<u64>(gid)));
}

i32 setegid(gid_t egid) {
    return static_cast<i32>(syscall1(SYS_SETEGID, static_cast<u64>(egid)));
}

i32 setreuid(uid_t ruid, uid_t euid) {
    return static_cast<i32>(syscall2(SYS_SETUID, 
        static_cast<u64>(ruid), 
        static_cast<u64>(euid)));
}

i32 setregid(gid_t rgid, gid_t egid) {
    return static_cast<i32>(syscall2(SYS_SETGID, 
        static_cast<u64>(rgid), 
        static_cast<u64>(egid)));
}

void* brk(void* addr) {
    return reinterpret_cast<void*>(syscall1(SYS_BRK, reinterpret_cast<u64>(addr)));
}

void* sbrk(ssize_t increment) {
    void* current = brk(nullptr);
    if (increment == 0) return current;
    
    void* newBrk = brk(reinterpret_cast<void*>(
        reinterpret_cast<u64>(current) + increment));
    
    if (newBrk == current) return reinterpret_cast<void*>(-1);
    return current;
}

void* mmap(void* addr, usize length, i32 prot, i32 flags, i32 fd, off_t offset) {
    return reinterpret_cast<void*>(syscall6(SYS_MMAP, 
        reinterpret_cast<u64>(addr), 
        length, 
        static_cast<u64>(prot), 
        static_cast<u64>(flags), 
        static_cast<u64>(fd), 
        static_cast<u64>(offset)));
}

i32 munmap(void* addr, usize length) {
    return static_cast<i32>(syscall2(SYS_MUNMAP, 
        reinterpret_cast<u64>(addr), 
        length));
}

i32 mprotect(void* addr, usize len, i32 prot) {
    return static_cast<i32>(syscall3(SYS_MMAP, 
        reinterpret_cast<u64>(addr), 
        len, 
        static_cast<u64>(prot)));
}

i32 kill(pid_t pid, i32 sig) {
    return static_cast<i32>(syscall2(SYS_KILL, 
        static_cast<u64>(pid), 
        static_cast<u64>(sig)));
}

i32 raise(i32 sig) {
    return kill(getpid(), sig);
}

i32 gettimeofday(struct timeval* tv, void*) {
    struct timespec ts;
    i32 result = clock_gettime(CLOCK_REALTIME, &ts);
    if (result == 0 && tv) {
        tv->tv_sec = ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000;
    }
    return result;
}

i32 clock_gettime(i32 clk_id, struct timespec* tp) {
    return static_cast<i32>(syscall2(SYS_CLOCK_GETTIME, 
        static_cast<u64>(clk_id), 
        reinterpret_cast<u64>(tp)));
}

i32 uname(struct utsname* buf) {
    return static_cast<i32>(syscall1(SYS_UNAME, reinterpret_cast<u64>(buf)));
}

i32 isatty(i32 fd) {
    struct stat st;
    if (fstat(fd, &st) < 0) return 0;
    return S_ISCHR(st.st_mode) ? 1 : 0;
}

char* ttyname(i32 fd) {
    static char ttyname_buf[32];
    if (!isatty(fd)) return nullptr;
    
    if (fd == 0) {
        memcpy(ttyname_buf, "/dev/stdin", 11);
    } else if (fd == 1) {
        memcpy(ttyname_buf, "/dev/stdout", 12);
    } else if (fd == 2) {
        memcpy(ttyname_buf, "/dev/stderr", 12);
    } else {
        memcpy(ttyname_buf, "/dev/tty", 9);
    }
    return ttyname_buf;
}

i32 getopt(i32 argc, char* const argv[], const char* optstring) {
    static i32 optpos = 1;
    
    if (optind >= argc || argv[optind] == nullptr) {
        return -1;
    }
    
    const char* arg = argv[optind];
    
    if (arg[0] != '-' || arg[1] == '\0') {
        return -1;
    }
    
    if (arg[1] == '-' && arg[2] == '\0') {
        optind++;
        return -1;
    }
    
    char opt = arg[optpos];
    const char* p = optstring;
    
    while (*p != '\0') {
        if (*p == opt) {
            if (p[1] == ':') {
                if (arg[optpos + 1] != '\0') {
                    optarg = const_cast<char*>(&arg[optpos + 1]);
                    optind++;
                    optpos = 1;
                } else if (optind + 1 < argc) {
                    optarg = argv[optind + 1];
                    optind += 2;
                    optpos = 1;
                } else {
                    optopt = opt;
                    optind++;
                    optpos = 1;
                    return '?';
                }
            } else {
                if (arg[optpos + 1] != '\0') {
                    optpos++;
                } else {
                    optind++;
                    optpos = 1;
                }
            }
            return opt;
        }
        p++;
    }
    
    optopt = opt;
    if (arg[optpos + 1] != '\0') {
        optpos++;
    } else {
        optind++;
        optpos = 1;
    }
    return '?';
}

i32 getenv_r(const char*, char*, usize) {
    return -1;
}

i32 setenv(const char*, const char*, i32) {
    return -1;
}

i32 unsetenv(const char*) {
    return -1;
}

i32 reboot(i32 cmd) {
    return static_cast<i32>(syscall1(SYS_REBOOT, static_cast<u64>(cmd)));
}

i32 poweroff() {
    return static_cast<i32>(syscall0(SYS_POWEROFF));
}

}
