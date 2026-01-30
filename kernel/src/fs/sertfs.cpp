#include "../../include/fs/sertfs.hpp"
#include "../../include/disk/ata.hpp"

namespace sertos::fs {

u8 SertFs::sDriveIndex = 0;
bool SertFs::sMounted = false;
char SertFs::sMountPoint[MAX_PATH] = "";
char SertFs::sCurrentDirectory[MAX_PATH] = "/";
SertFsSuperblock SertFs::sSuperblock;
u8 SertFs::sBlockBuffer[SERTFS_BLOCK_SIZE];
SertFsFileHandle SertFs::sFileHandles[MAX_FILE_HANDLES];
SertFsDirHandle SertFs::sDirHandles[MAX_DIR_HANDLES];

namespace {

void memoryCopy(void* dest, const void* src, usize size) {
    auto* d = static_cast<u8*>(dest);
    auto* s = static_cast<const u8*>(src);
    while (size--) {
        *d++ = *s++;
    }
}

void memorySet(void* dest, u8 value, usize size) {
    auto* d = static_cast<u8*>(dest);
    while (size--) {
        *d++ = value;
    }
}

usize stringLength(const char* str) {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

void stringCopy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int stringCompare(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

int stringCompareN(const char* a, const char* b, usize n) {
    while (n && *a && *b && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return *a - *b;
}

u32 sectorsPerBlock() {
    return SERTFS_BLOCK_SIZE / disk::SECTOR_SIZE;
}

}

bool SertFs::initialize(u8 driveIndex) {
    if (!disk::ATA::isInitialized()) {
        return false;
    }
    
    if (driveIndex >= disk::ATA::driveCount()) {
        return false;
    }
    
    sDriveIndex = driveIndex;
    
    for (usize i = 0; i < MAX_FILE_HANDLES; i++) {
        sFileHandles[i].valid = false;
    }
    for (usize i = 0; i < MAX_DIR_HANDLES; i++) {
        sDirHandles[i].valid = false;
    }
    
    stringCopy(sCurrentDirectory, "/", MAX_PATH);
    
    return true;
}

bool SertFs::format(u8 driveIndex, const char* volumeLabel) {
    if (!disk::ATA::isInitialized()) {
        return false;
    }
    
    if (driveIndex >= disk::ATA::driveCount()) {
        return false;
    }
    
    sDriveIndex = driveIndex;
    
    u64 diskSectors = disk::ATA::sectorCount(driveIndex);
    u64 diskSize = diskSectors * disk::SECTOR_SIZE;
    u32 totalBlocks = static_cast<u32>(diskSize / SERTFS_BLOCK_SIZE);
    
    if (totalBlocks < 100) {
        return false;
    }
    
    u32 inodesPerBlock = SERTFS_BLOCK_SIZE / SERTFS_INODE_SIZE;
    u32 totalInodes = totalBlocks / 4;
    if (totalInodes < 64) totalInodes = 64;
    if (totalInodes > 65536) totalInodes = 65536;
    
    u32 inodeBitmapBlocks = (totalInodes + SERTFS_BLOCK_SIZE * 8 - 1) / (SERTFS_BLOCK_SIZE * 8);
    u32 blockBitmapBlocks = (totalBlocks + SERTFS_BLOCK_SIZE * 8 - 1) / (SERTFS_BLOCK_SIZE * 8);
    u32 inodeTableBlocks = (totalInodes + inodesPerBlock - 1) / inodesPerBlock;
    
    u32 metadataBlocks = 1 + inodeBitmapBlocks + blockBitmapBlocks + inodeTableBlocks;
    u32 dataBlocks = totalBlocks - metadataBlocks;
    
    memorySet(&sSuperblock, 0, sizeof(sSuperblock));
    sSuperblock.magic = SERTFS_MAGIC;
    sSuperblock.version = SERTFS_VERSION;
    sSuperblock.blockSize = SERTFS_BLOCK_SIZE;
    sSuperblock.totalBlocks = totalBlocks;
    sSuperblock.freeBlocks = dataBlocks - 1;
    sSuperblock.totalInodes = totalInodes;
    sSuperblock.freeInodes = totalInodes - 2;
    sSuperblock.inodeBitmapBlock = 1;
    sSuperblock.blockBitmapBlock = 1 + inodeBitmapBlocks;
    sSuperblock.inodeTableBlock = 1 + inodeBitmapBlocks + blockBitmapBlocks;
    sSuperblock.dataBlockStart = metadataBlocks;
    sSuperblock.inodeBitmapBlocks = inodeBitmapBlocks;
    sSuperblock.blockBitmapBlocks = blockBitmapBlocks;
    sSuperblock.inodeTableBlocks = inodeTableBlocks;
    sSuperblock.createdTime = 0;
    sSuperblock.lastMountTime = 0;
    sSuperblock.mountCount = 0;
    sSuperblock.maxMountCount = 100;
    
    if (volumeLabel) {
        stringCopy(reinterpret_cast<char*>(sSuperblock.volumeLabel), volumeLabel, 32);
    }
    
    if (!writeSuperblock()) {
        return false;
    }
    
    memorySet(sBlockBuffer, 0, SERTFS_BLOCK_SIZE);
    for (u32 i = 0; i < inodeBitmapBlocks; i++) {
        if (!writeBlock(sSuperblock.inodeBitmapBlock + i, sBlockBuffer)) {
            return false;
        }
    }
    
    sBlockBuffer[0] = 0x03;
    if (!writeBlock(sSuperblock.inodeBitmapBlock, sBlockBuffer)) {
        return false;
    }
    
    memorySet(sBlockBuffer, 0, SERTFS_BLOCK_SIZE);
    for (u32 i = 0; i < blockBitmapBlocks; i++) {
        if (!writeBlock(sSuperblock.blockBitmapBlock + i, sBlockBuffer)) {
            return false;
        }
    }
    
    memorySet(sBlockBuffer, 0xFF, metadataBlocks / 8);
    if (metadataBlocks % 8) {
        sBlockBuffer[metadataBlocks / 8] = (1 << (metadataBlocks % 8)) - 1;
    }
    sBlockBuffer[sSuperblock.dataBlockStart / 8] |= (1 << (sSuperblock.dataBlockStart % 8));
    if (!writeBlock(sSuperblock.blockBitmapBlock, sBlockBuffer)) {
        return false;
    }
    
    memorySet(sBlockBuffer, 0, SERTFS_BLOCK_SIZE);
    for (u32 i = 0; i < inodeTableBlocks; i++) {
        if (!writeBlock(sSuperblock.inodeTableBlock + i, sBlockBuffer)) {
            return false;
        }
    }
    
    SertFsInode rootInode;
    memorySet(&rootInode, 0, sizeof(rootInode));
    rootInode.type = static_cast<u16>(SertFsInodeType::Directory);
    rootInode.permissions = 0755;
    rootInode.uid = 0;
    rootInode.gid = 0;
    rootInode.size = SERTFS_BLOCK_SIZE;
    rootInode.linkCount = 2;
    rootInode.directBlocks[0] = sSuperblock.dataBlockStart;
    
    if (!writeInode(SERTFS_ROOT_INODE, &rootInode)) {
        return false;
    }
    
    memorySet(sBlockBuffer, 0, SERTFS_BLOCK_SIZE);
    
    SertFsDirEntry* dotEntry = reinterpret_cast<SertFsDirEntry*>(sBlockBuffer);
    dotEntry->inode = SERTFS_ROOT_INODE;
    dotEntry->recordLength = 12;
    dotEntry->nameLength = 1;
    dotEntry->fileType = static_cast<u8>(SertFsInodeType::Directory);
    dotEntry->name[0] = '.';
    dotEntry->name[1] = '\0';
    
    SertFsDirEntry* dotdotEntry = reinterpret_cast<SertFsDirEntry*>(sBlockBuffer + 12);
    dotdotEntry->inode = SERTFS_ROOT_INODE;
    dotdotEntry->recordLength = SERTFS_BLOCK_SIZE - 12;
    dotdotEntry->nameLength = 2;
    dotdotEntry->fileType = static_cast<u8>(SertFsInodeType::Directory);
    dotdotEntry->name[0] = '.';
    dotdotEntry->name[1] = '.';
    dotdotEntry->name[2] = '\0';
    
    if (!writeBlock(sSuperblock.dataBlockStart, sBlockBuffer)) {
        return false;
    }
    
    return true;
}

bool SertFs::mount(const char* mountPoint) {
    if (sMounted || !mountPoint) {
        return false;
    }
    
    if (!readSuperblock()) {
        return false;
    }
    
    if (sSuperblock.magic != SERTFS_MAGIC) {
        return false;
    }
    
    if (sSuperblock.version != SERTFS_VERSION) {
        return false;
    }
    
    stringCopy(sMountPoint, mountPoint, MAX_PATH);
    stringCopy(sCurrentDirectory, "/", MAX_PATH);
    sMounted = true;
    
    sSuperblock.mountCount++;
    sSuperblock.lastMountTime = 0;
    writeSuperblock();
    
    return true;
}

bool SertFs::unmount() {
    if (!sMounted) {
        return false;
    }
    
    for (usize i = 0; i < MAX_FILE_HANDLES; i++) {
        sFileHandles[i].valid = false;
    }
    for (usize i = 0; i < MAX_DIR_HANDLES; i++) {
        sDirHandles[i].valid = false;
    }
    
    writeSuperblock();
    
    sMounted = false;
    sMountPoint[0] = '\0';
    return true;
}

bool SertFs::isMounted() {
    return sMounted;
}

const char* SertFs::mountPoint() {
    return sMountPoint;
}

const char* SertFs::currentDirectory() {
    return sCurrentDirectory;
}

bool SertFs::changeDirectory(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    if (!isDirectory(resolved)) {
        return false;
    }
    
    stringCopy(sCurrentDirectory, resolved, MAX_PATH);
    return true;
}

void SertFs::absolutePath(const char* path, char* result) {
    if (!path || !result) return;
    
    if (path[0] == '/') {
        usize len = stringLength(path);
        usize i = 0;
        usize j = 0;
        
        result[j++] = '/';
        i++;
        
        while (i < len && j < MAX_PATH - 1) {
            if (path[i] == '/') {
                if (j > 1 && result[j-1] != '/') {
                    result[j++] = '/';
                }
                i++;
                continue;
            }
            
            if (path[i] == '.') {
                if (path[i+1] == '/' || path[i+1] == '\0') {
                    i++;
                    if (path[i] == '/') i++;
                    continue;
                }
                
                if (path[i+1] == '.' && (path[i+2] == '/' || path[i+2] == '\0')) {
                    if (j > 1) {
                        j--;
                        while (j > 1 && result[j-1] != '/') {
                            j--;
                        }
                    }
                    i += 2;
                    if (path[i] == '/') i++;
                    continue;
                }
            }
            
            while (i < len && path[i] != '/' && j < MAX_PATH - 1) {
                result[j++] = path[i++];
            }
        }
        
        if (j > 1 && result[j-1] == '/') {
            j--;
        }
        
        result[j] = '\0';
    } else {
        char temp[MAX_PATH];
        usize cwdLen = stringLength(sCurrentDirectory);
        usize pathLen = stringLength(path);
        
        stringCopy(temp, sCurrentDirectory, MAX_PATH);
        
        if (cwdLen > 0 && temp[cwdLen-1] != '/') {
            temp[cwdLen++] = '/';
            temp[cwdLen] = '\0';
        }
        
        if (cwdLen + pathLen < MAX_PATH) {
            stringCopy(temp + cwdLen, path, MAX_PATH - cwdLen);
        }
        
        absolutePath(temp, result);
    }
}

bool SertFs::readSuperblock() {
    if (!readBlock(0, &sSuperblock)) {
        return false;
    }
    return true;
}

bool SertFs::writeSuperblock() {
    return writeBlock(0, &sSuperblock);
}

bool SertFs::readInode(u32 inodeNum, SertFsInode* inode) {
    if (inodeNum == 0 || inodeNum > sSuperblock.totalInodes || !inode) {
        return false;
    }
    
    u32 inodesPerBlock = SERTFS_BLOCK_SIZE / SERTFS_INODE_SIZE;
    u32 blockIndex = (inodeNum - 1) / inodesPerBlock;
    u32 inodeOffset = ((inodeNum - 1) % inodesPerBlock) * SERTFS_INODE_SIZE;
    
    if (!readBlock(sSuperblock.inodeTableBlock + blockIndex, sBlockBuffer)) {
        return false;
    }
    
    memoryCopy(inode, sBlockBuffer + inodeOffset, sizeof(SertFsInode));
    return true;
}

bool SertFs::writeInode(u32 inodeNum, const SertFsInode* inode) {
    if (inodeNum == 0 || inodeNum > sSuperblock.totalInodes || !inode) {
        return false;
    }
    
    u32 inodesPerBlock = SERTFS_BLOCK_SIZE / SERTFS_INODE_SIZE;
    u32 blockIndex = (inodeNum - 1) / inodesPerBlock;
    u32 inodeOffset = ((inodeNum - 1) % inodesPerBlock) * SERTFS_INODE_SIZE;
    
    if (!readBlock(sSuperblock.inodeTableBlock + blockIndex, sBlockBuffer)) {
        return false;
    }
    
    memoryCopy(sBlockBuffer + inodeOffset, inode, sizeof(SertFsInode));
    
    return writeBlock(sSuperblock.inodeTableBlock + blockIndex, sBlockBuffer);
}

bool SertFs::readBlock(u32 blockNum, void* buffer) {
    if (blockNum >= sSuperblock.totalBlocks || !buffer) {
        if (blockNum == 0 && sSuperblock.totalBlocks == 0) {
            u64 lba = 0;
            return disk::ATA::read(sDriveIndex, lba, sectorsPerBlock(), buffer);
        }
        return false;
    }
    
    u64 lba = static_cast<u64>(blockNum) * sectorsPerBlock();
    return disk::ATA::read(sDriveIndex, lba, sectorsPerBlock(), buffer);
}

bool SertFs::writeBlock(u32 blockNum, const void* buffer) {
    if (!buffer) {
        return false;
    }
    
    u64 lba = static_cast<u64>(blockNum) * sectorsPerBlock();
    return disk::ATA::write(sDriveIndex, lba, sectorsPerBlock(), buffer);
}

u32 SertFs::allocateInode() {
    for (u32 block = 0; block < sSuperblock.inodeBitmapBlocks; block++) {
        if (!readBlock(sSuperblock.inodeBitmapBlock + block, sBlockBuffer)) {
            return 0;
        }
        
        for (u32 byte = 0; byte < SERTFS_BLOCK_SIZE; byte++) {
            if (sBlockBuffer[byte] != 0xFF) {
                for (u32 bit = 0; bit < 8; bit++) {
                    if (!(sBlockBuffer[byte] & (1 << bit))) {
                        u32 inodeNum = block * SERTFS_BLOCK_SIZE * 8 + byte * 8 + bit + 1;
                        if (inodeNum > sSuperblock.totalInodes) {
                            return 0;
                        }
                        
                        sBlockBuffer[byte] |= (1 << bit);
                        if (!writeBlock(sSuperblock.inodeBitmapBlock + block, sBlockBuffer)) {
                            return 0;
                        }
                        
                        sSuperblock.freeInodes--;
                        writeSuperblock();
                        
                        return inodeNum;
                    }
                }
            }
        }
    }
    return 0;
}

void SertFs::freeInode(u32 inodeNum) {
    if (inodeNum == 0 || inodeNum > sSuperblock.totalInodes) {
        return;
    }
    
    u32 index = inodeNum - 1;
    u32 block = index / (SERTFS_BLOCK_SIZE * 8);
    u32 byte = (index % (SERTFS_BLOCK_SIZE * 8)) / 8;
    u32 bit = index % 8;
    
    if (!readBlock(sSuperblock.inodeBitmapBlock + block, sBlockBuffer)) {
        return;
    }
    
    sBlockBuffer[byte] &= ~(1 << bit);
    writeBlock(sSuperblock.inodeBitmapBlock + block, sBlockBuffer);
    
    sSuperblock.freeInodes++;
    writeSuperblock();
}

u32 SertFs::allocateBlock() {
    for (u32 block = 0; block < sSuperblock.blockBitmapBlocks; block++) {
        if (!readBlock(sSuperblock.blockBitmapBlock + block, sBlockBuffer)) {
            return SERTFS_INVALID_BLOCK;
        }
        
        for (u32 byte = 0; byte < SERTFS_BLOCK_SIZE; byte++) {
            if (sBlockBuffer[byte] != 0xFF) {
                for (u32 bit = 0; bit < 8; bit++) {
                    if (!(sBlockBuffer[byte] & (1 << bit))) {
                        u32 blockNum = block * SERTFS_BLOCK_SIZE * 8 + byte * 8 + bit;
                        if (blockNum >= sSuperblock.totalBlocks) {
                            return SERTFS_INVALID_BLOCK;
                        }
                        
                        sBlockBuffer[byte] |= (1 << bit);
                        if (!writeBlock(sSuperblock.blockBitmapBlock + block, sBlockBuffer)) {
                            return SERTFS_INVALID_BLOCK;
                        }
                        
                        sSuperblock.freeBlocks--;
                        writeSuperblock();
                        
                        return blockNum;
                    }
                }
            }
        }
    }
    return SERTFS_INVALID_BLOCK;
}

void SertFs::freeBlock(u32 blockNum) {
    if (blockNum >= sSuperblock.totalBlocks || blockNum < sSuperblock.dataBlockStart) {
        return;
    }
    
    u32 block = blockNum / (SERTFS_BLOCK_SIZE * 8);
    u32 byte = (blockNum % (SERTFS_BLOCK_SIZE * 8)) / 8;
    u32 bit = blockNum % 8;
    
    if (!readBlock(sSuperblock.blockBitmapBlock + block, sBlockBuffer)) {
        return;
    }
    
    sBlockBuffer[byte] &= ~(1 << bit);
    writeBlock(sSuperblock.blockBitmapBlock + block, sBlockBuffer);
    
    sSuperblock.freeBlocks++;
    writeSuperblock();
}

u32 SertFs::findInode(const char* path) {
    if (!path || path[0] != '/') {
        return 0;
    }
    
    if (path[0] == '/' && path[1] == '\0') {
        return SERTFS_ROOT_INODE;
    }
    
    u32 currentInode = SERTFS_ROOT_INODE;
    const char* p = path + 1;
    
    while (*p) {
        char component[SERTFS_NAME_MAX + 1];
        usize i = 0;
        while (*p && *p != '/' && i < SERTFS_NAME_MAX) {
            component[i++] = *p++;
        }
        component[i] = '\0';
        
        if (*p == '/') p++;
        
        if (i == 0) continue;
        
        currentInode = findInodeInDir(currentInode, component);
        if (currentInode == 0) {
            return 0;
        }
    }
    
    return currentInode;
}

u32 SertFs::findInodeInDir(u32 dirInode, const char* name) {
    SertFsInode inode;
    if (!readInode(dirInode, &inode)) {
        return 0;
    }
    
    if (static_cast<SertFsInodeType>(inode.type) != SertFsInodeType::Directory) {
        return 0;
    }
    
    u64 offset = 0;
    while (offset < inode.size) {
        u32 blockNum = getBlockForOffset(&inode, offset);
        if (blockNum == SERTFS_INVALID_BLOCK) {
            break;
        }
        
        u8 dirBuffer[SERTFS_BLOCK_SIZE];
        if (!readBlock(blockNum, dirBuffer)) {
            break;
        }
        
        u32 blockOffset = 0;
        while (blockOffset < SERTFS_BLOCK_SIZE && offset + blockOffset < inode.size) {
            SertFsDirEntry* entry = reinterpret_cast<SertFsDirEntry*>(dirBuffer + blockOffset);
            
            if (entry->inode != 0 && entry->nameLength > 0) {
                if (stringCompareN(entry->name, name, entry->nameLength) == 0 &&
                    name[entry->nameLength] == '\0') {
                    return entry->inode;
                }
            }
            
            if (entry->recordLength == 0) {
                break;
            }
            blockOffset += entry->recordLength;
        }
        
        offset += SERTFS_BLOCK_SIZE;
    }
    
    return 0;
}

bool SertFs::addDirEntry(u32 dirInode, u32 entryInode, const char* name, u8 type) {
    SertFsInode inode;
    if (!readInode(dirInode, &inode)) {
        return false;
    }
    
    usize nameLen = stringLength(name);
    u16 neededSize = static_cast<u16>(8 + nameLen + 1);
    neededSize = static_cast<u16>((neededSize + 3) & ~3);
    
    u64 offset = 0;
    while (offset < inode.size) {
        u32 blockNum = getBlockForOffset(&inode, offset);
        if (blockNum == SERTFS_INVALID_BLOCK) {
            break;
        }
        
        u8 dirBuffer[SERTFS_BLOCK_SIZE];
        if (!readBlock(blockNum, dirBuffer)) {
            break;
        }
        
        u32 blockOffset = 0;
        while (blockOffset < SERTFS_BLOCK_SIZE) {
            SertFsDirEntry* entry = reinterpret_cast<SertFsDirEntry*>(dirBuffer + blockOffset);
            
            u16 actualSize = static_cast<u16>(8 + entry->nameLength + 1);
            actualSize = static_cast<u16>((actualSize + 3) & ~3);
            
            u16 freeSpace = entry->recordLength - actualSize;
            
            if (freeSpace >= neededSize) {
                u16 oldRecordLen = entry->recordLength;
                entry->recordLength = actualSize;
                
                SertFsDirEntry* newEntry = reinterpret_cast<SertFsDirEntry*>(dirBuffer + blockOffset + actualSize);
                newEntry->inode = entryInode;
                newEntry->recordLength = oldRecordLen - actualSize;
                newEntry->nameLength = static_cast<u8>(nameLen);
                newEntry->fileType = type;
                stringCopy(newEntry->name, name, SERTFS_NAME_MAX + 1);
                
                return writeBlock(blockNum, dirBuffer);
            }
            
            if (entry->recordLength == 0) {
                break;
            }
            blockOffset += entry->recordLength;
        }
        
        offset += SERTFS_BLOCK_SIZE;
    }
    
    u32 newBlock = allocateBlockForOffset(&inode, inode.size);
    if (newBlock == SERTFS_INVALID_BLOCK) {
        return false;
    }
    
    u8 dirBuffer[SERTFS_BLOCK_SIZE];
    memorySet(dirBuffer, 0, SERTFS_BLOCK_SIZE);
    
    SertFsDirEntry* newEntry = reinterpret_cast<SertFsDirEntry*>(dirBuffer);
    newEntry->inode = entryInode;
    newEntry->recordLength = SERTFS_BLOCK_SIZE;
    newEntry->nameLength = static_cast<u8>(nameLen);
    newEntry->fileType = type;
    stringCopy(newEntry->name, name, SERTFS_NAME_MAX + 1);
    
    if (!writeBlock(newBlock, dirBuffer)) {
        return false;
    }
    
    inode.size += SERTFS_BLOCK_SIZE;
    return writeInode(dirInode, &inode);
}

bool SertFs::removeDirEntry(u32 dirInode, const char* name) {
    SertFsInode inode;
    if (!readInode(dirInode, &inode)) {
        return false;
    }
    
    u64 offset = 0;
    while (offset < inode.size) {
        u32 blockNum = getBlockForOffset(&inode, offset);
        if (blockNum == SERTFS_INVALID_BLOCK) {
            break;
        }
        
        u8 dirBuffer[SERTFS_BLOCK_SIZE];
        if (!readBlock(blockNum, dirBuffer)) {
            break;
        }
        
        u32 blockOffset = 0;
        SertFsDirEntry* prevEntry = nullptr;
        
        while (blockOffset < SERTFS_BLOCK_SIZE && offset + blockOffset < inode.size) {
            SertFsDirEntry* entry = reinterpret_cast<SertFsDirEntry*>(dirBuffer + blockOffset);
            
            if (entry->inode != 0 && entry->nameLength > 0) {
                if (stringCompareN(entry->name, name, entry->nameLength) == 0 &&
                    name[entry->nameLength] == '\0') {
                    
                    if (prevEntry) {
                        prevEntry->recordLength += entry->recordLength;
                    } else {
                        entry->inode = 0;
                    }
                    
                    return writeBlock(blockNum, dirBuffer);
                }
            }
            
            if (entry->recordLength == 0) {
                break;
            }
            
            prevEntry = entry;
            blockOffset += entry->recordLength;
        }
        
        offset += SERTFS_BLOCK_SIZE;
    }
    
    return false;
}

u32 SertFs::getBlockForOffset(SertFsInode* inode, u64 offset) {
    u32 blockIndex = static_cast<u32>(offset / SERTFS_BLOCK_SIZE);
    
    if (blockIndex < SERTFS_DIRECT_BLOCKS) {
        return inode->directBlocks[blockIndex];
    }
    
    blockIndex -= SERTFS_DIRECT_BLOCKS;
    u32 pointersPerBlock = SERTFS_BLOCK_SIZE / sizeof(u32);
    
    if (blockIndex < pointersPerBlock) {
        if (inode->indirectBlock == 0) {
            return SERTFS_INVALID_BLOCK;
        }
        
        u32 indirectBuffer[SERTFS_BLOCK_SIZE / sizeof(u32)];
        if (!readBlock(inode->indirectBlock, indirectBuffer)) {
            return SERTFS_INVALID_BLOCK;
        }
        
        return indirectBuffer[blockIndex];
    }
    
    return SERTFS_INVALID_BLOCK;
}

u32 SertFs::allocateBlockForOffset(SertFsInode* inode, u64 offset) {
    u32 blockIndex = static_cast<u32>(offset / SERTFS_BLOCK_SIZE);
    
    if (blockIndex < SERTFS_DIRECT_BLOCKS) {
        if (inode->directBlocks[blockIndex] == 0) {
            inode->directBlocks[blockIndex] = allocateBlock();
        }
        return inode->directBlocks[blockIndex];
    }
    
    blockIndex -= SERTFS_DIRECT_BLOCKS;
    u32 pointersPerBlock = SERTFS_BLOCK_SIZE / sizeof(u32);
    
    if (blockIndex < pointersPerBlock) {
        if (inode->indirectBlock == 0) {
            inode->indirectBlock = allocateBlock();
            if (inode->indirectBlock == SERTFS_INVALID_BLOCK) {
                return SERTFS_INVALID_BLOCK;
            }
            
            u32 indirectBuffer[SERTFS_BLOCK_SIZE / sizeof(u32)];
            memorySet(indirectBuffer, 0, SERTFS_BLOCK_SIZE);
            writeBlock(inode->indirectBlock, indirectBuffer);
        }
        
        u32 indirectBuffer[SERTFS_BLOCK_SIZE / sizeof(u32)];
        if (!readBlock(inode->indirectBlock, indirectBuffer)) {
            return SERTFS_INVALID_BLOCK;
        }
        
        if (indirectBuffer[blockIndex] == 0) {
            indirectBuffer[blockIndex] = allocateBlock();
            writeBlock(inode->indirectBlock, indirectBuffer);
        }
        
        return indirectBuffer[blockIndex];
    }
    
    return SERTFS_INVALID_BLOCK;
}

void SertFs::freeInodeBlocks(SertFsInode* inode) {
    for (u32 i = 0; i < SERTFS_DIRECT_BLOCKS; i++) {
        if (inode->directBlocks[i] != 0) {
            freeBlock(inode->directBlocks[i]);
            inode->directBlocks[i] = 0;
        }
    }
    
    if (inode->indirectBlock != 0) {
        u32 indirectBuffer[SERTFS_BLOCK_SIZE / sizeof(u32)];
        if (readBlock(inode->indirectBlock, indirectBuffer)) {
            u32 pointersPerBlock = SERTFS_BLOCK_SIZE / sizeof(u32);
            for (u32 i = 0; i < pointersPerBlock; i++) {
                if (indirectBuffer[i] != 0) {
                    freeBlock(indirectBuffer[i]);
                }
            }
        }
        freeBlock(inode->indirectBlock);
        inode->indirectBlock = 0;
    }
}

const char* SertFs::skipMountPoint(const char* path) {
    usize mountLen = stringLength(sMountPoint);
    
    if (mountLen > 0 && path[0] == '/' && sMountPoint[0] == '/') {
        const char* p = path;
        const char* m = sMountPoint;
        
        while (*m && *p == *m) {
            p++;
            m++;
        }
        
        if (*m == '\0') {
            if (*p == '\0') return "/";
            if (*p == '/') return p;
        }
    }
    
    return path;
}

void SertFs::splitPath(const char* path, char* parent, char* name) {
    usize len = stringLength(path);
    
    while (len > 1 && path[len - 1] == '/') {
        len--;
    }
    
    usize lastSlash = len;
    while (lastSlash > 0 && path[lastSlash - 1] != '/') {
        lastSlash--;
    }
    
    if (lastSlash == 0) {
        parent[0] = '/';
        parent[1] = '\0';
        stringCopy(name, path, MAX_FILENAME);
    } else if (lastSlash == 1) {
        parent[0] = '/';
        parent[1] = '\0';
        stringCopy(name, path + 1, MAX_FILENAME);
    } else {
        for (usize i = 0; i < lastSlash - 1 && i < MAX_PATH - 1; i++) {
            parent[i] = path[i];
        }
        parent[lastSlash - 1] = '\0';
        stringCopy(name, path + lastSlash, MAX_FILENAME);
    }
}

FileHandle SertFs::open(const char* path, u32 flags) {
    FileHandle handle = {nullptr, 0, false};
    
    if (!sMounted || !path) {
        return handle;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    u32 inodeNum = findInode(resolved);
    
    if (inodeNum == 0) {
        if (flags & O_CREATE) {
            if (!createFile(resolved)) {
                return handle;
            }
            inodeNum = findInode(resolved);
        }
        if (inodeNum == 0) {
            return handle;
        }
    }
    
    SertFsInode inode;
    if (!readInode(inodeNum, &inode)) {
        return handle;
    }
    
    if (static_cast<SertFsInodeType>(inode.type) != SertFsInodeType::Regular) {
        return handle;
    }
    
    if (flags & O_TRUNCATE) {
        freeInodeBlocks(&inode);
        inode.size = 0;
        writeInode(inodeNum, &inode);
    }
    
    for (usize i = 0; i < MAX_FILE_HANDLES; i++) {
        if (!sFileHandles[i].valid) {
            sFileHandles[i].inode = inodeNum;
            sFileHandles[i].position = (flags & O_APPEND) ? inode.size : 0;
            sFileHandles[i].flags = flags;
            sFileHandles[i].valid = true;
            sFileHandles[i].inodeData = inode;
            
            handle.fsData = &sFileHandles[i];
            handle.flags = flags;
            handle.valid = true;
            return handle;
        }
    }
    
    return handle;
}

i64 SertFs::read(FileHandle* handle, void* buffer, usize size) {
    if (!handle || !handle->valid || !buffer) {
        return -1;
    }
    
    auto* fh = static_cast<SertFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid) {
        return -1;
    }
    
    if (fh->position >= fh->inodeData.size) {
        return 0;
    }
    
    usize bytesToRead = size;
    if (fh->position + bytesToRead > fh->inodeData.size) {
        bytesToRead = static_cast<usize>(fh->inodeData.size - fh->position);
    }
    
    u8* buf = static_cast<u8*>(buffer);
    usize bytesRead = 0;
    
    while (bytesRead < bytesToRead) {
        u32 blockNum = getBlockForOffset(&fh->inodeData, fh->position);
        if (blockNum == SERTFS_INVALID_BLOCK || blockNum == 0) {
            break;
        }
        
        u8 blockBuffer[SERTFS_BLOCK_SIZE];
        if (!readBlock(blockNum, blockBuffer)) {
            break;
        }
        
        u32 blockOffset = static_cast<u32>(fh->position % SERTFS_BLOCK_SIZE);
        u32 bytesInBlock = SERTFS_BLOCK_SIZE - blockOffset;
        if (bytesInBlock > bytesToRead - bytesRead) {
            bytesInBlock = static_cast<u32>(bytesToRead - bytesRead);
        }
        
        memoryCopy(buf + bytesRead, blockBuffer + blockOffset, bytesInBlock);
        bytesRead += bytesInBlock;
        fh->position += bytesInBlock;
    }
    
    return static_cast<i64>(bytesRead);
}

i64 SertFs::write(FileHandle* handle, const void* buffer, usize size) {
    if (!handle || !handle->valid || !buffer) {
        return -1;
    }
    
    auto* fh = static_cast<SertFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid) {
        return -1;
    }
    
    const u8* buf = static_cast<const u8*>(buffer);
    usize bytesWritten = 0;
    
    while (bytesWritten < size) {
        u32 blockNum = allocateBlockForOffset(&fh->inodeData, fh->position);
        if (blockNum == SERTFS_INVALID_BLOCK) {
            break;
        }
        
        u8 blockBuffer[SERTFS_BLOCK_SIZE];
        u32 blockOffset = static_cast<u32>(fh->position % SERTFS_BLOCK_SIZE);
        
        if (blockOffset != 0 || size - bytesWritten < SERTFS_BLOCK_SIZE) {
            if (!readBlock(blockNum, blockBuffer)) {
                memorySet(blockBuffer, 0, SERTFS_BLOCK_SIZE);
            }
        }
        
        u32 bytesInBlock = SERTFS_BLOCK_SIZE - blockOffset;
        if (bytesInBlock > size - bytesWritten) {
            bytesInBlock = static_cast<u32>(size - bytesWritten);
        }
        
        memoryCopy(blockBuffer + blockOffset, buf + bytesWritten, bytesInBlock);
        
        if (!writeBlock(blockNum, blockBuffer)) {
            break;
        }
        
        bytesWritten += bytesInBlock;
        fh->position += bytesInBlock;
        
        if (fh->position > fh->inodeData.size) {
            fh->inodeData.size = fh->position;
        }
    }
    
    writeInode(fh->inode, &fh->inodeData);
    
    return static_cast<i64>(bytesWritten);
}

i64 SertFs::seek(FileHandle* handle, i64 offset, SeekMode mode) {
    if (!handle || !handle->valid) {
        return -1;
    }
    
    auto* fh = static_cast<SertFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid) {
        return -1;
    }
    
    i64 newPos;
    switch (mode) {
        case SeekMode::Set:
            newPos = offset;
            break;
        case SeekMode::Current:
            newPos = static_cast<i64>(fh->position) + offset;
            break;
        case SeekMode::End:
            newPos = static_cast<i64>(fh->inodeData.size) + offset;
            break;
        default:
            return -1;
    }
    
    if (newPos < 0) {
        return -1;
    }
    
    fh->position = static_cast<u64>(newPos);
    return static_cast<i64>(fh->position);
}

i64 SertFs::tell(FileHandle* handle) {
    if (!handle || !handle->valid) {
        return -1;
    }
    
    auto* fh = static_cast<SertFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid) {
        return -1;
    }
    
