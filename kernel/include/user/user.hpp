#pragma once

#include "../types.hpp"

namespace sertos::user {

constexpr u32 MAX_USERS = 64;
constexpr u32 MAX_GROUPS = 64;
constexpr u32 MAX_USERNAME = 32;
constexpr u32 MAX_PASSWORD_HASH = 64;
constexpr u32 MAX_HOME_PATH = 128;
constexpr u32 MAX_SHELL_PATH = 64;
constexpr u32 MAX_GROUPS_PER_USER = 16;

constexpr u32 ROOT_UID = 0;
constexpr u32 ROOT_GID = 0;
constexpr u32 NOBODY_UID = 65534;
constexpr u32 NOBODY_GID = 65534;

constexpr u32 PERM_READ = 0x04;
constexpr u32 PERM_WRITE = 0x02;
constexpr u32 PERM_EXEC = 0x01;

constexpr u32 CAP_CHOWN = 1 << 0;
constexpr u32 CAP_DAC_OVERRIDE = 1 << 1;
constexpr u32 CAP_DAC_READ_SEARCH = 1 << 2;
constexpr u32 CAP_FOWNER = 1 << 3;
constexpr u32 CAP_KILL = 1 << 4;
constexpr u32 CAP_SETGID = 1 << 5;
constexpr u32 CAP_SETUID = 1 << 6;
constexpr u32 CAP_NET_BIND_SERVICE = 1 << 7;
constexpr u32 CAP_SYS_BOOT = 1 << 8;
constexpr u32 CAP_SYS_ADMIN = 1 << 9;
constexpr u32 CAP_SYS_RESOURCE = 1 << 10;
constexpr u32 CAP_SYS_TIME = 1 << 11;
constexpr u32 CAP_MKNOD = 1 << 12;
constexpr u32 CAP_ALL = 0xFFFFFFFF;

struct User {
    u32 uid;
    u32 gid;
    char username[MAX_USERNAME];
    char passwordHash[MAX_PASSWORD_HASH];
    char homeDirectory[MAX_HOME_PATH];
    char shell[MAX_SHELL_PATH];
    u32 groups[MAX_GROUPS_PER_USER];
    u32 groupCount;
    u32 capabilities;
    bool active;
    bool locked;
};

struct Group {
    u32 gid;
    char name[MAX_USERNAME];
    u32 members[MAX_USERS];
    u32 memberCount;
    bool active;
};

struct Credentials {
    u32 uid;
    u32 gid;
    u32 euid;
    u32 egid;
    u32 suid;
    u32 sgid;
    u32 supplementaryGroups[MAX_GROUPS_PER_USER];
    u32 supplementaryGroupCount;
    u32 capabilities;
};

struct Session {
    u32 sessionId;
    u32 uid;
    u64 loginTime;
    u64 lastActivity;
    char terminal[32];
    bool active;
};

class UserManager {
public:
    static void initialize();
    
    static bool createUser(const char* username, const char* password, u32 uid, u32 gid);
    static bool deleteUser(u32 uid);
    static bool modifyUser(u32 uid, const User* newData);
    static User* getUser(u32 uid);
    static User* getUserByName(const char* username);
    static u32 getUserCount();
    
    static bool createGroup(const char* name, u32 gid);
    static bool deleteGroup(u32 gid);
    static Group* getGroup(u32 gid);
    static Group* getGroupByName(const char* name);
    static bool addUserToGroup(u32 uid, u32 gid);
    static bool removeUserFromGroup(u32 uid, u32 gid);
    static bool isUserInGroup(u32 uid, u32 gid);
    
    static bool authenticate(const char* username, const char* password);
    static bool setPassword(u32 uid, const char* newPassword);
    static bool verifyPassword(u32 uid, const char* password);
    
    static Session* createSession(u32 uid, const char* terminal);
    static void destroySession(u32 sessionId);
    static Session* getSession(u32 sessionId);
    static Session* getCurrentSession();
    static void setCurrentSession(Session* session);
    
    static bool checkPermission(u32 uid, u32 ownerUid, u32 ownerGid, u32 mode, u32 requestedAccess);
    static bool hasCapability(u32 uid, u32 capability);
    static bool grantCapability(u32 uid, u32 capability);
    static bool revokeCapability(u32 uid, u32 capability);
    
    static Credentials* createCredentials(u32 uid);
    static void destroyCredentials(Credentials* creds);
    static bool setEffectiveUid(Credentials* creds, u32 euid);
    static bool setEffectiveGid(Credentials* creds, u32 egid);
    
    static bool isInitialized();

private:
    static void hashPassword(const char* password, char* hashOut);
    static bool compareHash(const char* password, const char* hash);
    static u32 allocateUid();
    static u32 allocateGid();
    static u32 allocateSessionId();
    
    static User sUsers[MAX_USERS];
    static Group sGroups[MAX_GROUPS];
    static Session sSessions[MAX_USERS];
    static Session* sCurrentSession;
    static u32 sUserCount;
    static u32 sGroupCount;
    static u32 sSessionCount;
    static u32 sNextUid;
    static u32 sNextGid;
    static u32 sNextSessionId;
    static bool sInitialized;
};

using UM = UserManager;

}
