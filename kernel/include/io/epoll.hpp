#pragma once

#include "../types.hpp"

namespace sertos::io {

constexpr u32 MAX_EPOLL_INSTANCES = 64;
constexpr u32 MAX_EPOLL_EVENTS = 256;
constexpr u32 MAX_EPOLL_FDS_PER_INSTANCE = 128;

constexpr u32 EPOLLIN = 0x001;
constexpr u32 EPOLLPRI = 0x002;
constexpr u32 EPOLLOUT = 0x004;
constexpr u32 EPOLLRDNORM = 0x040;
constexpr u32 EPOLLRDBAND = 0x080;
constexpr u32 EPOLLWRNORM = 0x100;
constexpr u32 EPOLLWRBAND = 0x200;
constexpr u32 EPOLLMSG = 0x400;
constexpr u32 EPOLLERR = 0x008;
constexpr u32 EPOLLHUP = 0x010;
constexpr u32 EPOLLRDHUP = 0x2000;
constexpr u32 EPOLLEXCLUSIVE = 1u << 28;
constexpr u32 EPOLLWAKEUP = 1u << 29;
constexpr u32 EPOLLONESHOT = 1u << 30;
constexpr u32 EPOLLET = 1u << 31;

constexpr i32 EPOLL_CTL_ADD = 1;
constexpr i32 EPOLL_CTL_DEL = 2;
constexpr i32 EPOLL_CTL_MOD = 3;

constexpr i32 EPOLL_CLOEXEC = 02000000;

struct EpollData {
    union {
        void* ptr;
        i32 fd;
        u32 u32;
        u64 u64;
    };
};

struct EpollEvent {
    u32 events;
    EpollData data;
} __attribute__((packed));

struct EpollFdEntry {
    i32 fd;
    u32 events;
    EpollData data;
    bool active;
};

struct EpollInstance {
    u32 id;
    EpollFdEntry fds[MAX_EPOLL_FDS_PER_INSTANCE];
    u32 fdCount;
    u32 flags;
    u32 ownerPid;
    bool active;
};

class Epoll {
public:
    static void initialize();
    
    static i32 create(i32 flags);
    static i32 create1(i32 flags);
    static i32 ctl(i32 epfd, i32 op, i32 fd, EpollEvent* event);
    static i32 wait(i32 epfd, EpollEvent* events, i32 maxevents, i32 timeout);
    static i32 pwait(i32 epfd, EpollEvent* events, i32 maxevents, i32 timeout, const u64* sigmask);
    static i32 close(i32 epfd);
    
    static bool isInitialized();

private:
    static EpollInstance* getInstance(i32 epfd);
    static i32 allocateInstance();
    static void freeInstance(i32 epfd);
    static u32 checkFdEvents(i32 fd);
    
    static EpollInstance sInstances[MAX_EPOLL_INSTANCES];
    static u32 sInstanceCount;
    static bool sInitialized;
};

constexpr i16 POLLIN = 0x0001;
constexpr i16 POLLPRI = 0x0002;
constexpr i16 POLLOUT = 0x0004;
constexpr i16 POLLERR = 0x0008;
constexpr i16 POLLHUP = 0x0010;
constexpr i16 POLLNVAL = 0x0020;
constexpr i16 POLLRDNORM = 0x0040;
constexpr i16 POLLRDBAND = 0x0080;
constexpr i16 POLLWRNORM = 0x0100;
constexpr i16 POLLWRBAND = 0x0200;

struct PollFd {
    i32 fd;
    i16 events;
    i16 revents;
};

class Poll {
public:
    static void initialize();
    
    static i32 poll(PollFd* fds, u32 nfds, i32 timeout);
    static i32 ppoll(PollFd* fds, u32 nfds, const u64* timeout, const u64* sigmask);
    
    static bool isInitialized();

private:
    static i16 checkFdEvents(i32 fd, i16 events);
    
