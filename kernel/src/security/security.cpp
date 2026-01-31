#include "../../include/security/security.hpp"
#include "../../include/memory/vmm.hpp"
#include "../../include/process/scheduler.hpp"

namespace sertos::security {

namespace {

void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

void strcpy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

u64 simpleRandom() {
    static u64 seed = 0x123456789ABCDEF0ULL;
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return seed;
}

}

CapabilitySet CapabilityManager::sProcessCaps[process::MAX_PROCESSES];
bool CapabilityManager::sInitialized = false;

void CapabilityManager::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < process::MAX_PROCESSES; i++) {
        memset(&sProcessCaps[i], 0, sizeof(CapabilitySet));
    }
    
    sProcessCaps[0].effective = CAP_ALL;
    sProcessCaps[0].permitted = CAP_ALL;
    sProcessCaps[0].inheritable = CAP_ALL;
    sProcessCaps[0].bounding = CAP_ALL;
    sProcessCaps[0].ambient = 0;
    
    sInitialized = true;
}

bool CapabilityManager::hasCapability(u32 pid, u64 capability) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    return (sProcessCaps[pid].effective & capability) == capability;
}

bool CapabilityManager::grantCapability(u32 pid, u64 capability) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    if ((sProcessCaps[pid].bounding & capability) != capability) {
        return false;
    }
    
    sProcessCaps[pid].permitted |= capability;
    sProcessCaps[pid].effective |= capability;
    
    return true;
}

bool CapabilityManager::revokeCapability(u32 pid, u64 capability) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    sProcessCaps[pid].effective &= ~capability;
    
    return true;
}

CapabilitySet* CapabilityManager::getCapabilities(u32 pid) {
    if (!sInitialized) return nullptr;
    if (pid >= process::MAX_PROCESSES) return nullptr;
    
    return &sProcessCaps[pid];
}

bool CapabilityManager::setCapabilities(u32 pid, const CapabilitySet* caps) {
    if (!sInitialized || !caps) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    if ((caps->permitted & ~sProcessCaps[pid].bounding) != 0) {
        return false;
    }
    
    sProcessCaps[pid] = *caps;
    
    return true;
}

bool CapabilityManager::dropCapability(u32 pid, u64 capability) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    sProcessCaps[pid].effective &= ~capability;
    sProcessCaps[pid].permitted &= ~capability;
    sProcessCaps[pid].inheritable &= ~capability;
    sProcessCaps[pid].ambient &= ~capability;
    
    return true;
}

bool CapabilityManager::dropAllCapabilities(u32 pid) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    sProcessCaps[pid].effective = 0;
    sProcessCaps[pid].permitted = 0;
    sProcessCaps[pid].inheritable = 0;
    sProcessCaps[pid].ambient = 0;
    
    return true;
}

bool CapabilityManager::canRaiseCapability(u32 pid, u64 capability) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    return (sProcessCaps[pid].permitted & capability) == capability;
}

bool CapabilityManager::raiseCapability(u32 pid, u64 capability) {
    if (!sInitialized) return false;
    if (!canRaiseCapability(pid, capability)) return false;
    
    sProcessCaps[pid].effective |= capability;
    
    return true;
}

bool CapabilityManager::isInitialized() {
    return sInitialized;
}

MemoryRegion MemoryProtection::sRegions[process::MAX_PROCESSES][64];
u32 MemoryProtection::sRegionCount[process::MAX_PROCESSES];
bool MemoryProtection::sWXEnforced[process::MAX_PROCESSES];
bool MemoryProtection::sASLREnabled[process::MAX_PROCESSES];
bool MemoryProtection::sInitialized = false;

void MemoryProtection::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < process::MAX_PROCESSES; i++) {
        for (u32 j = 0; j < 64; j++) {
            memset(&sRegions[i][j], 0, sizeof(MemoryRegion));
        }
        sRegionCount[i] = 0;
        sWXEnforced[i] = true;
        sASLREnabled[i] = true;
    }
    
    sInitialized = true;
}

