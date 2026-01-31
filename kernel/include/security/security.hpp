#pragma once

#include "../types.hpp"
#include "../process/process.hpp"

namespace sertos::security {

constexpr u32 MAX_SECURITY_CONTEXTS = 256;
constexpr u32 MAX_ACCESS_RULES = 512;
constexpr u32 MAX_AUDIT_ENTRIES = 1024;

constexpr u64 CAP_CHOWN = 1ULL << 0;
constexpr u64 CAP_DAC_OVERRIDE = 1ULL << 1;
constexpr u64 CAP_DAC_READ_SEARCH = 1ULL << 2;
constexpr u64 CAP_FOWNER = 1ULL << 3;
constexpr u64 CAP_FSETID = 1ULL << 4;
constexpr u64 CAP_KILL = 1ULL << 5;
constexpr u64 CAP_SETGID = 1ULL << 6;
constexpr u64 CAP_SETUID = 1ULL << 7;
constexpr u64 CAP_SETPCAP = 1ULL << 8;
constexpr u64 CAP_LINUX_IMMUTABLE = 1ULL << 9;
constexpr u64 CAP_NET_BIND_SERVICE = 1ULL << 10;
constexpr u64 CAP_NET_BROADCAST = 1ULL << 11;
constexpr u64 CAP_NET_ADMIN = 1ULL << 12;
constexpr u64 CAP_NET_RAW = 1ULL << 13;
constexpr u64 CAP_IPC_LOCK = 1ULL << 14;
constexpr u64 CAP_IPC_OWNER = 1ULL << 15;
constexpr u64 CAP_SYS_MODULE = 1ULL << 16;
constexpr u64 CAP_SYS_RAWIO = 1ULL << 17;
constexpr u64 CAP_SYS_CHROOT = 1ULL << 18;
constexpr u64 CAP_SYS_PTRACE = 1ULL << 19;
constexpr u64 CAP_SYS_PACCT = 1ULL << 20;
constexpr u64 CAP_SYS_ADMIN = 1ULL << 21;
constexpr u64 CAP_SYS_BOOT = 1ULL << 22;
constexpr u64 CAP_SYS_NICE = 1ULL << 23;
constexpr u64 CAP_SYS_RESOURCE = 1ULL << 24;
constexpr u64 CAP_SYS_TIME = 1ULL << 25;
constexpr u64 CAP_SYS_TTY_CONFIG = 1ULL << 26;
constexpr u64 CAP_MKNOD = 1ULL << 27;
constexpr u64 CAP_LEASE = 1ULL << 28;
constexpr u64 CAP_AUDIT_WRITE = 1ULL << 29;
constexpr u64 CAP_AUDIT_CONTROL = 1ULL << 30;
constexpr u64 CAP_SETFCAP = 1ULL << 31;
constexpr u64 CAP_ALL = 0xFFFFFFFFFFFFFFFFULL;

constexpr u32 PROT_NONE = 0x0;
constexpr u32 PROT_READ = 0x1;
constexpr u32 PROT_WRITE = 0x2;
constexpr u32 PROT_EXEC = 0x4;

constexpr u32 MAP_SHARED = 0x01;
constexpr u32 MAP_PRIVATE = 0x02;
constexpr u32 MAP_FIXED = 0x10;
constexpr u32 MAP_ANONYMOUS = 0x20;

enum class SecurityLevel : u8 {
    Unclassified = 0,
    Confidential = 1,
    Secret = 2,
    TopSecret = 3
};

enum class AccessType : u8 {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4,
    All = 7
};

enum class AuditEventType : u8 {
    None = 0,
    ProcessCreate,
    ProcessExit,
    FileOpen,
    FileRead,
    FileWrite,
    FileDelete,
    NetworkConnect,
    NetworkListen,
    PrivilegeUse,
    CapabilityChange,
    SecurityViolation
};

struct CapabilitySet {
    u64 effective;
    u64 permitted;
    u64 inheritable;
    u64 bounding;
    u64 ambient;
};

struct SecurityContext {
    u32 id;
    u32 pid;
    u32 uid;
    u32 gid;
    SecurityLevel level;
    CapabilitySet capabilities;
    u64 seccompFilter;
    bool noNewPrivs;
    bool active;
};

struct MemoryRegion {
    u64 start;
    u64 end;
    u32 protection;
    u32 flags;
    bool shared;
    bool executable;
    bool writable;
    bool readable;
};

struct AccessRule {
    u32 id;
    u32 subjectUid;
    u32 objectUid;
    AccessType access;
    bool allow;
    bool active;
};

struct AuditEntry {
    u64 timestamp;
    AuditEventType type;
    u32 pid;
    u32 uid;
    u64 result;
    char details[128];
    bool valid;
};

class CapabilityManager {
public:
    static void initialize();
    
