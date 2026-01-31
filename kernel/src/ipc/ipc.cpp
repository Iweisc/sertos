#include "../../include/ipc/ipc.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/memory/vmm.hpp"
#include "../../include/process/scheduler.hpp"

namespace sertos::ipc {

namespace {

void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

void memcpy(void* dest, const void* src, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    const u8* s = reinterpret_cast<const u8*>(src);
    for (usize i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

usize min(usize a, usize b) {
    return a < b ? a : b;
}

}

Pipe PipeManager::sPipes[MAX_PIPES];
PipeFd PipeManager::sFds[MAX_PIPES * 2];
u32 PipeManager::sNextPipeId = 1;
bool PipeManager::sInitialized = false;

void PipeManager::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_PIPES; i++) {
        memset(&sPipes[i], 0, sizeof(Pipe));
        sPipes[i].state = PipeState::Invalid;
    }
    
    for (u32 i = 0; i < MAX_PIPES * 2; i++) {
        sFds[i].valid = false;
    }
    
    sNextPipeId = 1;
    sInitialized = true;
}

bool PipeManager::createPipe(i32* readFd, i32* writeFd) {
    if (!sInitialized || !readFd || !writeFd) return false;
    
    Pipe* pipe = nullptr;
    for (u32 i = 0; i < MAX_PIPES; i++) {
        if (sPipes[i].state == PipeState::Invalid) {
            pipe = &sPipes[i];
            break;
        }
    }
    
    if (!pipe) return false;
    
    memset(pipe, 0, sizeof(Pipe));
    pipe->id = allocatePipeId();
    pipe->state = PipeState::Open;
    pipe->readPos = 0;
    pipe->writePos = 0;
    pipe->dataSize = 0;
    pipe->blocking = true;
    
    process::Process* current = process::PM::currentProcess();
    if (current) {
        pipe->readerPid = current->pid;
        pipe->writerPid = current->pid;
    }
    
    *readFd = allocateFd(pipe->id, true);
    *writeFd = allocateFd(pipe->id, false);
    
    if (*readFd < 0 || *writeFd < 0) {
        pipe->state = PipeState::Invalid;
        return false;
    }
    
    return true;
}

i64 PipeManager::read(i32 fd, void* buffer, usize size) {
    if (!sInitialized || !buffer || size == 0) return -1;
    
    PipeFd* pfd = getFd(fd);
    if (!pfd || !pfd->valid || !pfd->isReadEnd) return -1;
    
    Pipe* pipe = getPipe(pfd->pipeId);
    if (!pipe || pipe->state == PipeState::Invalid) return -1;
    
    if (pipe->state == PipeState::WriteClosed && pipe->dataSize == 0) {
        return 0;
    }
    
    while (pipe->dataSize == 0) {
        if (!pipe->blocking) return -1;
        if (pipe->state == PipeState::WriteClosed) return 0;
        process::Scheduler::yield();
    }
    
    usize toRead = min(size, pipe->dataSize);
    u8* buf = reinterpret_cast<u8*>(buffer);
    
    for (usize i = 0; i < toRead; i++) {
        buf[i] = pipe->buffer[pipe->readPos];
        pipe->readPos = (pipe->readPos + 1) % PIPE_BUFFER_SIZE;
    }
    
    pipe->dataSize -= toRead;
    
    return static_cast<i64>(toRead);
}

i64 PipeManager::write(i32 fd, const void* buffer, usize size) {
    if (!sInitialized || !buffer || size == 0) return -1;
    
    PipeFd* pfd = getFd(fd);
    if (!pfd || !pfd->valid || pfd->isReadEnd) return -1;
    
    Pipe* pipe = getPipe(pfd->pipeId);
    if (!pipe || pipe->state == PipeState::Invalid) return -1;
    
    if (pipe->state == PipeState::ReadClosed || pipe->state == PipeState::Closed) {
        process::Process* current = process::PM::currentProcess();
        if (current) {
            SignalManager::send(current->pid, SIGPIPE);
        }
        return -1;
    }
    
    const u8* buf = reinterpret_cast<const u8*>(buffer);
    usize written = 0;
    
    while (written < size) {
        while (pipe->dataSize >= PIPE_BUFFER_SIZE) {
            if (!pipe->blocking) return static_cast<i64>(written);
            if (pipe->state == PipeState::ReadClosed) return -1;
            process::Scheduler::yield();
        }
        
        usize spaceAvailable = PIPE_BUFFER_SIZE - pipe->dataSize;
        usize toWrite = min(size - written, spaceAvailable);
        
        for (usize i = 0; i < toWrite; i++) {
            pipe->buffer[pipe->writePos] = buf[written + i];
            pipe->writePos = (pipe->writePos + 1) % PIPE_BUFFER_SIZE;
        }
        
        pipe->dataSize += toWrite;
        written += toWrite;
    }
    
    return static_cast<i64>(written);
}

bool PipeManager::close(i32 fd) {
    if (!sInitialized) return false;
    
    PipeFd* pfd = getFd(fd);
    if (!pfd || !pfd->valid) return false;
    
    Pipe* pipe = getPipe(pfd->pipeId);
    if (!pipe) return false;
    
    if (pfd->isReadEnd) {
        if (pipe->state == PipeState::Open) {
            pipe->state = PipeState::ReadClosed;
        } else if (pipe->state == PipeState::WriteClosed) {
            pipe->state = PipeState::Closed;
        }
    } else {
        if (pipe->state == PipeState::Open) {
            pipe->state = PipeState::WriteClosed;
        } else if (pipe->state == PipeState::ReadClosed) {
            pipe->state = PipeState::Closed;
        }
    }
    
    pfd->valid = false;
    
    if (pipe->state == PipeState::Closed) {
        pipe->state = PipeState::Invalid;
    }
    
    return true;
}

bool PipeManager::setBlocking(i32 fd, bool blocking) {
    if (!sInitialized) return false;
    
    PipeFd* pfd = getFd(fd);
    if (!pfd || !pfd->valid) return false;
    
    Pipe* pipe = getPipe(pfd->pipeId);
    if (!pipe) return false;
    
    pipe->blocking = blocking;
    return true;
}

bool PipeManager::isReadable(i32 fd) {
    if (!sInitialized) return false;
    
    PipeFd* pfd = getFd(fd);
    if (!pfd || !pfd->valid || !pfd->isReadEnd) return false;
    
    Pipe* pipe = getPipe(pfd->pipeId);
    if (!pipe) return false;
    
    return pipe->dataSize > 0 || pipe->state == PipeState::WriteClosed;
}

bool PipeManager::isWritable(i32 fd) {
    if (!sInitialized) return false;
    
    PipeFd* pfd = getFd(fd);
    if (!pfd || !pfd->valid || pfd->isReadEnd) return false;
    
    Pipe* pipe = getPipe(pfd->pipeId);
    if (!pipe) return false;
    
    return pipe->dataSize < PIPE_BUFFER_SIZE && pipe->state != PipeState::ReadClosed;
}

bool PipeManager::isInitialized() {
    return sInitialized;
}

Pipe* PipeManager::getPipe(u32 id) {
    for (u32 i = 0; i < MAX_PIPES; i++) {
        if (sPipes[i].id == id && sPipes[i].state != PipeState::Invalid) {
            return &sPipes[i];
        }
    }
    return nullptr;
}

PipeFd* PipeManager::getFd(i32 fd) {
    if (fd < 0 || fd >= static_cast<i32>(MAX_PIPES * 2)) return nullptr;
    return &sFds[fd];
}

u32 PipeManager::allocatePipeId() {
    return sNextPipeId++;
}

i32 PipeManager::allocateFd(u32 pipeId, bool isReadEnd) {
    for (u32 i = 0; i < MAX_PIPES * 2; i++) {
        if (!sFds[i].valid) {
            sFds[i].pipeId = pipeId;
            sFds[i].isReadEnd = isReadEnd;
            sFds[i].valid = true;
            return static_cast<i32>(i);
        }
    }
    return -1;
}

SharedMemory SharedMemoryManager::sSegments[MAX_SHARED_MEMORY];
SharedMemoryAttachment SharedMemoryManager::sAttachments[MAX_SHARED_MEMORY * 4];
u32 SharedMemoryManager::sNextId = 1;
bool SharedMemoryManager::sInitialized = false;

void SharedMemoryManager::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_SHARED_MEMORY; i++) {
        memset(&sSegments[i], 0, sizeof(SharedMemory));
        sSegments[i].state = SharedMemoryState::Invalid;
    }
    
    for (u32 i = 0; i < MAX_SHARED_MEMORY * 4; i++) {
        sAttachments[i].valid = false;
    }
    
    sNextId = 1;
    sInitialized = true;
}

