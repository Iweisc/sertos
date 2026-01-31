#include "../../include/user/user.hpp"

namespace sertos::user {

User UserManager::sUsers[MAX_USERS];
Group UserManager::sGroups[MAX_GROUPS];
Session UserManager::sSessions[MAX_USERS];
Session* UserManager::sCurrentSession = nullptr;
u32 UserManager::sUserCount = 0;
u32 UserManager::sGroupCount = 0;
u32 UserManager::sSessionCount = 0;
u32 UserManager::sNextUid = 1000;
u32 UserManager::sNextGid = 1000;
u32 UserManager::sNextSessionId = 1;
bool UserManager::sInitialized = false;

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

bool strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) return false;
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

usize strlen(const char* s) {
    usize len = 0;
    while (s[len]) len++;
    return len;
}

u32 simpleHash(const char* str) {
    u32 hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + static_cast<u8>(*str);
        str++;
    }
    return hash;
}

void u32ToHex(u32 value, char* out) {
    const char* hex = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) {
        out[i] = hex[value & 0xF];
        value >>= 4;
    }
    out[8] = '\0';
}

}

void UserManager::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_USERS; i++) {
        memset(&sUsers[i], 0, sizeof(User));
        sUsers[i].active = false;
    }
    
    for (u32 i = 0; i < MAX_GROUPS; i++) {
        memset(&sGroups[i], 0, sizeof(Group));
        sGroups[i].active = false;
    }
    
    for (u32 i = 0; i < MAX_USERS; i++) {
        memset(&sSessions[i], 0, sizeof(Session));
        sSessions[i].active = false;
    }
    
    createGroup("root", ROOT_GID);
    createGroup("wheel", 10);
    createGroup("users", 100);
    createGroup("nobody", NOBODY_GID);
    
    User* root = &sUsers[0];
    root->uid = ROOT_UID;
    root->gid = ROOT_GID;
    strcpy(root->username, "root", MAX_USERNAME);
    hashPassword("root", root->passwordHash);
    strcpy(root->homeDirectory, "/root", MAX_HOME_PATH);
    strcpy(root->shell, "/bin/sh", MAX_SHELL_PATH);
    root->capabilities = CAP_ALL;
    root->active = true;
    root->locked = false;
    root->groupCount = 1;
    root->groups[0] = ROOT_GID;
    sUserCount++;
    
    User* nobody = &sUsers[1];
    nobody->uid = NOBODY_UID;
    nobody->gid = NOBODY_GID;
    strcpy(nobody->username, "nobody", MAX_USERNAME);
    hashPassword("", nobody->passwordHash);
    strcpy(nobody->homeDirectory, "/nonexistent", MAX_HOME_PATH);
    strcpy(nobody->shell, "/bin/false", MAX_SHELL_PATH);
    nobody->capabilities = 0;
    nobody->active = true;
    nobody->locked = true;
    nobody->groupCount = 1;
    nobody->groups[0] = NOBODY_GID;
    sUserCount++;
    
    sInitialized = true;
}

bool UserManager::createUser(const char* username, const char* password, u32 uid, u32 gid) {
    if (!sInitialized) return false;
    if (sUserCount >= MAX_USERS) return false;
    if (getUserByName(username)) return false;
    if (getUser(uid)) return false;
    
    User* user = nullptr;
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (!sUsers[i].active) {
            user = &sUsers[i];
            break;
        }
    }
    
    if (!user) return false;
    
    memset(user, 0, sizeof(User));
    user->uid = uid;
    user->gid = gid;
    strcpy(user->username, username, MAX_USERNAME);
    hashPassword(password, user->passwordHash);
    
    char homePath[MAX_HOME_PATH];
    strcpy(homePath, "/home/", MAX_HOME_PATH);
    usize homeLen = strlen(homePath);
    strcpy(homePath + homeLen, username, MAX_HOME_PATH - homeLen);
    strcpy(user->homeDirectory, homePath, MAX_HOME_PATH);
    
    strcpy(user->shell, "/bin/sh", MAX_SHELL_PATH);
    user->capabilities = 0;
    user->active = true;
    user->locked = false;
    user->groupCount = 1;
    user->groups[0] = gid;
    
    sUserCount++;
    
    addUserToGroup(uid, gid);
    
    return true;
}