    static bool hasCapability(u32 pid, u64 capability);
    static bool grantCapability(u32 pid, u64 capability);
    static bool revokeCapability(u32 pid, u64 capability);
    
    static CapabilitySet* getCapabilities(u32 pid);
    static bool setCapabilities(u32 pid, const CapabilitySet* caps);
    
    static bool dropCapability(u32 pid, u64 capability);
    static bool dropAllCapabilities(u32 pid);
    
    static bool canRaiseCapability(u32 pid, u64 capability);
    static bool raiseCapability(u32 pid, u64 capability);
    
    static bool isInitialized();

private:
    static CapabilitySet sProcessCaps[process::MAX_PROCESSES];
    static bool sInitialized;
};

class MemoryProtection {
public:
    static void initialize();
    
    static bool protectRegion(u32 pid, u64 start, u64 size, u32 protection);
    static bool unprotectRegion(u32 pid, u64 start, u64 size);
    
    static bool checkAccess(u32 pid, u64 address, AccessType access);
    static u32 getProtection(u32 pid, u64 address);
    
    static bool setExecutable(u32 pid, u64 start, u64 size, bool executable);
    static bool setWritable(u32 pid, u64 start, u64 size, bool writable);
    static bool setReadable(u32 pid, u64 start, u64 size, bool readable);
    
    static bool enforceWX(u32 pid);
    static bool isWXEnforced(u32 pid);
    
    static bool enableASLR(u32 pid);
    static bool disableASLR(u32 pid);
    static bool isASLREnabled(u32 pid);
    
    static u64 randomizeAddress(u64 base, u64 range);
    
    static bool isInitialized();

private:
    static void applyPageProtection(u32 pid, u64 address, u32 protection);
    
    static MemoryRegion sRegions[process::MAX_PROCESSES][64];
    static u32 sRegionCount[process::MAX_PROCESSES];
    static bool sWXEnforced[process::MAX_PROCESSES];
    static bool sASLREnabled[process::MAX_PROCESSES];
    static bool sInitialized;
};

class AccessControl {
public:
    static void initialize();
    
    static bool checkAccess(u32 subjectUid, u32 objectUid, u32 objectGid, 
                           u32 mode, AccessType requested);
    
    static bool addRule(u32 subjectUid, u32 objectUid, AccessType access, bool allow);
    static bool removeRule(u32 ruleId);
    static AccessRule* getRule(u32 ruleId);
    
    static bool setSecurityLevel(u32 pid, SecurityLevel level);
    static SecurityLevel getSecurityLevel(u32 pid);
    
    static bool canAccess(u32 subjectPid, u32 objectPid);
    
    static bool isInitialized();

private:
    static bool checkMandatoryAccess(SecurityLevel subject, SecurityLevel object, AccessType access);
    static bool checkDiscretionaryAccess(u32 subjectUid, u32 objectUid, u32 objectGid, 
                                        u32 mode, AccessType access);
    
    static AccessRule sRules[MAX_ACCESS_RULES];
    static SecurityLevel sProcessLevels[process::MAX_PROCESSES];
    static u32 sRuleCount;
    static bool sInitialized;
};

class AuditLog {
public:
    static void initialize();
    
    static void log(AuditEventType type, u32 pid, u32 uid, u64 result, const char* details);
    static void logSecurityViolation(u32 pid, u32 uid, const char* violation);
    
    static AuditEntry* getEntry(u32 index);
    static u32 entryCount();
    static void clear();
    
    static void enableAudit();
    static void disableAudit();
    static bool isAuditEnabled();
    
    static bool isInitialized();

private:
    static AuditEntry sEntries[MAX_AUDIT_ENTRIES];
    static u32 sEntryCount;
    static u32 sNextIndex;
    static bool sAuditEnabled;
    static bool sInitialized;
};

class SecurityManager {
public:
    static void initialize();
    
    static SecurityContext* createContext(u32 pid);
    static void destroyContext(u32 pid);
    static SecurityContext* getContext(u32 pid);
    
    static bool setNoNewPrivs(u32 pid);
    static bool hasNoNewPrivs(u32 pid);
    
    static bool validateSyscall(u32 pid, u64 syscallNum);
    static bool installSeccompFilter(u32 pid, u64 filter);
    
    static bool isInitialized();

private:
    static SecurityContext sContexts[MAX_SECURITY_CONTEXTS];
    static u32 sContextCount;
    static bool sInitialized;
};

}