i32 SharedMemoryManager::create(u32 key, usize size, u32 permissions) {
    if (!sInitialized || size == 0) return -1;
    
    if (key != 0 && findByKey(key)) return -1;
    
    SharedMemory* shm = nullptr;
    for (u32 i = 0; i < MAX_SHARED_MEMORY; i++) {
        if (sSegments[i].state == SharedMemoryState::Invalid) {
            shm = &sSegments[i];
            break;
        }
    }
    
    if (!shm) return -1;
    
    usize pages = (size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
    void* physMem = memory::PMM::allocatePages(pages);
    if (!physMem) return -1;
    
    memset(physMem, 0, pages * memory::PAGE_SIZE);
    
    shm->id = allocateId();
    shm->key = key;
    shm->state = SharedMemoryState::Active;
    shm->physicalAddress = reinterpret_cast<u64>(physMem);
    shm->size = pages * memory::PAGE_SIZE;
    shm->permissions = permissions;
    shm->attachCount = 0;
    
    process::Process* current = process::PM::currentProcess();
    if (current) {
        shm->ownerUid = 0;
        shm->creatorPid = current->pid;
    }
    
    return static_cast<i32>(shm->id);
}

i32 SharedMemoryManager::get(u32 key) {
    if (!sInitialized || key == 0) return -1;
    
    SharedMemory* shm = findByKey(key);
    if (!shm) return -1;
    
    return static_cast<i32>(shm->id);
}

void* SharedMemoryManager::attach(i32 shmId, u64 preferredAddr) {
    if (!sInitialized) return nullptr;
    
    SharedMemory* shm = findById(shmId);
    if (!shm || shm->state != SharedMemoryState::Active) return nullptr;
    
    process::Process* current = process::PM::currentProcess();
    if (!current) return nullptr;
    
    SharedMemoryAttachment* att = nullptr;
    for (u32 i = 0; i < MAX_SHARED_MEMORY * 4; i++) {
        if (!sAttachments[i].valid) {
            att = &sAttachments[i];
            break;
        }
    }
    
    if (!att) return nullptr;
    
    u64 virtAddr = preferredAddr;
    if (virtAddr == 0) {
        virtAddr = 0x700000000000ULL + (shm->id * 0x10000000ULL);
    }
    
    usize pages = shm->size / memory::PAGE_SIZE;
    u64 flags = memory::PAGE_PRESENT | memory::PAGE_USER;
    if (shm->permissions & 0x2) {
        flags |= memory::PAGE_WRITABLE;
    }
    
    for (usize i = 0; i < pages; i++) {
        memory::VMM::mapPageIn(
            current->pageTable,
            virtAddr + i * memory::PAGE_SIZE,
            shm->physicalAddress + i * memory::PAGE_SIZE,
            flags
        );
    }
    
    att->shmId = shm->id;
    att->pid = current->pid;
    att->virtualAddress = virtAddr;
    att->valid = true;
    
    shm->attachCount++;
    
    return reinterpret_cast<void*>(virtAddr);
}

bool SharedMemoryManager::detach(void* addr) {
    if (!sInitialized || !addr) return false;
    
    process::Process* current = process::PM::currentProcess();
    if (!current) return false;
    
    SharedMemoryAttachment* att = findAttachment(current->pid, reinterpret_cast<u64>(addr));
    if (!att) return false;
    
    SharedMemory* shm = findById(static_cast<i32>(att->shmId));
    if (!shm) return false;
    
    usize pages = shm->size / memory::PAGE_SIZE;
    for (usize i = 0; i < pages; i++) {
        memory::VMM::unmapPageIn(current->pageTable, att->virtualAddress + i * memory::PAGE_SIZE);
    }
    
    att->valid = false;
    shm->attachCount--;
    
    if (shm->state == SharedMemoryState::MarkedForDeletion && shm->attachCount == 0) {
        memory::PMM::freePages(
            reinterpret_cast<void*>(shm->physicalAddress),
            shm->size / memory::PAGE_SIZE
        );
        shm->state = SharedMemoryState::Invalid;
    }
    
    return true;
}

bool SharedMemoryManager::remove(i32 shmId) {
    if (!sInitialized) return false;
    
    SharedMemory* shm = findById(shmId);
    if (!shm || shm->state != SharedMemoryState::Active) return false;
    
    if (shm->attachCount > 0) {
        shm->state = SharedMemoryState::MarkedForDeletion;
    } else {
        memory::PMM::freePages(
            reinterpret_cast<void*>(shm->physicalAddress),
            shm->size / memory::PAGE_SIZE
        );
        shm->state = SharedMemoryState::Invalid;
    }
    
    return true;
}

SharedMemory* SharedMemoryManager::getInfo(i32 shmId) {
    return findById(shmId);
}

bool SharedMemoryManager::isInitialized() {
    return sInitialized;
}

SharedMemory* SharedMemoryManager::findByKey(u32 key) {
    for (u32 i = 0; i < MAX_SHARED_MEMORY; i++) {
        if (sSegments[i].state != SharedMemoryState::Invalid && sSegments[i].key == key) {
            return &sSegments[i];
        }
    }
    return nullptr;
}

SharedMemory* SharedMemoryManager::findById(i32 id) {
    for (u32 i = 0; i < MAX_SHARED_MEMORY; i++) {
        if (sSegments[i].state != SharedMemoryState::Invalid && 
            static_cast<i32>(sSegments[i].id) == id) {
            return &sSegments[i];
        }
    }
    return nullptr;
}

SharedMemoryAttachment* SharedMemoryManager::findAttachment(u32 pid, u64 addr) {
    for (u32 i = 0; i < MAX_SHARED_MEMORY * 4; i++) {
        if (sAttachments[i].valid && 
            sAttachments[i].pid == pid && 
            sAttachments[i].virtualAddress == addr) {
            return &sAttachments[i];
        }
    }
    return nullptr;
}

u32 SharedMemoryManager::allocateId() {
    return sNextId++;
}

MessageQueue MessageQueueManager::sQueues[MAX_MESSAGE_QUEUES];
u32 MessageQueueManager::sNextId = 1;
bool MessageQueueManager::sInitialized = false;

void MessageQueueManager::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        memset(&sQueues[i], 0, sizeof(MessageQueue));
        sQueues[i].state = MessageQueueState::Invalid;
    }
    
    sNextId = 1;
    sInitialized = true;
}