bool UserManager::deleteUser(u32 uid) {
    if (!sInitialized) return false;
    if (uid == ROOT_UID) return false;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    for (u32 i = 0; i < user->groupCount; i++) {
        removeUserFromGroup(uid, user->groups[i]);
    }
    
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (sSessions[i].active && sSessions[i].uid == uid) {
            destroySession(sSessions[i].sessionId);
        }
    }
    
    memset(user, 0, sizeof(User));
    user->active = false;
    sUserCount--;
    
    return true;
}

bool UserManager::modifyUser(u32 uid, const User* newData) {
    if (!sInitialized || !newData) return false;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    if (newData->username[0]) {
        User* existing = getUserByName(newData->username);
        if (existing && existing->uid != uid) return false;
        strcpy(user->username, newData->username, MAX_USERNAME);
    }
    
    if (newData->homeDirectory[0]) {
        strcpy(user->homeDirectory, newData->homeDirectory, MAX_HOME_PATH);
    }
    
    if (newData->shell[0]) {
        strcpy(user->shell, newData->shell, MAX_SHELL_PATH);
    }
    
    if (newData->gid != user->gid) {
        user->gid = newData->gid;
    }
    
    return true;
}

User* UserManager::getUser(u32 uid) {
    if (!sInitialized) return nullptr;
    
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (sUsers[i].active && sUsers[i].uid == uid) {
            return &sUsers[i];
        }
    }
    
    return nullptr;
}

User* UserManager::getUserByName(const char* username) {
    if (!sInitialized || !username) return nullptr;
    
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (sUsers[i].active && strcmp(sUsers[i].username, username)) {
            return &sUsers[i];
        }
    }
    
    return nullptr;
}

u32 UserManager::getUserCount() {
    return sUserCount;
}

bool UserManager::createGroup(const char* name, u32 gid) {
    if (!sInitialized) return false;
    if (sGroupCount >= MAX_GROUPS) return false;
    if (getGroupByName(name)) return false;
    if (getGroup(gid)) return false;
    
    Group* group = nullptr;
    for (u32 i = 0; i < MAX_GROUPS; i++) {
        if (!sGroups[i].active) {
            group = &sGroups[i];
            break;
        }
    }
    
    if (!group) return false;
    
    memset(group, 0, sizeof(Group));
    group->gid = gid;
    strcpy(group->name, name, MAX_USERNAME);
    group->memberCount = 0;
    group->active = true;
    
    sGroupCount++;
    
    return true;
}

bool UserManager::deleteGroup(u32 gid) {
    if (!sInitialized) return false;
    if (gid == ROOT_GID) return false;
    
    Group* group = getGroup(gid);
    if (!group) return false;
    
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (sUsers[i].active && sUsers[i].gid == gid) {
            return false;
        }
    }
    
    memset(group, 0, sizeof(Group));
    group->active = false;
    sGroupCount--;
    
    return true;
}

Group* UserManager::getGroup(u32 gid) {
    if (!sInitialized) return nullptr;
    
    for (u32 i = 0; i < MAX_GROUPS; i++) {
        if (sGroups[i].active && sGroups[i].gid == gid) {
            return &sGroups[i];
        }
    }
    
    return nullptr;
}

Group* UserManager::getGroupByName(const char* name) {
    if (!sInitialized || !name) return nullptr;
    
    for (u32 i = 0; i < MAX_GROUPS; i++) {
        if (sGroups[i].active && strcmp(sGroups[i].name, name)) {
            return &sGroups[i];
        }
    }
    
    return nullptr;
}