bool MemoryProtection::protectRegion(u32 pid, u64 start, u64 size, u32 protection) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    if (sRegionCount[pid] >= 64) return false;
    
    if (sWXEnforced[pid] && (protection & PROT_WRITE) && (protection & PROT_EXEC)) {
        return false;
    }
    
    MemoryRegion* region = &sRegions[pid][sRegionCount[pid]];
    region->start = start;
    region->end = start + size;
    region->protection = protection;
    region->readable = (protection & PROT_READ) != 0;
    region->writable = (protection & PROT_WRITE) != 0;
    region->executable = (protection & PROT_EXEC) != 0;
    
    sRegionCount[pid]++;
    
    applyPageProtection(pid, start, protection);
    
    return true;
}

bool MemoryProtection::unprotectRegion(u32 pid, u64 start, u64 size) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    u64 end = start + size;
    
    for (u32 i = 0; i < sRegionCount[pid]; i++) {
        if (sRegions[pid][i].start == start && sRegions[pid][i].end == end) {
            for (u32 j = i; j < sRegionCount[pid] - 1; j++) {
                sRegions[pid][j] = sRegions[pid][j + 1];
            }
            sRegionCount[pid]--;
            return true;
        }
    }
    
    return false;
}

bool MemoryProtection::checkAccess(u32 pid, u64 address, AccessType access) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    for (u32 i = 0; i < sRegionCount[pid]; i++) {
        if (address >= sRegions[pid][i].start && address < sRegions[pid][i].end) {
            u32 prot = sRegions[pid][i].protection;
            
            if (access == AccessType::Read && !(prot & PROT_READ)) return false;
            if (access == AccessType::Write && !(prot & PROT_WRITE)) return false;
            if (access == AccessType::Execute && !(prot & PROT_EXEC)) return false;
            
            return true;
        }
    }
    
    return false;
}

u32 MemoryProtection::getProtection(u32 pid, u64 address) {
    if (!sInitialized) return PROT_NONE;
    if (pid >= process::MAX_PROCESSES) return PROT_NONE;
    
    for (u32 i = 0; i < sRegionCount[pid]; i++) {
        if (address >= sRegions[pid][i].start && address < sRegions[pid][i].end) {
            return sRegions[pid][i].protection;
        }
    }
    
    return PROT_NONE;
}

bool MemoryProtection::setExecutable(u32 pid, u64 start, u64 size, bool executable) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    u64 end = start + size;
    
    for (u32 i = 0; i < sRegionCount[pid]; i++) {
        if (sRegions[pid][i].start == start && sRegions[pid][i].end == end) {
            if (executable && sWXEnforced[pid] && sRegions[pid][i].writable) {
                return false;
            }
            
            sRegions[pid][i].executable = executable;
            if (executable) {
                sRegions[pid][i].protection |= PROT_EXEC;
            } else {
                sRegions[pid][i].protection &= ~PROT_EXEC;
            }
            
            applyPageProtection(pid, start, sRegions[pid][i].protection);
            return true;
        }
    }
    
    return false;
}

bool MemoryProtection::setWritable(u32 pid, u64 start, u64 size, bool writable) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    u64 end = start + size;
    
    for (u32 i = 0; i < sRegionCount[pid]; i++) {
        if (sRegions[pid][i].start == start && sRegions[pid][i].end == end) {
            if (writable && sWXEnforced[pid] && sRegions[pid][i].executable) {
                return false;
            }
            
            sRegions[pid][i].writable = writable;
            if (writable) {
                sRegions[pid][i].protection |= PROT_WRITE;
            } else {
                sRegions[pid][i].protection &= ~PROT_WRITE;
            }
            
            applyPageProtection(pid, start, sRegions[pid][i].protection);
            return true;
        }
    }
    
    return false;
}

bool MemoryProtection::setReadable(u32 pid, u64 start, u64 size, bool readable) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    u64 end = start + size;
    
    for (u32 i = 0; i < sRegionCount[pid]; i++) {
        if (sRegions[pid][i].start == start && sRegions[pid][i].end == end) {
            sRegions[pid][i].readable = readable;
            if (readable) {
                sRegions[pid][i].protection |= PROT_READ;
            } else {
                sRegions[pid][i].protection &= ~PROT_READ;
            }
            
            applyPageProtection(pid, start, sRegions[pid][i].protection);
            return true;
        }
    }
    
    return false;
}