    return static_cast<i64>(fh->position);
}

bool SertFs::close(FileHandle* handle) {
    if (!handle || !handle->valid) {
        return false;
    }
    
    auto* fh = static_cast<SertFsFileHandle*>(handle->fsData);
    if (!fh || !fh->valid) {
        return false;
    }
    
    writeInode(fh->inode, &fh->inodeData);
    
    fh->valid = false;
    handle->valid = false;
    handle->fsData = nullptr;
    
    return true;
}

DirHandle SertFs::openDir(const char* path) {
    DirHandle handle = {nullptr, false};
    
    if (!sMounted || !path) {
        return handle;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    u32 inodeNum = findInode(resolved);
    
    if (inodeNum == 0) {
        return handle;
    }
    
    SertFsInode inode;
    if (!readInode(inodeNum, &inode)) {
        return handle;
    }
    
    if (static_cast<SertFsInodeType>(inode.type) != SertFsInodeType::Directory) {
        return handle;
    }
    
    for (usize i = 0; i < MAX_DIR_HANDLES; i++) {
        if (!sDirHandles[i].valid) {
            sDirHandles[i].inode = inodeNum;
            sDirHandles[i].position = 0;
            sDirHandles[i].valid = true;
            sDirHandles[i].inodeData = inode;
            
            handle.fsData = &sDirHandles[i];
            handle.valid = true;
            return handle;
        }
    }
    
    return handle;
}

bool SertFs::readDir(DirHandle* handle, DirEntry* entry) {
    if (!handle || !handle->valid || !entry) {
        return false;
    }
    
    auto* dh = static_cast<SertFsDirHandle*>(handle->fsData);
    if (!dh || !dh->valid) {
        return false;
    }
    
    while (dh->position < dh->inodeData.size) {
        u32 blockNum = getBlockForOffset(&dh->inodeData, dh->position);
        if (blockNum == SERTFS_INVALID_BLOCK || blockNum == 0) {
            return false;
        }
        
        u8 dirBuffer[SERTFS_BLOCK_SIZE];
        if (!readBlock(blockNum, dirBuffer)) {
            return false;
        }
        
        u32 blockOffset = static_cast<u32>(dh->position % SERTFS_BLOCK_SIZE);
        
        while (blockOffset < SERTFS_BLOCK_SIZE) {
            SertFsDirEntry* dirEntry = reinterpret_cast<SertFsDirEntry*>(dirBuffer + blockOffset);
            
            if (dirEntry->recordLength == 0) {
                dh->position = ((dh->position / SERTFS_BLOCK_SIZE) + 1) * SERTFS_BLOCK_SIZE;
                break;
            }
            
            dh->position += dirEntry->recordLength;
            blockOffset += dirEntry->recordLength;
            
            if (dirEntry->inode != 0 && dirEntry->nameLength > 0) {
                for (usize i = 0; i < dirEntry->nameLength && i < MAX_FILENAME - 1; i++) {
                    entry->name[i] = dirEntry->name[i];
                }
                entry->name[dirEntry->nameLength] = '\0';
                
                switch (static_cast<SertFsInodeType>(dirEntry->fileType)) {
                    case SertFsInodeType::Regular:
                        entry->type = FileType::Regular;
                        break;
                    case SertFsInodeType::Directory:
                        entry->type = FileType::Directory;
                        break;
                    case SertFsInodeType::Symlink:
                        entry->type = FileType::Symlink;
                        break;
                    default:
                        entry->type = FileType::Unknown;
                        break;
                }
                
                entry->inode = dirEntry->inode;
                return true;
            }
        }
    }
    
    return false;
}

bool SertFs::closeDir(DirHandle* handle) {
    if (!handle || !handle->valid) {
        return false;
    }
    
    auto* dh = static_cast<SertFsDirHandle*>(handle->fsData);
    if (!dh || !dh->valid) {
        return false;
    }
    
    dh->valid = false;
    handle->valid = false;
    handle->fsData = nullptr;
    
    return true;
}

bool SertFs::exists(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    return findInode(resolved) != 0;
}

bool SertFs::isFile(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    u32 inodeNum = findInode(resolved);
    if (inodeNum == 0) {
        return false;
    }
    
    SertFsInode inode;
    if (!readInode(inodeNum, &inode)) {
        return false;
    }
    
    return static_cast<SertFsInodeType>(inode.type) == SertFsInodeType::Regular;
}

bool SertFs::isDirectory(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    u32 inodeNum = findInode(resolved);
    if (inodeNum == 0) {
        return false;
    }
    
    SertFsInode inode;
    if (!readInode(inodeNum, &inode)) {
        return false;
    }
    
    return static_cast<SertFsInodeType>(inode.type) == SertFsInodeType::Directory;
}

bool SertFs::createFile(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    if (findInode(resolved) != 0) {
        return false;
    }
    
    char parentPath[MAX_PATH];
    char fileName[MAX_FILENAME];
    splitPath(resolved, parentPath, fileName);
    
    u32 parentInode = findInode(parentPath);
    if (parentInode == 0) {
        return false;
    }
    
    u32 newInode = allocateInode();
    if (newInode == 0) {
        return false;
    }
    
    SertFsInode inode;
    memorySet(&inode, 0, sizeof(inode));
    inode.type = static_cast<u16>(SertFsInodeType::Regular);
    inode.permissions = 0644;
    inode.linkCount = 1;
    
    if (!writeInode(newInode, &inode)) {
        freeInode(newInode);
        return false;
    }
    
    if (!addDirEntry(parentInode, newInode, fileName, static_cast<u8>(SertFsInodeType::Regular))) {
        freeInode(newInode);
        return false;
    }
    
    return true;
}

bool SertFs::createDirectory(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    if (findInode(resolved) != 0) {
        return false;
    }
    
    char parentPath[MAX_PATH];
    char dirName[MAX_FILENAME];
    splitPath(resolved, parentPath, dirName);
    
    u32 parentInode = findInode(parentPath);
    if (parentInode == 0) {
        return false;
    }
    
    u32 newInode = allocateInode();
    if (newInode == 0) {
        return false;
    }
    
    u32 dataBlock = allocateBlock();
    if (dataBlock == SERTFS_INVALID_BLOCK) {
        freeInode(newInode);
        return false;
    }
    
    SertFsInode inode;
    memorySet(&inode, 0, sizeof(inode));
    inode.type = static_cast<u16>(SertFsInodeType::Directory);
    inode.permissions = 0755;
    inode.linkCount = 2;
    inode.size = SERTFS_BLOCK_SIZE;
    inode.directBlocks[0] = dataBlock;
    
    u8 dirBuffer[SERTFS_BLOCK_SIZE];
    memorySet(dirBuffer, 0, SERTFS_BLOCK_SIZE);
    
    SertFsDirEntry* dotEntry = reinterpret_cast<SertFsDirEntry*>(dirBuffer);
    dotEntry->inode = newInode;
    dotEntry->recordLength = 12;
    dotEntry->nameLength = 1;
    dotEntry->fileType = static_cast<u8>(SertFsInodeType::Directory);
    dotEntry->name[0] = '.';
    
    SertFsDirEntry* dotdotEntry = reinterpret_cast<SertFsDirEntry*>(dirBuffer + 12);
    dotdotEntry->inode = parentInode;
    dotdotEntry->recordLength = SERTFS_BLOCK_SIZE - 12;
    dotdotEntry->nameLength = 2;
    dotdotEntry->fileType = static_cast<u8>(SertFsInodeType::Directory);
    dotdotEntry->name[0] = '.';
    dotdotEntry->name[1] = '.';
    
    if (!writeBlock(dataBlock, dirBuffer)) {
        freeBlock(dataBlock);
        freeInode(newInode);
        return false;
    }
    
    if (!writeInode(newInode, &inode)) {
        freeBlock(dataBlock);
        freeInode(newInode);
        return false;
    }
    
    if (!addDirEntry(parentInode, newInode, dirName, static_cast<u8>(SertFsInodeType::Directory))) {
        freeBlock(dataBlock);
        freeInode(newInode);
        return false;
    }
    
    SertFsInode parentInodeData;
    if (readInode(parentInode, &parentInodeData)) {
        parentInodeData.linkCount++;
        writeInode(parentInode, &parentInodeData);
    }
    
    return true;
}

bool SertFs::remove(const char* path) {
    if (!sMounted || !path) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    if (stringCompare(resolved, "/") == 0) {
        return false;
    }
    
    u32 inodeNum = findInode(resolved);
    if (inodeNum == 0) {
        return false;
    }
    
    SertFsInode inode;
    if (!readInode(inodeNum, &inode)) {
        return false;
    }
    
    if (static_cast<SertFsInodeType>(inode.type) == SertFsInodeType::Directory) {
        if (inode.size > SERTFS_BLOCK_SIZE) {
            return false;
        }
        
        u8 dirBuffer[SERTFS_BLOCK_SIZE];
        if (!readBlock(inode.directBlocks[0], dirBuffer)) {
            return false;
        }
        
        u32 entryCount = 0;
        u32 offset = 0;
        while (offset < SERTFS_BLOCK_SIZE) {
            SertFsDirEntry* entry = reinterpret_cast<SertFsDirEntry*>(dirBuffer + offset);
            if (entry->recordLength == 0) break;
            if (entry->inode != 0) entryCount++;
            offset += entry->recordLength;
        }
        
        if (entryCount > 2) {
            return false;
        }
    }
    
    char parentPath[MAX_PATH];
    char name[MAX_FILENAME];
    splitPath(resolved, parentPath, name);
    
    u32 parentInode = findInode(parentPath);
    if (parentInode == 0) {
        return false;
    }
    
    if (!removeDirEntry(parentInode, name)) {
        return false;
    }
    
    freeInodeBlocks(&inode);
    freeInode(inodeNum);
    
    if (static_cast<SertFsInodeType>(inode.type) == SertFsInodeType::Directory) {
        SertFsInode parentInodeData;
        if (readInode(parentInode, &parentInodeData)) {
            if (parentInodeData.linkCount > 0) {
                parentInodeData.linkCount--;
                writeInode(parentInode, &parentInodeData);
            }
        }
    }
    
    return true;
}

bool SertFs::rename(const char* oldPath, const char* newPath) {
    if (!sMounted || !oldPath || !newPath) {
        return false;
    }
    
    char oldResolved[MAX_PATH];
    char newResolved[MAX_PATH];
    absolutePath(oldPath, oldResolved);
    absolutePath(newPath, newResolved);
    
    u32 inodeNum = findInode(oldResolved);
    if (inodeNum == 0) {
        return false;
    }
    
    if (findInode(newResolved) != 0) {
        return false;
    }
    
    SertFsInode inode;
    if (!readInode(inodeNum, &inode)) {
        return false;
    }
    
    char oldParentPath[MAX_PATH];
    char oldName[MAX_FILENAME];
    splitPath(oldResolved, oldParentPath, oldName);
    
    char newParentPath[MAX_PATH];
    char newName[MAX_FILENAME];
    splitPath(newResolved, newParentPath, newName);
    
    u32 oldParentInode = findInode(oldParentPath);
    u32 newParentInode = findInode(newParentPath);
    
    if (oldParentInode == 0 || newParentInode == 0) {
        return false;
    }
    
    u8 fileType = static_cast<u8>(inode.type);
    
    if (!addDirEntry(newParentInode, inodeNum, newName, fileType)) {
        return false;
    }
    
    if (!removeDirEntry(oldParentInode, oldName)) {
        removeDirEntry(newParentInode, newName);
        return false;
    }
    
    if (static_cast<SertFsInodeType>(inode.type) == SertFsInodeType::Directory &&
        oldParentInode != newParentInode) {
        
        u8 dirBuffer[SERTFS_BLOCK_SIZE];
        if (readBlock(inode.directBlocks[0], dirBuffer)) {
            SertFsDirEntry* dotdotEntry = reinterpret_cast<SertFsDirEntry*>(dirBuffer + 12);
            dotdotEntry->inode = newParentInode;
            writeBlock(inode.directBlocks[0], dirBuffer);
        }
        
        SertFsInode oldParentData;
        if (readInode(oldParentInode, &oldParentData)) {
            if (oldParentData.linkCount > 0) {
                oldParentData.linkCount--;
                writeInode(oldParentInode, &oldParentData);
            }
        }
        
        SertFsInode newParentData;
        if (readInode(newParentInode, &newParentData)) {
            newParentData.linkCount++;
            writeInode(newParentInode, &newParentData);
        }
    }
    
    return true;
}

bool SertFs::getInfo(const char* path, FileInfo* info) {
    if (!sMounted || !path || !info) {
        return false;
    }
    
    char resolved[MAX_PATH];
    absolutePath(path, resolved);
    
    u32 inodeNum = findInode(resolved);
    if (inodeNum == 0) {
        return false;
    }
    
    SertFsInode inode;
    if (!readInode(inodeNum, &inode)) {
        return false;
    }
    
    char name[MAX_FILENAME];
    char parent[MAX_PATH];
    splitPath(resolved, parent, name);
    stringCopy(info->name, name, MAX_FILENAME);
    
    switch (static_cast<SertFsInodeType>(inode.type)) {
        case SertFsInodeType::Regular:
            info->type = FileType::Regular;
            break;
        case SertFsInodeType::Directory:
            info->type = FileType::Directory;
            break;
        case SertFsInodeType::Symlink:
            info->type = FileType::Symlink;
            break;
        default:
            info->type = FileType::Unknown;
            break;
    }
    
    info->size = inode.size;
    info->createdTime = inode.createdTime;
    info->modifiedTime = inode.modifiedTime;
    info->accessedTime = inode.accessedTime;
    info->permissions = inode.permissions;
    info->uid = inode.uid;
    info->gid = inode.gid;
    
    return true;
}

u64 SertFs::freeSpace() {
    if (!sMounted) {
        return 0;
    }
    return static_cast<u64>(sSuperblock.freeBlocks) * SERTFS_BLOCK_SIZE;
}

u64 SertFs::totalSpace() {
    if (!sMounted) {
        return 0;
    }
    return static_cast<u64>(sSuperblock.totalBlocks) * SERTFS_BLOCK_SIZE;
}

}