    static bool sInitialized;
};

struct FdSet {
    u64 bits[16];
    
    void set(i32 fd) {
        if (fd >= 0 && fd < 1024) {
            bits[fd / 64] |= (1ULL << (fd % 64));
        }
    }
    
    void clear(i32 fd) {
        if (fd >= 0 && fd < 1024) {
            bits[fd / 64] &= ~(1ULL << (fd % 64));
        }
    }
    
    bool isSet(i32 fd) const {
        if (fd >= 0 && fd < 1024) {
            return (bits[fd / 64] & (1ULL << (fd % 64))) != 0;
        }
        return false;
    }
    
    void zero() {
        for (int i = 0; i < 16; i++) {
            bits[i] = 0;
        }
    }
};

class Select {
public:
    static void initialize();
    
    static i32 select(i32 nfds, FdSet* readfds, FdSet* writefds, FdSet* exceptfds, i64 timeout);
    static i32 pselect(i32 nfds, FdSet* readfds, FdSet* writefds, FdSet* exceptfds,
                       const u64* timeout, const u64* sigmask);
    
    static bool isInitialized();

private:
    static bool sInitialized;
};

constexpr i32 EFD_CLOEXEC = 02000000;
constexpr i32 EFD_NONBLOCK = 04000;
constexpr i32 EFD_SEMAPHORE = 1;

struct EventFd {
    u32 id;
    u64 counter;
    u32 flags;
    u32 ownerPid;
    bool active;
};

class EventFdManager {
public:
    static void initialize();
    static i32 create(u32 initval, i32 flags);
    static i64 read(i32 fd, u64* value);
    static i64 write(i32 fd, u64 value);
    static i32 close(i32 fd);
    static bool isInitialized();

private:
    static EventFd sEventFds[64];
    static u32 sCount;
    static bool sInitialized;
};

struct TimeSpec {
    i64 tv_sec;
    i64 tv_nsec;
};

struct ITimerSpec {
    TimeSpec it_interval;
    TimeSpec it_value;
};

constexpr i32 TFD_CLOEXEC = 02000000;
constexpr i32 TFD_NONBLOCK = 04000;
constexpr i32 TFD_TIMER_ABSTIME = 1;

struct TimerFd {
    u32 id;
    i32 clockid;
    ITimerSpec setting;
    u64 expirations;
    u64 nextExpiry;
    u32 flags;
    u32 ownerPid;
    bool active;
};

class TimerFdManager {
public:
    static void initialize();
    static i32 create(i32 clockid, i32 flags);
    static i32 settime(i32 fd, i32 flags, const ITimerSpec* newValue, ITimerSpec* oldValue);
    static i32 gettime(i32 fd, ITimerSpec* currValue);
    static i32 close(i32 fd);
    static bool isInitialized();

private:
    static TimerFd sTimerFds[64];
    static u32 sCount;
    static bool sInitialized;
};

constexpr i32 SFD_CLOEXEC = 02000000;
constexpr i32 SFD_NONBLOCK = 04000;

struct SignalFdInfo {
    u32 signo;
    i32 errno_;
    i32 code;
    u32 pid;
    u32 uid;
    i32 fd;
    u32 tid;
    u32 band;
    u32 overrun;
    u32 trapno;
    i32 status;
    i32 int_;
    u64 ptr;
    u64 utime;
    u64 stime;
    u64 addr;
    u8 pad[48];
};

struct SignalFd {
    u32 id;
    u64 mask;
    u32 flags;
    u32 ownerPid;
    bool active;
};

class SignalFdManager {
public:
    static void initialize();
    static i32 create(i32 fd, const u64* mask, i32 flags);
    static i64 read(i32 fd, SignalFdInfo* info, usize count);
    static i32 close(i32 fd);
    static bool isInitialized();

private:
    static SignalFd sSignalFds[64];
    static u32 sCount;
    static bool sInitialized;
};

}