bool MemoryProtection::enforceWX(u32 pid) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    for (u32 i = 0; i < sRegionCount[pid]; i++) {
        if (sRegions[pid][i].writable && sRegions[pid][i].executable) {
            sRegions[pid][i].executable = false;
            sRegions[pid][i].protection &= ~PROT_EXEC;
            applyPageProtection(pid, sRegions[pid][i].start, sRegions[pid][i].protection);
        }
    }
    
    sWXEnforced[pid] = true;
    return true;
}

bool MemoryProtection::isWXEnforced(u32 pid) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    return sWXEnforced[pid];
}

bool MemoryProtection::enableASLR(u32 pid) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    sASLREnabled[pid] = true;
    return true;
}

bool MemoryProtection::disableASLR(u32 pid) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    sASLREnabled[pid] = false;
    return true;
}

bool MemoryProtection::isASLREnabled(u32 pid) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    return sASLREnabled[pid];
}

u64 MemoryProtection::randomizeAddress(u64 base, u64 range) {
    u64 random = simpleRandom();
    u64 offset = (random % range) & ~0xFFFULL;
    return base + offset;
}

bool MemoryProtection::isInitialized() {
    return sInitialized;
}

void MemoryProtection::applyPageProtection(u32 pid, u64 address, u32 protection) {
    process::Process* proc = process::PM::getProcess(pid);
    if (!proc || !proc->pageTable) return;
    
    u64 flags = memory::PAGE_PRESENT | memory::PAGE_USER;
    
    if (protection & PROT_WRITE) {
        flags |= memory::PAGE_WRITABLE;
    }
    
    if (!(protection & PROT_EXEC)) {
        flags |= memory::PAGE_NO_EXECUTE;
    }
}

AccessRule AccessControl::sRules[MAX_ACCESS_RULES];
SecurityLevel AccessControl::sProcessLevels[process::MAX_PROCESSES];
u32 AccessControl::sRuleCount = 0;
bool AccessControl::sInitialized = false;

void AccessControl::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_ACCESS_RULES; i++) {
        memset(&sRules[i], 0, sizeof(AccessRule));
        sRules[i].active = false;
    }
    
    for (u32 i = 0; i < process::MAX_PROCESSES; i++) {
        sProcessLevels[i] = SecurityLevel::Unclassified;
    }
    
    sRuleCount = 0;
    sInitialized = true;
}

bool AccessControl::checkAccess(u32 subjectUid, u32 objectUid, u32 objectGid, 
                                u32 mode, AccessType requested) {
    if (!sInitialized) return false;
    
    if (subjectUid == 0) return true;
    
    return checkDiscretionaryAccess(subjectUid, objectUid, objectGid, mode, requested);
}

bool AccessControl::addRule(u32 subjectUid, u32 objectUid, AccessType access, bool allow) {
    if (!sInitialized) return false;
    if (sRuleCount >= MAX_ACCESS_RULES) return false;
    
    AccessRule* rule = nullptr;
    for (u32 i = 0; i < MAX_ACCESS_RULES; i++) {
        if (!sRules[i].active) {
            rule = &sRules[i];
            rule->id = i;
            break;
        }
    }
    
    if (!rule) return false;
    
    rule->subjectUid = subjectUid;
    rule->objectUid = objectUid;
    rule->access = access;
    rule->allow = allow;
    rule->active = true;
    
    sRuleCount++;
    
    return true;
}

bool AccessControl::removeRule(u32 ruleId) {
    if (!sInitialized) return false;
    if (ruleId >= MAX_ACCESS_RULES) return false;
    
    if (!sRules[ruleId].active) return false;
    
    sRules[ruleId].active = false;
    sRuleCount--;
    
    return true;
}

AccessRule* AccessControl::getRule(u32 ruleId) {
    if (!sInitialized) return nullptr;
    if (ruleId >= MAX_ACCESS_RULES) return nullptr;
    
    if (sRules[ruleId].active) {
        return &sRules[ruleId];
    }
    
    return nullptr;
}

