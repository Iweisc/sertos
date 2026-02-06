#include "../../include/io/epoll.hpp"
#include "../../include/process/process.hpp"
#include "../../include/process/scheduler.hpp"

namespace sertos::io {

EpollInstance Epoll::sInstances[MAX_EPOLL_INSTANCES];
u32 Epoll::sInstanceCount = 0;
bool Epoll::sInitialized = false;

void Epoll::initialize() {
    for (u32 i = 0; i < MAX_EPOLL_INSTANCES; i++) {
        sInstances[i].active = false;
        sInstances[i].fdCount = 0;
    }
    sInstanceCount = 0;
    sInitialized = true;
}

i32 Epoll::create(i32 flags) {
    i32 epfd = allocateInstance();
    if (epfd < 0) return -1;
    
    EpollInstance* inst = &sInstances[epfd];
    inst->flags = static_cast<u32>(flags);
    
    process::Process* current = process::PM::currentProcess();
    if (current) {
        inst->ownerPid = current->pid;
    }
    
    return epfd;
}

i32 Epoll::create1(i32 flags) {
    return create(flags);
}

i32 Epoll::ctl(i32 epfd, i32 op, i32 fd, EpollEvent* event) {
    EpollInstance* inst = getInstance(epfd);
    if (!inst) return -1;
    
    switch (op) {
        case EPOLL_CTL_ADD: {
            if (inst->fdCount >= MAX_EPOLL_FDS_PER_INSTANCE) return -1;
            
            for (u32 i = 0; i < MAX_EPOLL_FDS_PER_INSTANCE; i++) {
                if (inst->fds[i].active && inst->fds[i].fd == fd) {
                    return -1;
                }
            }
            
            for (u32 i = 0; i < MAX_EPOLL_FDS_PER_INSTANCE; i++) {
                if (!inst->fds[i].active) {
                    inst->fds[i].fd = fd;
                    inst->fds[i].events = event ? event->events : 0;
                    inst->fds[i].data = event ? event->data : EpollData{};
                    inst->fds[i].active = true;
                    inst->fdCount++;
                    return 0;
                }
            }
            return -1;
        }
        
        case EPOLL_CTL_DEL: {
            for (u32 i = 0; i < MAX_EPOLL_FDS_PER_INSTANCE; i++) {
                if (inst->fds[i].active && inst->fds[i].fd == fd) {
                    inst->fds[i].active = false;
                    inst->fdCount--;
                    return 0;
                }
            }
            return -1;
        }
        
        case EPOLL_CTL_MOD: {
            for (u32 i = 0; i < MAX_EPOLL_FDS_PER_INSTANCE; i++) {
                if (inst->fds[i].active && inst->fds[i].fd == fd) {
                    inst->fds[i].events = event ? event->events : 0;
                    inst->fds[i].data = event ? event->data : EpollData{};
                    return 0;
                }
            }
            return -1;
        }
        
        default:
            return -1;
    }
}

i32 Epoll::wait(i32 epfd, EpollEvent* events, i32 maxevents, i32 timeout) {
    EpollInstance* inst = getInstance(epfd);
    if (!inst || !events || maxevents <= 0) return -1;
    
    u64 startTime = process::Scheduler::systemTime();
    u64 endTime = (timeout < 0) ? 0xFFFFFFFFFFFFFFFFULL : startTime + static_cast<u64>(timeout);
    
    while (true) {
        i32 count = 0;
        
        for (u32 i = 0; i < MAX_EPOLL_FDS_PER_INSTANCE && count < maxevents; i++) {
            if (!inst->fds[i].active) continue;
            
            u32 revents = checkFdEvents(inst->fds[i].fd);
            u32 matched = revents & inst->fds[i].events;
            
            if (matched) {
                events[count].events = matched;
                events[count].data = inst->fds[i].data;
                count++;
            }
        }
        
        if (count > 0) return count;
        
        if (timeout == 0) return 0;
        
        if (process::Scheduler::systemTime() >= endTime) return 0;
        
        process::Scheduler::yield();
    }
}

i32 Epoll::pwait(i32 epfd, EpollEvent* events, i32 maxevents, i32 timeout, const u64* sigmask) {
    (void)sigmask;
    return wait(epfd, events, maxevents, timeout);
}

i32 Epoll::close(i32 epfd) {
    EpollInstance* inst = getInstance(epfd);
    if (!inst) return -1;
    
    inst->active = false;
    inst->fdCount = 0;
    sInstanceCount--;
    return 0;
}

bool Epoll::isInitialized() {
    return sInitialized;
}

EpollInstance* Epoll::getInstance(i32 epfd) {
    if (epfd < 0 || epfd >= static_cast<i32>(MAX_EPOLL_INSTANCES)) return nullptr;
    if (!sInstances[epfd].active) return nullptr;
    return &sInstances[epfd];
}

i32 Epoll::allocateInstance() {
    for (u32 i = 0; i < MAX_EPOLL_INSTANCES; i++) {
        if (!sInstances[i].active) {
            sInstances[i].active = true;
            sInstances[i].id = i;
            sInstances[i].fdCount = 0;
            sInstanceCount++;
            return static_cast<i32>(i);
        }
    }
    return -1;
}

void Epoll::freeInstance(i32 epfd) {
    if (epfd >= 0 && epfd < static_cast<i32>(MAX_EPOLL_INSTANCES)) {
        sInstances[epfd].active = false;
        sInstanceCount--;
    }
}

u32 Epoll::checkFdEvents(i32 fd) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return 0;
    