i32 MessageQueueManager::create(u32 key, u32 permissions) {
    if (!sInitialized) return -1;
    
    if (key != 0 && findByKey(key)) return -1;
    
    MessageQueue* mq = nullptr;
    for (u32 i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (sQueues[i].state == MessageQueueState::Invalid) {
            mq = &sQueues[i];
            break;
        }
    }
    
    if (!mq) return -1;
    
    memset(mq, 0, sizeof(MessageQueue));
    mq->id = allocateId();
    mq->key = key;
    mq->state = MessageQueueState::Active;
    mq->head = 0;
    mq->tail = 0;
    mq->count = 0;
    mq->maxMessages = MAX_MESSAGES_PER_QUEUE;
    mq->permissions = permissions;
    
    process::Process* current = process::PM::currentProcess();
    if (current) {
        mq->ownerUid = 0;
        mq->creatorPid = current->pid;
    }
    
    return static_cast<i32>(mq->id);
}

i32 MessageQueueManager::get(u32 key) {
    if (!sInitialized || key == 0) return -1;
    
    MessageQueue* mq = findByKey(key);
    if (!mq) return -1;
    
    return static_cast<i32>(mq->id);
}

bool MessageQueueManager::send(i32 mqId, const void* data, usize size, u32 type) {
    if (!sInitialized || !data || size == 0 || size > MAX_MESSAGE_SIZE) return false;
    
    MessageQueue* mq = findById(mqId);
    if (!mq || mq->state != MessageQueueState::Active) return false;
    
    if (mq->count >= mq->maxMessages) return false;
    
    Message* msg = &mq->messages[mq->tail];
    msg->type = type;
    msg->size = static_cast<u32>(size);
    memcpy(msg->data, data, size);
    msg->timestamp = process::Scheduler::systemTime();
    
    process::Process* current = process::PM::currentProcess();
    if (current) {
        msg->senderPid = current->pid;
    }
    
    mq->tail = (mq->tail + 1) % MAX_MESSAGES_PER_QUEUE;
    mq->count++;
    
    return true;
}