bool AccessControl::setSecurityLevel(u32 pid, SecurityLevel level) {
    if (!sInitialized) return false;
    if (pid >= process::MAX_PROCESSES) return false;
    
    sProcessLevels[pid] = level;
    return true;
}

SecurityLevel AccessControl::getSecurityLevel(u32 pid) {
    if (!sInitialized) return SecurityLevel::Unclassified;
    if (pid >= process::MAX_PROCESSES) return SecurityLevel::Unclassified;
    
    return sProcessLevels[pid];
}

bool AccessControl::canAccess(u32 subjectPid, u32 objectPid) {
    if (!sInitialized) return false;
    if (subjectPid >= process::MAX_PROCESSES) return false;
    if (objectPid >= process::MAX_PROCESSES) return false;
    
    SecurityLevel subjectLevel = sProcessLevels[subjectPid];
    SecurityLevel objectLevel = sProcessLevels[objectPid];
    
    return checkMandatoryAccess(subjectLevel, objectLevel, AccessType::Read);
}

bool AccessControl::isInitialized() {
    return sInitialized;
}

bool AccessControl::checkMandatoryAccess(SecurityLevel subject, SecurityLevel object, AccessType access) {
    if (access == AccessType::Read || access == AccessType::Execute) {
        return static_cast<u8>(subject) >= static_cast<u8>(object);
    }
    
    if (access == AccessType::Write) {
        return static_cast<u8>(subject) <= static_cast<u8>(object);
    }
    
    return false;
}

bool AccessControl::checkDiscretionaryAccess(u32 subjectUid, u32 objectUid, u32 objectGid, 
                                             u32 mode, AccessType access) {
    u32 effectiveMode = 0;
    
    if (subjectUid == objectUid) {
        effectiveMode = (mode >> 6) & 0x7;
    } else {
        effectiveMode = mode & 0x7;
    }
    
    u32 requestedBits = 0;
    if (access == AccessType::Read) requestedBits = 0x4;
    else if (access == AccessType::Write) requestedBits = 0x2;
    else if (access == AccessType::Execute) requestedBits = 0x1;
    else if (access == AccessType::All) requestedBits = 0x7;
    
    return (effectiveMode & requestedBits) == requestedBits;
}

AuditEntry AuditLog::sEntries[MAX_AUDIT_ENTRIES];
u32 AuditLog::sEntryCount = 0;
u32 AuditLog::sNextIndex = 0;
bool AuditLog::sAuditEnabled = true;
bool AuditLog::sInitialized = false;

void AuditLog::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_AUDIT_ENTRIES; i++) {
        memset(&sEntries[i], 0, sizeof(AuditEntry));
        sEntries[i].valid = false;
    }
    
    sEntryCount = 0;
    sNextIndex = 0;
    sAuditEnabled = true;
    sInitialized = true;
}

void AuditLog::log(AuditEventType type, u32 pid, u32 uid, u64 result, const char* details) {
    if (!sInitialized || !sAuditEnabled) return;
    
    AuditEntry* entry = &sEntries[sNextIndex];
    
    entry->timestamp = process::Scheduler::systemTime();
    entry->type = type;
    entry->pid = pid;
    entry->uid = uid;
    entry->result = result;
    if (details) {
        strcpy(entry->details, details, sizeof(entry->details));
    } else {
        entry->details[0] = '\0';
    }
    entry->valid = true;
    
    sNextIndex = (sNextIndex + 1) % MAX_AUDIT_ENTRIES;
    if (sEntryCount < MAX_AUDIT_ENTRIES) {
        sEntryCount++;
    }
}

void AuditLog::logSecurityViolation(u32 pid, u32 uid, const char* violation) {
    log(AuditEventType::SecurityViolation, pid, uid, 0, violation);
}

AuditEntry* AuditLog::getEntry(u32 index) {
    if (!sInitialized) return nullptr;
    if (index >= MAX_AUDIT_ENTRIES) return nullptr;
    
    if (sEntries[index].valid) {
        return &sEntries[index];
    }
    
    return nullptr;
}