    if (fd < 0 || fd >= static_cast<i32>(process::MAX_FDS)) return 0;
    if (!current->fds[fd].valid) return EPOLLERR;
    
    u32 events = 0;
    
    if (current->fds[fd].type == process::FdType::Console) {
        events |= EPOLLOUT;
    } else if (current->fds[fd].type == process::FdType::Pipe) {
        events |= EPOLLIN | EPOLLOUT;
    } else if (current->fds[fd].type == process::FdType::File) {
        events |= EPOLLIN | EPOLLOUT;
    }
    
    return events;
}

bool Poll::sInitialized = false;

void Poll::initialize() {
    sInitialized = true;
}

i32 Poll::poll(PollFd* fds, u32 nfds, i32 timeout) {
    if (!fds) return -1;
    
    u64 startTime = process::Scheduler::systemTime();
    u64 endTime = (timeout < 0) ? 0xFFFFFFFFFFFFFFFFULL : startTime + static_cast<u64>(timeout);
    
    while (true) {
        i32 count = 0;
        
        for (u32 i = 0; i < nfds; i++) {
            fds[i].revents = checkFdEvents(fds[i].fd, fds[i].events);
            if (fds[i].revents) count++;
        }
        
        if (count > 0) return count;
        
        if (timeout == 0) return 0;
        
        if (process::Scheduler::systemTime() >= endTime) return 0;
        
        process::Scheduler::yield();
    }
}

i32 Poll::ppoll(PollFd* fds, u32 nfds, const u64* timeout, const u64* sigmask) {
    (void)sigmask;
    
    i32 timeoutMs = -1;
    if (timeout) {
        timeoutMs = static_cast<i32>(*timeout / 1000000);
    }
    
    return poll(fds, nfds, timeoutMs);
}

bool Poll::isInitialized() {
    return sInitialized;
}

i16 Poll::checkFdEvents(i32 fd, i16 events) {
    process::Process* current = process::PM::currentProcess();
    if (!current) return POLLNVAL;
    
    if (fd < 0 || fd >= static_cast<i32>(process::MAX_FDS)) return POLLNVAL;
    if (!current->fds[fd].valid) return POLLNVAL;
    
    i16 revents = 0;
    
    if (current->fds[fd].type == process::FdType::Console) {
        if (events & POLLOUT) revents |= POLLOUT;
    } else if (current->fds[fd].type == process::FdType::Pipe) {
        if (events & POLLIN) revents |= POLLIN;
        if (events & POLLOUT) revents |= POLLOUT;
    } else if (current->fds[fd].type == process::FdType::File) {
        if (events & POLLIN) revents |= POLLIN;
        if (events & POLLOUT) revents |= POLLOUT;
    }
    
    return revents;
}

bool Select::sInitialized = false;

void Select::initialize() {
    sInitialized = true;
}