i64 MessageQueueManager::receive(i32 mqId, void* buffer, usize maxSize, u32 type) {
    if (!sInitialized || !buffer || maxSize == 0) return -1;
    
    MessageQueue* mq = findById(mqId);
    if (!mq || mq->state != MessageQueueState::Active) return -1;
    
    if (mq->count == 0) return 0;
    
    u32 index = mq->head;
    u32 prevIndex = mq->head;
    bool found = false;
    
    for (u32 i = 0; i < mq->count; i++) {
        u32 checkIndex = (mq->head + i) % MAX_MESSAGES_PER_QUEUE;
        if (type == 0 || mq->messages[checkIndex].type == type) {
            index = checkIndex;
            found = true;
            break;
        }
    }
    
    if (!found) return 0;
    
    Message* msg = &mq->messages[index];
    usize copySize = min(maxSize, msg->size);
    memcpy(buffer, msg->data, copySize);
    
    if (index == mq->head) {
        mq->head = (mq->head + 1) % MAX_MESSAGES_PER_QUEUE;
    } else {
        for (u32 i = index; i != mq->head; ) {
            u32 prev = (i == 0) ? MAX_MESSAGES_PER_QUEUE - 1 : i - 1;
            mq->messages[i] = mq->messages[prev];
            i = prev;
        }
        mq->head = (mq->head + 1) % MAX_MESSAGES_PER_QUEUE;
    }
    
    mq->count--;
    
    return static_cast<i64>(copySize);
}