bool UserManager::addUserToGroup(u32 uid, u32 gid) {
    if (!sInitialized) return false;
    
    User* user = getUser(uid);
    Group* group = getGroup(gid);
    
    if (!user || !group) return false;
    
    if (isUserInGroup(uid, gid)) return true;
    
    if (user->groupCount >= MAX_GROUPS_PER_USER) return false;
    if (group->memberCount >= MAX_USERS) return false;
    
    user->groups[user->groupCount++] = gid;
    group->members[group->memberCount++] = uid;
    
    return true;
}

bool UserManager::removeUserFromGroup(u32 uid, u32 gid) {
    if (!sInitialized) return false;
    
    User* user = getUser(uid);
    Group* group = getGroup(gid);
    
    if (!user || !group) return false;
    
    bool foundInUser = false;
    for (u32 i = 0; i < user->groupCount; i++) {
        if (user->groups[i] == gid) {
            for (u32 j = i; j < user->groupCount - 1; j++) {
                user->groups[j] = user->groups[j + 1];
            }
            user->groupCount--;
            foundInUser = true;
            break;
        }
    }
    
    bool foundInGroup = false;
    for (u32 i = 0; i < group->memberCount; i++) {
        if (group->members[i] == uid) {
            for (u32 j = i; j < group->memberCount - 1; j++) {
                group->members[j] = group->members[j + 1];
            }
            group->memberCount--;
            foundInGroup = true;
            break;
        }
    }
    
    return foundInUser && foundInGroup;
}

bool UserManager::isUserInGroup(u32 uid, u32 gid) {
    if (!sInitialized) return false;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    for (u32 i = 0; i < user->groupCount; i++) {
        if (user->groups[i] == gid) return true;
    }
    
    return false;
}

bool UserManager::authenticate(const char* username, const char* password) {
    if (!sInitialized || !username || !password) return false;
    
    User* user = getUserByName(username);
    if (!user) return false;
    if (user->locked) return false;
    
    return compareHash(password, user->passwordHash);
}

bool UserManager::setPassword(u32 uid, const char* newPassword) {
    if (!sInitialized || !newPassword) return false;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    hashPassword(newPassword, user->passwordHash);
    return true;
}

bool UserManager::verifyPassword(u32 uid, const char* password) {
    if (!sInitialized || !password) return false;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    return compareHash(password, user->passwordHash);
}

Session* UserManager::createSession(u32 uid, const char* terminal) {
    if (!sInitialized) return nullptr;
    
    User* user = getUser(uid);
    if (!user || user->locked) return nullptr;
    
    Session* session = nullptr;
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (!sSessions[i].active) {
            session = &sSessions[i];
            break;
        }
    }
    
    if (!session) return nullptr;
    
    session->sessionId = allocateSessionId();
    session->uid = uid;
    session->loginTime = 0;
    session->lastActivity = 0;
    if (terminal) {
        strcpy(session->terminal, terminal, 32);
    } else {
        strcpy(session->terminal, "tty0", 32);
    }
    session->active = true;
    
    sSessionCount++;
    
    return session;
}

void UserManager::destroySession(u32 sessionId) {
    if (!sInitialized) return;
    
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (sSessions[i].active && sSessions[i].sessionId == sessionId) {
            memset(&sSessions[i], 0, sizeof(Session));
            sSessions[i].active = false;
            sSessionCount--;
            
            if (sCurrentSession && sCurrentSession->sessionId == sessionId) {
                sCurrentSession = nullptr;
            }
            return;
        }
    }
}

Session* UserManager::getSession(u32 sessionId) {
    if (!sInitialized) return nullptr;
    
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (sSessions[i].active && sSessions[i].sessionId == sessionId) {
            return &sSessions[i];
        }
    }
    
    return nullptr;
}

Session* UserManager::getCurrentSession() {
    return sCurrentSession;
}

void UserManager::setCurrentSession(Session* session) {
    sCurrentSession = session;
}