i32 Select::select(i32 nfds, FdSet* readfds, FdSet* writefds, FdSet* exceptfds, i64 timeout) {
    u64 startTime = process::Scheduler::systemTime();
    u64 endTime = (timeout < 0) ? 0xFFFFFFFFFFFFFFFFULL : startTime + static_cast<u64>(timeout);
    
    FdSet readResult, writeResult, exceptResult;
    readResult.zero();
    writeResult.zero();
    exceptResult.zero();
    
    while (true) {
        i32 count = 0;
        
        process::Process* current = process::PM::currentProcess();
        if (!current) return -1;
        
        for (i32 fd = 0; fd < nfds; fd++) {
            if (fd >= static_cast<i32>(process::MAX_FDS)) break;
            if (!current->fds[fd].valid) continue;
            
            bool readable = false;
            bool writable = false;
            
            if (current->fds[fd].type == process::FdType::Console) {
                writable = true;
            } else if (current->fds[fd].type == process::FdType::Pipe) {
                readable = true;
                writable = true;
            } else if (current->fds[fd].type == process::FdType::File) {
                readable = true;
                writable = true;
            }
            
            if (readfds && readfds->isSet(fd) && readable) {
                readResult.set(fd);
                count++;
            }
            if (writefds && writefds->isSet(fd) && writable) {
                writeResult.set(fd);
                count++;
            }
        }
        
        if (count > 0) {
            if (readfds) *readfds = readResult;
            if (writefds) *writefds = writeResult;
            if (exceptfds) *exceptfds = exceptResult;
            return count;
        }
        
        if (timeout == 0) {
            if (readfds) readfds->zero();
            if (writefds) writefds->zero();
            if (exceptfds) exceptfds->zero();
            return 0;
        }
        
        if (process::Scheduler::systemTime() >= endTime) {
            if (readfds) readfds->zero();
            if (writefds) writefds->zero();
            if (exceptfds) exceptfds->zero();
            return 0;
        }
        
        process::Scheduler::yield();
    }
}

i32 Select::pselect(i32 nfds, FdSet* readfds, FdSet* writefds, FdSet* exceptfds, 
                   const u64* timeout, const u64* sigmask) {
    (void)sigmask;
    
    i64 timeoutMs = -1;
    if (timeout) {
        timeoutMs = static_cast<i64>(*timeout / 1000000);
    }
    
    return select(nfds, readfds, writefds, exceptfds, timeoutMs);
}

bool Select::isInitialized() {
    return sInitialized;
}

EventFd EventFdManager::sEventFds[64];
u32 EventFdManager::sCount = 0;
bool EventFdManager::sInitialized = false;

void EventFdManager::initialize() {
    for (u32 i = 0; i < 64; i++) {
        sEventFds[i].active = false;
    }
    sCount = 0;
    sInitialized = true;
}

i32 EventFdManager::create(u32 initval, i32 flags) {
    for (u32 i = 0; i < 64; i++) {
        if (!sEventFds[i].active) {
            sEventFds[i].id = i;
            sEventFds[i].counter = initval;
            sEventFds[i].flags = static_cast<u32>(flags);
            sEventFds[i].active = true;
            
            process::Process* current = process::PM::currentProcess();
            if (current) {
                sEventFds[i].ownerPid = current->pid;
            }
            
            sCount++;
            return static_cast<i32>(i + 1000);
        }
    }
    return -1;
}

i64 EventFdManager::read(i32 fd, u64* value) {
    i32 idx = fd - 1000;
    if (idx < 0 || idx >= 64 || !sEventFds[idx].active) return -1;
    
    if (sEventFds[idx].counter == 0) {
        if (sEventFds[idx].flags & EFD_NONBLOCK) return -1;
        while (sEventFds[idx].counter == 0) {
            process::Scheduler::yield();
        }
    }
    
    if (sEventFds[idx].flags & EFD_SEMAPHORE) {
        *value = 1;
        sEventFds[idx].counter--;
    } else {
        *value = sEventFds[idx].counter;
        sEventFds[idx].counter = 0;
    }
    
    return 8;
}

i64 EventFdManager::write(i32 fd, u64 value) {
    i32 idx = fd - 1000;
    if (idx < 0 || idx >= 64 || !sEventFds[idx].active) return -1;
    
    sEventFds[idx].counter += value;
    return 8;
}

i32 EventFdManager::close(i32 fd) {
    i32 idx = fd - 1000;
    if (idx < 0 || idx >= 64 || !sEventFds[idx].active) return -1;
    
    sEventFds[idx].active = false;
    sCount--;
    return 0;
}

bool EventFdManager::isInitialized() {
    return sInitialized;
}

TimerFd TimerFdManager::sTimerFds[64];
u32 TimerFdManager::sCount = 0;
bool TimerFdManager::sInitialized = false;