bool MessageQueueManager::remove(i32 mqId) {
    if (!sInitialized) return false;
    
    MessageQueue* mq = findById(mqId);
    if (!mq || mq->state != MessageQueueState::Active) return false;
    
    mq->state = MessageQueueState::Invalid;
    return true;
}

MessageQueue* MessageQueueManager::getInfo(i32 mqId) {
    return findById(mqId);
}

u32 MessageQueueManager::messageCount(i32 mqId) {
    MessageQueue* mq = findById(mqId);
    if (!mq) return 0;
    return mq->count;
}

bool MessageQueueManager::isInitialized() {
    return sInitialized;
}

MessageQueue* MessageQueueManager::findByKey(u32 key) {
    for (u32 i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (sQueues[i].state != MessageQueueState::Invalid && sQueues[i].key == key) {
            return &sQueues[i];
        }
    }
    return nullptr;
}

MessageQueue* MessageQueueManager::findById(i32 id) {
    for (u32 i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (sQueues[i].state != MessageQueueState::Invalid && 
            static_cast<i32>(sQueues[i].id) == id) {
            return &sQueues[i];
        }
    }
    return nullptr;
}

u32 MessageQueueManager::allocateId() {
    return sNextId++;
}

ProcessSignals SignalManager::sProcessSignals[process::MAX_PROCESSES];
bool SignalManager::sInitialized = false;