bool UserManager::checkPermission(u32 uid, u32 ownerUid, u32 ownerGid, u32 mode, u32 requestedAccess) {
    if (!sInitialized) return false;
    
    if (uid == ROOT_UID) return true;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    if (user->capabilities & CAP_DAC_OVERRIDE) return true;
    
    u32 effectiveMode = 0;
    
    if (uid == ownerUid) {
        effectiveMode = (mode >> 6) & 0x7;
    } else if (isUserInGroup(uid, ownerGid)) {
        effectiveMode = (mode >> 3) & 0x7;
    } else {
        effectiveMode = mode & 0x7;
    }
    
    return (effectiveMode & requestedAccess) == requestedAccess;
}

bool UserManager::hasCapability(u32 uid, u32 capability) {
    if (!sInitialized) return false;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    return (user->capabilities & capability) != 0;
}

bool UserManager::grantCapability(u32 uid, u32 capability) {
    if (!sInitialized) return false;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    user->capabilities |= capability;
    return true;
}

bool UserManager::revokeCapability(u32 uid, u32 capability) {
    if (!sInitialized) return false;
    
    User* user = getUser(uid);
    if (!user) return false;
    
    user->capabilities &= ~capability;
    return true;
}

Credentials* UserManager::createCredentials(u32 uid) {
    if (!sInitialized) return nullptr;
    
    User* user = getUser(uid);
    if (!user) return nullptr;
    
    static Credentials creds[MAX_USERS];
    static u32 nextCredSlot = 0;
    
    Credentials* c = &creds[nextCredSlot];
    nextCredSlot = (nextCredSlot + 1) % MAX_USERS;
    
    c->uid = uid;
    c->gid = user->gid;
    c->euid = uid;
    c->egid = user->gid;
    c->suid = uid;
    c->sgid = user->gid;
    c->capabilities = user->capabilities;
    
    c->supplementaryGroupCount = user->groupCount;
    for (u32 i = 0; i < user->groupCount && i < MAX_GROUPS_PER_USER; i++) {
        c->supplementaryGroups[i] = user->groups[i];
    }
    
    return c;
}

void UserManager::destroyCredentials(Credentials*) {
}

bool UserManager::setEffectiveUid(Credentials* creds, u32 euid) {
    if (!creds) return false;
    
    if (creds->uid == ROOT_UID || creds->euid == ROOT_UID) {
        creds->euid = euid;
        return true;
    }
    
    if (euid == creds->uid || euid == creds->suid) {
        creds->euid = euid;
        return true;
    }
    
    return false;
}

bool UserManager::setEffectiveGid(Credentials* creds, u32 egid) {
    if (!creds) return false;
    
    if (creds->uid == ROOT_UID || creds->euid == ROOT_UID) {
        creds->egid = egid;
        return true;
    }
    
    if (egid == creds->gid || egid == creds->sgid) {
        creds->egid = egid;
        return true;
    }
    
    return false;
}

bool UserManager::isInitialized() {
    return sInitialized;
}

void UserManager::hashPassword(const char* password, char* hashOut) {
    u32 hash1 = simpleHash(password);
    u32 hash2 = simpleHash(password) ^ 0xDEADBEEF;
    
    char temp[32];
    strcpy(temp, password, 32);
    for (usize i = 0; i < strlen(temp); i++) {
        temp[i] = static_cast<char>(temp[i] ^ 0x5A);
    }
    u32 hash3 = simpleHash(temp);
    u32 hash4 = hash1 ^ hash2 ^ hash3;
    
    u32ToHex(hash1, hashOut);
    u32ToHex(hash2, hashOut + 8);
    u32ToHex(hash3, hashOut + 16);
    u32ToHex(hash4, hashOut + 24);
    hashOut[32] = '\0';
}

bool UserManager::compareHash(const char* password, const char* hash) {
    char computed[MAX_PASSWORD_HASH];
    hashPassword(password, computed);
    return strcmp(computed, hash);
}

u32 UserManager::allocateUid() {
    return sNextUid++;
}

u32 UserManager::allocateGid() {
    return sNextGid++;
}

u32 UserManager::allocateSessionId() {
    return sNextSessionId++;
}

}