void TimerFdManager::initialize() {
    for (u32 i = 0; i < 64; i++) {
        sTimerFds[i].active = false;
    }
    sCount = 0;
    sInitialized = true;
}

i32 TimerFdManager::create(i32 clockid, i32 flags) {
    for (u32 i = 0; i < 64; i++) {
        if (!sTimerFds[i].active) {
            sTimerFds[i].id = i;
            sTimerFds[i].clockid = clockid;
            sTimerFds[i].flags = static_cast<u32>(flags);
            sTimerFds[i].expirations = 0;
            sTimerFds[i].nextExpiry = 0;
            sTimerFds[i].active = true;
            
            process::Process* current = process::PM::currentProcess();
            if (current) {
                sTimerFds[i].ownerPid = current->pid;
            }
            
            sCount++;
            return static_cast<i32>(i + 2000);
        }
    }
    return -1;
}

i32 TimerFdManager::settime(i32 fd, i32 flags, const ITimerSpec* newValue, ITimerSpec* oldValue) {
    i32 idx = fd - 2000;
    if (idx < 0 || idx >= 64 || !sTimerFds[idx].active) return -1;
    
    if (oldValue) {
        *oldValue = sTimerFds[idx].setting;
    }
    
    if (newValue) {
        sTimerFds[idx].setting = *newValue;
        
        u64 now = process::Scheduler::systemTime();
        u64 valueMs = static_cast<u64>(newValue->it_value.tv_sec * 1000 + 
                                        newValue->it_value.tv_nsec / 1000000);
        
        if (flags & TFD_TIMER_ABSTIME) {
            sTimerFds[idx].nextExpiry = valueMs;
        } else {
            sTimerFds[idx].nextExpiry = now + valueMs;
        }
    }
    
    return 0;
}

i32 TimerFdManager::gettime(i32 fd, ITimerSpec* currValue) {
    i32 idx = fd - 2000;
    if (idx < 0 || idx >= 64 || !sTimerFds[idx].active) return -1;
    
    if (currValue) {
        *currValue = sTimerFds[idx].setting;
    }
    
    return 0;
}

i32 TimerFdManager::close(i32 fd) {
    i32 idx = fd - 2000;
    if (idx < 0 || idx >= 64 || !sTimerFds[idx].active) return -1;
    
    sTimerFds[idx].active = false;
    sCount--;
    return 0;
}

bool TimerFdManager::isInitialized() {
    return sInitialized;
}

SignalFd SignalFdManager::sSignalFds[64];
u32 SignalFdManager::sCount = 0;
bool SignalFdManager::sInitialized = false;

void SignalFdManager::initialize() {
    for (u32 i = 0; i < 64; i++) {
        sSignalFds[i].active = false;
    }
    sCount = 0;
    sInitialized = true;
}

i32 SignalFdManager::create(i32 fd, const u64* mask, i32 flags) {
    if (fd >= 0) {
        i32 idx = fd - 3000;
        if (idx >= 0 && idx < 64 && sSignalFds[idx].active) {
            if (mask) sSignalFds[idx].mask = *mask;
            sSignalFds[idx].flags = static_cast<u32>(flags);
            return fd;
        }
        return -1;
    }
    
    for (u32 i = 0; i < 64; i++) {
        if (!sSignalFds[i].active) {
            sSignalFds[i].id = i;
            sSignalFds[i].mask = mask ? *mask : 0;
            sSignalFds[i].flags = static_cast<u32>(flags);
            sSignalFds[i].active = true;
            
            process::Process* current = process::PM::currentProcess();
            if (current) {
                sSignalFds[i].ownerPid = current->pid;
            }
            
            sCount++;
            return static_cast<i32>(i + 3000);
        }
    }
    return -1;
}

i64 SignalFdManager::read(i32 fd, SignalFdInfo* info, usize count) {
    i32 idx = fd - 3000;
    if (idx < 0 || idx >= 64 || !sSignalFds[idx].active) return -1;
    
    (void)info;
    (void)count;
    
    return 0;
}

i32 SignalFdManager::close(i32 fd) {
    i32 idx = fd - 3000;
    if (idx < 0 || idx >= 64 || !sSignalFds[idx].active) return -1;
    
    sSignalFds[idx].active = false;
    sCount--;
    return 0;
}

bool SignalFdManager::isInitialized() {
    return sInitialized;
}

}