void SignalManager::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < process::MAX_PROCESSES; i++) {
        memset(&sProcessSignals[i], 0, sizeof(ProcessSignals));
        
        for (i32 j = 0; j < MAX_SIGNALS; j++) {
            sProcessSignals[i].actions[j].handler = SIG_DFL;
            sProcessSignals[i].actions[j].flags = 0;
            sProcessSignals[i].actions[j].mask = 0;
        }
    }
    
    sInitialized = true;
}

bool SignalManager::send(u32 targetPid, i32 signum) {
    if (!sInitialized) return false;
    if (signum < 0 || signum >= MAX_SIGNALS) return false;
    
    process::Process* target = process::PM::getProcess(targetPid);
    if (!target) return false;
    
    ProcessSignals* ps = &sProcessSignals[targetPid % process::MAX_PROCESSES];
    
    ps->pending[signum].signum = signum;
    ps->pending[signum].senderPid = 0;
    ps->pending[signum].pending = true;
    ps->pendingMask |= (1ULL << signum);
    
    if (target->state == process::ProcessState::Blocked ||
        target->state == process::ProcessState::Sleeping) {
        if (signum == SIGKILL || signum == SIGCONT) {
            process::Scheduler::wakeup(target);
        }
    }
    
    return true;
}

bool SignalManager::sendFromProcess(u32 senderPid, u32 targetPid, i32 signum) {
    if (!sInitialized) return false;
    if (!canSendSignal(senderPid, targetPid, signum)) return false;
    
    process::Process* target = process::PM::getProcess(targetPid);
    if (!target) return false;
    
    ProcessSignals* ps = &sProcessSignals[targetPid % process::MAX_PROCESSES];
    
    ps->pending[signum].signum = signum;
    ps->pending[signum].senderPid = senderPid;
    ps->pending[signum].pending = true;
    ps->pendingMask |= (1ULL << signum);
    
    if (target->state == process::ProcessState::Blocked ||
        target->state == process::ProcessState::Sleeping) {
        if (signum == SIGKILL || signum == SIGCONT) {
            process::Scheduler::wakeup(target);
        }
    }
    
    return true;
}

bool SignalManager::setHandler(u32 pid, i32 signum, u64 handler, u32 flags) {
    if (!sInitialized) return false;
    if (signum < 0 || signum >= MAX_SIGNALS) return false;
    if (signum == SIGKILL || signum == SIGSTOP) return false;
    
    ProcessSignals* ps = &sProcessSignals[pid % process::MAX_PROCESSES];
    ps->actions[signum].handler = handler;
    ps->actions[signum].flags = flags;
    
    return true;
}

SignalAction* SignalManager::getHandler(u32 pid, i32 signum) {
    if (!sInitialized) return nullptr;
    if (signum < 0 || signum >= MAX_SIGNALS) return nullptr;
    
    return &sProcessSignals[pid % process::MAX_PROCESSES].actions[signum];
}

bool SignalManager::block(u32 pid, i32 signum) {
    if (!sInitialized) return false;
    if (signum < 0 || signum >= MAX_SIGNALS) return false;
    if (signum == SIGKILL || signum == SIGSTOP) return false;
    
    sProcessSignals[pid % process::MAX_PROCESSES].blocked |= (1ULL << signum);
    return true;
}

bool SignalManager::unblock(u32 pid, i32 signum) {
    if (!sInitialized) return false;
    if (signum < 0 || signum >= MAX_SIGNALS) return false;
    
    sProcessSignals[pid % process::MAX_PROCESSES].blocked &= ~(1ULL << signum);
    return true;
}

