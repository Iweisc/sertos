#include "../../include/sync/futex.hpp"
#include "../../include/process/scheduler.hpp"

namespace sertos::sync {

FutexWaiter Futex::sWaiters[MAX_FUTEX_WAITERS];
FutexBucket Futex::sBuckets[FUTEX_HASH_SIZE];
bool Futex::sInitialized = false;

void Futex::initialize() {
    for (u32 i = 0; i < MAX_FUTEX_WAITERS; i++) {
        sWaiters[i].active = false;
        sWaiters[i].thread = nullptr;
        sWaiters[i].next = nullptr;
        sWaiters[i].prev = nullptr;
    }
    
    for (u32 i = 0; i < FUTEX_HASH_SIZE; i++) {
        sBuckets[i].head = nullptr;
        sBuckets[i].tail = nullptr;
        sBuckets[i].count = 0;
    }
    
    sInitialized = true;
}

i64 Futex::futex(u32* uaddr, i32 op, u32 val, const u64* timeout, u32* uaddr2, u32 val3) {
    i32 cmd = op & 0x7F;
    
    switch (cmd) {
        case FUTEX_WAIT:
            return wait(uaddr, val, timeout ? *timeout : 0, FUTEX_BITSET_MATCH_ANY);
            
        case FUTEX_WAKE:
            return wake(uaddr, val, FUTEX_BITSET_MATCH_ANY);
            
        case FUTEX_WAIT_BITSET:
            return wait(uaddr, val, timeout ? *timeout : 0, val3);
            
        case FUTEX_WAKE_BITSET:
            return wake(uaddr, val, val3);
            
        case FUTEX_REQUEUE:
            return requeue(uaddr, val, uaddr2, val3, 0, false);
            
        case FUTEX_CMP_REQUEUE:
            return requeue(uaddr, val, uaddr2, val3, val3, true);
            
        case FUTEX_WAKE_OP:
            return wakeOp(uaddr, uaddr2, val, val3, val3);
            
        default:
            return -1;
    }
}

i64 Futex::wait(u32* uaddr, u32 val, u64 timeout, u32 bitset) {
    if (!uaddr) return -1;
    
    if (*uaddr != val) {
        return -1;
    }
    
    process::Thread* currentThread = process::ThreadManager::currentThread();
    if (!currentThread) return -1;
    
    FutexWaiter* waiter = allocateWaiter();
    if (!waiter) return -1;
    
    waiter->thread = currentThread;
    waiter->address = reinterpret_cast<u64>(uaddr);
    waiter->bitset = bitset;
    waiter->timeout = timeout;
    waiter->active = true;
    
    FutexBucket* bucket = getBucket(reinterpret_cast<u64>(uaddr));
    addWaiter(bucket, waiter);
    
    currentThread->state = process::ThreadState::Blocked;
    
    u64 startTime = process::Scheduler::systemTime();
    u64 endTime = (timeout == 0) ? 0xFFFFFFFFFFFFFFFFULL : startTime + timeout;
    
    while (waiter->active && currentThread->state == process::ThreadState::Blocked) {
        if (timeout > 0 && process::Scheduler::systemTime() >= endTime) {
            removeWaiter(waiter);
            return -1;
        }
        process::Scheduler::yield();
    }
    
    return 0;
}

i64 Futex::wake(u32* uaddr, u32 count, u32 bitset) {
    if (!uaddr) return -1;
    
    FutexBucket* bucket = getBucket(reinterpret_cast<u64>(uaddr));
    u32 woken = 0;
    
    FutexWaiter* waiter = bucket->head;
    while (waiter && woken < count) {
        FutexWaiter* next = waiter->next;
        
        if (waiter->active && 
            waiter->address == reinterpret_cast<u64>(uaddr) &&
            (waiter->bitset & bitset)) {
            
            wakeupThread(waiter->thread);
            removeWaiterFromBucket(bucket, waiter);
            freeWaiter(waiter);
            woken++;
        }
        
        waiter = next;
    }
    
    return static_cast<i64>(woken);
}

i64 Futex::requeue(u32* uaddr, u32 count, u32* uaddr2, u32 count2, u32 cmpVal, bool cmp) {
    if (!uaddr || !uaddr2) return -1;
    
    if (cmp && *uaddr != cmpVal) {
        return -1;
    }
    
    FutexBucket* srcBucket = getBucket(reinterpret_cast<u64>(uaddr));
    FutexBucket* dstBucket = getBucket(reinterpret_cast<u64>(uaddr2));
    
    u32 woken = 0;
    u32 requeued = 0;
    
    FutexWaiter* waiter = srcBucket->head;
    while (waiter && (woken < count || requeued < count2)) {
        FutexWaiter* next = waiter->next;
        
        if (waiter->active && waiter->address == reinterpret_cast<u64>(uaddr)) {
            if (woken < count) {
                wakeupThread(waiter->thread);
                removeWaiterFromBucket(srcBucket, waiter);
                freeWaiter(waiter);
                woken++;
            } else if (requeued < count2) {
                removeWaiterFromBucket(srcBucket, waiter);
                waiter->address = reinterpret_cast<u64>(uaddr2);
                addWaiter(dstBucket, waiter);
                requeued++;
            }
        }
        
        waiter = next;
    }
    
    return static_cast<i64>(woken);
}

i64 Futex::wakeOp(u32* uaddr, u32* uaddr2, u32 count, u32 count2, u32 op) {
    (void)op;
    
    i64 woken1 = wake(uaddr, count, FUTEX_BITSET_MATCH_ANY);
    i64 woken2 = wake(uaddr2, count2, FUTEX_BITSET_MATCH_ANY);
    
    return woken1 + woken2;
}

void Futex::wakeupThread(process::Thread* thread) {
    if (thread) {
        thread->state = process::ThreadState::Ready;
    }
}

void Futex::removeWaiter(FutexWaiter* waiter) {
    if (!waiter) return;
    
    FutexBucket* bucket = getBucket(waiter->address);
    removeWaiterFromBucket(bucket, waiter);
    freeWaiter(waiter);
}

bool Futex::isInitialized() {
    return sInitialized;
}

u32 Futex::hashAddress(u64 addr) {
    return static_cast<u32>((addr >> 2) % FUTEX_HASH_SIZE);
}

FutexBucket* Futex::getBucket(u64 addr) {
    return &sBuckets[hashAddress(addr)];
}

FutexWaiter* Futex::allocateWaiter() {
    for (u32 i = 0; i < MAX_FUTEX_WAITERS; i++) {
        if (!sWaiters[i].active) {
            sWaiters[i].active = true;
            sWaiters[i].next = nullptr;
            sWaiters[i].prev = nullptr;
            return &sWaiters[i];
        }
    }
    return nullptr;
}

void Futex::freeWaiter(FutexWaiter* waiter) {
    if (waiter) {
        waiter->active = false;
        waiter->thread = nullptr;
        waiter->next = nullptr;
        waiter->prev = nullptr;
    }
}

void Futex::addWaiter(FutexBucket* bucket, FutexWaiter* waiter) {
    if (!bucket || !waiter) return;
    
    waiter->next = nullptr;
    waiter->prev = bucket->tail;
    
    if (bucket->tail) {
        bucket->tail->next = waiter;
    } else {
        bucket->head = waiter;
    }
    
    bucket->tail = waiter;
    bucket->count++;
}

void Futex::removeWaiterFromBucket(FutexBucket* bucket, FutexWaiter* waiter) {
    if (!bucket || !waiter) return;
    
    if (waiter->prev) {
        waiter->prev->next = waiter->next;
    } else {
        bucket->head = waiter->next;
    }
    
    if (waiter->next) {
        waiter->next->prev = waiter->prev;
    } else {
        bucket->tail = waiter->prev;
    }
    
    bucket->count--;
    waiter->next = nullptr;
    waiter->prev = nullptr;
}

}
