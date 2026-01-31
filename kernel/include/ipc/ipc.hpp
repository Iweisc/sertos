#pragma once

#include "../types.hpp"
#include "../process/process.hpp"

namespace sertos::ipc {

constexpr u32 MAX_PIPES = 128;
constexpr u32 MAX_SHARED_MEMORY = 64;
constexpr u32 MAX_MESSAGE_QUEUES = 32;
constexpr u32 MAX_MESSAGES_PER_QUEUE = 256;
constexpr u32 PIPE_BUFFER_SIZE = 4096;
constexpr u32 MAX_MESSAGE_SIZE = 1024;
constexpr u32 MAX_SIGNAL_HANDLERS = 32;

constexpr i32 SIGKILL = 9;
constexpr i32 SIGTERM = 15;
constexpr i32 SIGSTOP = 19;
constexpr i32 SIGCONT = 18;
constexpr i32 SIGCHLD = 17;
constexpr i32 SIGUSR1 = 10;
constexpr i32 SIGUSR2 = 12;
constexpr i32 SIGALRM = 14;
constexpr i32 SIGINT = 2;
constexpr i32 SIGQUIT = 3;
constexpr i32 SIGPIPE = 13;
constexpr i32 SIGSEGV = 11;
constexpr i32 SIGBUS = 7;
constexpr i32 SIGFPE = 8;
constexpr i32 SIGILL = 4;
constexpr i32 SIGHUP = 1;
constexpr i32 MAX_SIGNALS = 32;

constexpr u64 SIG_DFL = 0;
constexpr u64 SIG_IGN = 1;

enum class PipeState : u8 {
    Invalid = 0,
    Open,
    ReadClosed,
    WriteClosed,
    Closed
};

struct Pipe {
    u32 id;
    PipeState state;
    u8 buffer[PIPE_BUFFER_SIZE];
    u32 readPos;
    u32 writePos;
    u32 dataSize;
    u32 readerPid;
    u32 writerPid;
    bool blocking;
};

struct PipeFd {
    u32 pipeId;
    bool isReadEnd;
    bool valid;
};

enum class SharedMemoryState : u8 {
    Invalid = 0,
    Active,
    MarkedForDeletion
};

struct SharedMemory {
    u32 id;
    u32 key;
    SharedMemoryState state;
    u64 physicalAddress;
    u64 size;
    u32 ownerUid;
    u32 permissions;
    u32 attachCount;
    u32 creatorPid;
};

struct SharedMemoryAttachment {
    u32 shmId;
    u32 pid;
    u64 virtualAddress;
    bool valid;
};

struct Message {
    u32 type;
    u32 size;
    u8 data[MAX_MESSAGE_SIZE];
    u32 senderPid;
    u64 timestamp;
};

enum class MessageQueueState : u8 {
    Invalid = 0,
    Active,
    MarkedForDeletion
};

struct MessageQueue {
    u32 id;
    u32 key;
    MessageQueueState state;
    Message messages[MAX_MESSAGES_PER_QUEUE];
    u32 head;
    u32 tail;
    u32 count;
    u32 maxMessages;
    u32 ownerUid;
    u32 permissions;
    u32 creatorPid;
};

using SignalHandler = void (*)(i32);

struct SignalAction {
    u64 handler;
    u32 flags;
    u64 mask;
};

struct PendingSignal {
    i32 signum;
    u32 senderPid;
    bool pending;
};

struct ProcessSignals {
    SignalAction actions[MAX_SIGNALS];
    PendingSignal pending[MAX_SIGNALS];
    u64 blocked;
    u64 pendingMask;
};

class PipeManager {
public:
    static void initialize();
    
    static bool createPipe(i32* readFd, i32* writeFd);
    static i64 read(i32 fd, void* buffer, usize size);
    static i64 write(i32 fd, const void* buffer, usize size);
    static bool close(i32 fd);
    
    static bool setBlocking(i32 fd, bool blocking);
    static bool isReadable(i32 fd);
    static bool isWritable(i32 fd);
    
    static bool isInitialized();

private:
    static Pipe* getPipe(u32 id);
    static PipeFd* getFd(i32 fd);
    static u32 allocatePipeId();
    static i32 allocateFd(u32 pipeId, bool isReadEnd);
    
    static Pipe sPipes[MAX_PIPES];
    static PipeFd sFds[MAX_PIPES * 2];
    static u32 sNextPipeId;
    static bool sInitialized;
};

class SharedMemoryManager {
public:
    static void initialize();
    
    static i32 create(u32 key, usize size, u32 permissions);
    static i32 get(u32 key);
    static void* attach(i32 shmId, u64 preferredAddr);
    static bool detach(void* addr);
    static bool remove(i32 shmId);
    
    static SharedMemory* getInfo(i32 shmId);
    static bool isInitialized();

private:
    static SharedMemory* findByKey(u32 key);
    static SharedMemory* findById(i32 id);
    static SharedMemoryAttachment* findAttachment(u32 pid, u64 addr);
    static u32 allocateId();
    
    static SharedMemory sSegments[MAX_SHARED_MEMORY];
    static SharedMemoryAttachment sAttachments[MAX_SHARED_MEMORY * 4];
    static u32 sNextId;
    static bool sInitialized;
};

class MessageQueueManager {
public:
    static void initialize();
    
    static i32 create(u32 key, u32 permissions);
    static i32 get(u32 key);
    static bool send(i32 mqId, const void* data, usize size, u32 type);
    static i64 receive(i32 mqId, void* buffer, usize maxSize, u32 type);
    static bool remove(i32 mqId);
    
    static MessageQueue* getInfo(i32 mqId);
    static u32 messageCount(i32 mqId);
    static bool isInitialized();

private:
    static MessageQueue* findByKey(u32 key);
    static MessageQueue* findById(i32 id);
    static u32 allocateId();
    
    static MessageQueue sQueues[MAX_MESSAGE_QUEUES];
    static u32 sNextId;
    static bool sInitialized;
};

class SignalManager {
public:
    static void initialize();
    
    static bool send(u32 targetPid, i32 signum);
    static bool sendFromProcess(u32 senderPid, u32 targetPid, i32 signum);
    
    static bool setHandler(u32 pid, i32 signum, u64 handler, u32 flags);
    static SignalAction* getHandler(u32 pid, i32 signum);
    
    static bool block(u32 pid, i32 signum);
    static bool unblock(u32 pid, i32 signum);
    static bool isBlocked(u32 pid, i32 signum);
    
    static bool hasPending(u32 pid);
    static i32 getNextPending(u32 pid);
    static void clearPending(u32 pid, i32 signum);
    
    static void deliverSignals(process::Process* proc);
    static ProcessSignals* getProcessSignals(u32 pid);
    
    static bool isInitialized();

private:
    static void defaultHandler(process::Process* proc, i32 signum);
    static bool canSendSignal(u32 senderPid, u32 targetPid, i32 signum);
    
    static ProcessSignals sProcessSignals[process::MAX_PROCESSES];
    static bool sInitialized;
};

class IPC {
public:
    static void initialize();
    static bool isInitialized();
};

}