bool SignalManager::isBlocked(u32 pid, i32 signum) {
    if (!sInitialized) return false;
    if (signum < 0 || signum >= MAX_SIGNALS) return false;
    
    return (sProcessSignals[pid % process::MAX_PROCESSES].blocked & (1ULL << signum)) != 0;
}

bool SignalManager::hasPending(u32 pid) {
    if (!sInitialized) return false;
    
    ProcessSignals* ps = &sProcessSignals[pid % process::MAX_PROCESSES];
    u64 deliverable = ps->pendingMask & ~ps->blocked;
    
    return deliverable != 0;
}

i32 SignalManager::getNextPending(u32 pid) {
    if (!sInitialized) return -1;
    
    ProcessSignals* ps = &sProcessSignals[pid % process::MAX_PROCESSES];
    u64 deliverable = ps->pendingMask & ~ps->blocked;
    
    if (deliverable == 0) return -1;
    
    for (i32 i = 0; i < MAX_SIGNALS; i++) {
        if (deliverable & (1ULL << i)) {
            return i;
        }
    }
    
    return -1;
}

void SignalManager::clearPending(u32 pid, i32 signum) {
    if (!sInitialized) return;
    if (signum < 0 || signum >= MAX_SIGNALS) return;
    
    ProcessSignals* ps = &sProcessSignals[pid % process::MAX_PROCESSES];
    ps->pending[signum].pending = false;
    ps->pendingMask &= ~(1ULL << signum);
}

void SignalManager::deliverSignals(process::Process* proc) {
    if (!sInitialized || !proc) return;
    
    while (hasPending(proc->pid)) {
        i32 signum = getNextPending(proc->pid);
        if (signum < 0) break;
        
        ProcessSignals* ps = &sProcessSignals[proc->pid % process::MAX_PROCESSES];
        SignalAction* action = &ps->actions[signum];
        
        clearPending(proc->pid, signum);
        
        if (action->handler == SIG_IGN) {
            continue;
        }
        
        if (action->handler == SIG_DFL) {
            defaultHandler(proc, signum);
            continue;
        }
    }
}

ProcessSignals* SignalManager::getProcessSignals(u32 pid) {
    if (!sInitialized) return nullptr;
    return &sProcessSignals[pid % process::MAX_PROCESSES];
}

bool SignalManager::isInitialized() {
    return sInitialized;
}

void SignalManager::defaultHandler(process::Process* proc, i32 signum) {
    switch (signum) {
        case SIGKILL:
        case SIGTERM:
        case SIGINT:
        case SIGQUIT:
        case SIGSEGV:
        case SIGBUS:
        case SIGFPE:
        case SIGILL:
        case SIGPIPE:
            process::PM::terminateProcess(proc, 128 + signum);
            break;
            
        case SIGSTOP:
            process::Scheduler::blockProcess(proc);
            break;
            
        case SIGCONT:
            if (proc->state == process::ProcessState::Blocked) {
                process::Scheduler::unblockProcess(proc);
            }
            break;
            
        case SIGCHLD:
        case SIGUSR1:
        case SIGUSR2:
        case SIGALRM:
        case SIGHUP:
            break;
    }
}

bool SignalManager::canSendSignal(u32 senderPid, u32 targetPid, i32 signum) {
    if (signum == SIGKILL || signum == SIGSTOP) {
        process::Process* sender = process::PM::getProcess(senderPid);
        if (!sender) return false;
    }
    
    return true;
}

static bool sIPCInitialized = false;

void IPC::initialize() {
    if (sIPCInitialized) return;
    
    PipeManager::initialize();
    SharedMemoryManager::initialize();
    MessageQueueManager::initialize();
    SignalManager::initialize();
    
    sIPCInitialized = true;
}

bool IPC::isInitialized() {
    return sIPCInitialized;
}

}