u32 AuditLog::entryCount() {
    return sEntryCount;
}

void AuditLog::clear() {
    if (!sInitialized) return;
    
    for (u32 i = 0; i < MAX_AUDIT_ENTRIES; i++) {
        sEntries[i].valid = false;
    }
    
    sEntryCount = 0;
    sNextIndex = 0;
}

void AuditLog::enableAudit() {
    sAuditEnabled = true;
}

void AuditLog::disableAudit() {
    sAuditEnabled = false;
}

bool AuditLog::isAuditEnabled() {
    return sAuditEnabled;
}

bool AuditLog::isInitialized() {
    return sInitialized;
}

SecurityContext SecurityManager::sContexts[MAX_SECURITY_CONTEXTS];
u32 SecurityManager::sContextCount = 0;
bool SecurityManager::sInitialized = false;

void SecurityManager::initialize() {
    if (sInitialized) return;
    
    CapabilityManager::initialize();
    MemoryProtection::initialize();
    AccessControl::initialize();
    AuditLog::initialize();
    
    for (u32 i = 0; i < MAX_SECURITY_CONTEXTS; i++) {
        memset(&sContexts[i], 0, sizeof(SecurityContext));
        sContexts[i].active = false;
    }
    
    sContextCount = 0;
    sInitialized = true;
}

SecurityContext* SecurityManager::createContext(u32 pid) {
    if (!sInitialized) return nullptr;
    if (sContextCount >= MAX_SECURITY_CONTEXTS) return nullptr;
    
    SecurityContext* ctx = nullptr;
    for (u32 i = 0; i < MAX_SECURITY_CONTEXTS; i++) {
        if (!sContexts[i].active) {
            ctx = &sContexts[i];
            ctx->id = i;
            break;
        }
    }
    
    if (!ctx) return nullptr;
    
    memset(ctx, 0, sizeof(SecurityContext));
    ctx->pid = pid;
    ctx->uid = 0;
    ctx->gid = 0;
    ctx->level = SecurityLevel::Unclassified;
    ctx->noNewPrivs = false;
    ctx->seccompFilter = 0;
    ctx->active = true;
    
    CapabilitySet* caps = CapabilityManager::getCapabilities(pid);
    if (caps) {
        ctx->capabilities = *caps;
    }
    
    sContextCount++;
    
    return ctx;
}

void SecurityManager::destroyContext(u32 pid) {
    if (!sInitialized) return;
    
    for (u32 i = 0; i < MAX_SECURITY_CONTEXTS; i++) {
        if (sContexts[i].active && sContexts[i].pid == pid) {
            sContexts[i].active = false;
            sContextCount--;
            return;
        }
    }
}

SecurityContext* SecurityManager::getContext(u32 pid) {
    if (!sInitialized) return nullptr;
    
    for (u32 i = 0; i < MAX_SECURITY_CONTEXTS; i++) {
        if (sContexts[i].active && sContexts[i].pid == pid) {
            return &sContexts[i];
        }
    }
    
    return nullptr;
}

bool SecurityManager::setNoNewPrivs(u32 pid) {
    if (!sInitialized) return false;
    
    SecurityContext* ctx = getContext(pid);
    if (!ctx) return false;
    
    ctx->noNewPrivs = true;
    
    return true;
}

bool SecurityManager::hasNoNewPrivs(u32 pid) {
    if (!sInitialized) return false;
    
    SecurityContext* ctx = getContext(pid);
    if (!ctx) return false;
    
    return ctx->noNewPrivs;
}

bool SecurityManager::validateSyscall(u32 pid, u64 syscallNum) {
    if (!sInitialized) return true;
    
    SecurityContext* ctx = getContext(pid);
    if (!ctx) return true;
    
    if (ctx->seccompFilter == 0) return true;
    
    return true;
}

bool SecurityManager::installSeccompFilter(u32 pid, u64 filter) {
    if (!sInitialized) return false;
    
    SecurityContext* ctx = getContext(pid);
    if (!ctx) return false;
    
    ctx->seccompFilter = filter;
    
    return true;
}

bool SecurityManager::isInitialized() {
    return sInitialized;
}

}
