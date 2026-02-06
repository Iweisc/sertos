#pragma once

#include "../types.hpp"
#include "../process/thread.hpp"

namespace sertos::sync {

constexpr u32 MAX_FUTEX_WAITERS = 256;
constexpr u32 FUTEX_HASH_SIZE = 64;

constexpr i32 FUTEX_WAIT = 0;
constexpr i32 FUTEX_WAKE = 1;
constexpr i32 FUTEX_FD = 2;
constexpr i32 FUTEX_REQUEUE = 3;
constexpr i32 FUTEX_CMP_REQUEUE = 4;
constexpr i32 FUTEX_WAKE_OP = 5;
constexpr i32 FUTEX_LOCK_PI = 6;
constexpr i32 FUTEX_UNLOCK_PI = 7;
constexpr i32 FUTEX_TRYLOCK_PI = 8;
constexpr i32 FUTEX_WAIT_BITSET = 9;
constexpr i32 FUTEX_WAKE_BITSET = 10;
constexpr i32 FUTEX_WAIT_REQUEUE_PI = 11;
constexpr i32 FUTEX_CMP_REQUEUE_PI = 12;

constexpr i32 FUTEX_PRIVATE_FLAG = 128;
constexpr i32 FUTEX_CLOCK_REALTIME = 256;

constexpr u32 FUTEX_BITSET_MATCH_ANY = 0xFFFFFFFF;

struct FutexWaiter {
    process::Thread* thread;
    u64 address;
    u32 bitset;
    u64 timeout;
    bool active;
    FutexWaiter* next;
    FutexWaiter* prev;
};

struct FutexBucket {
    FutexWaiter* head;
    FutexWaiter* tail;
    u32 count;
};

class Futex {
public:
    static void initialize();
    
    static i64 futex(u32* uaddr, i32 op, u32 val, const u64* timeout, u32* uaddr2, u32 val3);
    
    static i64 wait(u32* uaddr, u32 val, u64 timeout, u32 bitset);
    static i64 wake(u32* uaddr, u32 count, u32 bitset);
    static i64 requeue(u32* uaddr, u32 count, u32* uaddr2, u32 count2, u32 cmpVal, bool cmp);
    static i64 wakeOp(u32* uaddr, u32* uaddr2, u32 count, u32 count2, u32 op);
    
    static void wakeupThread(process::Thread* thread);
    static void removeWaiter(FutexWaiter* waiter);
    
    static bool isInitialized();

private:
    static u32 hashAddress(u64 addr);
    static FutexBucket* getBucket(u64 addr);
    static FutexWaiter* allocateWaiter();
    static void freeWaiter(FutexWaiter* waiter);
    static void addWaiter(FutexBucket* bucket, FutexWaiter* waiter);
    static void removeWaiterFromBucket(FutexBucket* bucket, FutexWaiter* waiter);
    
    static FutexWaiter sWaiters[MAX_FUTEX_WAITERS];
    static FutexBucket sBuckets[FUTEX_HASH_SIZE];
    static bool sInitialized;
};

}
