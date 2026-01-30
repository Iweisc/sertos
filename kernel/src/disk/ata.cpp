#include "../../include/disk/ata.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::disk {

AtaDrive ATA::sDrives[MAX_DRIVES];
u8 ATA::sDriveCount = 0;
bool ATA::sInitialized = false;

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

void copyString(char* dest, const u16* src, usize wordCount) {
    for (usize i = 0; i < wordCount; i++) {
        dest[i * 2] = static_cast<char>(src[i] >> 8);
        dest[i * 2 + 1] = static_cast<char>(src[i] & 0xFF);
    }
    dest[wordCount * 2] = '\0';
    
    isize end = wordCount * 2 - 1;
    while (end >= 0 && dest[end] == ' ') {
        dest[end--] = '\0';
    }
}

}

void ATA::initialize() {
    if (sInitialized) {
        return;
    }
    
    memorySet(sDrives, 0, sizeof(sDrives));
    sDriveCount = 0;
    
    detectDrive(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, true);
    detectDrive(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, false);
    detectDrive(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, true);
    detectDrive(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, false);
    
    sInitialized = true;
}

bool ATA::isInitialized() {
    return sInitialized;
}

void ATA::detectDrive(u16 ioBase, u16 ctrlBase, bool isMaster) {
    if (sDriveCount >= MAX_DRIVES) {
        return;
    }
    
    AtaDrive drive;
    memorySet(&drive, 0, sizeof(drive));
    drive.ioBase = ioBase;
    drive.ctrlBase = ctrlBase;
    drive.isMaster = isMaster;
    
    if (identify(ioBase, isMaster, &drive)) {
        sDrives[sDriveCount++] = drive;
    }
}

bool ATA::identify(u16 ioBase, bool isMaster, AtaDrive* drive) {
    selectDrive(ioBase, isMaster, false);
    
    cpu::outb(ioBase + ATA_REG_SECCOUNT, 0);
    cpu::outb(ioBase + ATA_REG_LBA_LO, 0);
    cpu::outb(ioBase + ATA_REG_LBA_MID, 0);
    cpu::outb(ioBase + ATA_REG_LBA_HI, 0);
    
    cpu::outb(ioBase + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    delay400ns(ioBase);
    
    u8 status = cpu::inb(ioBase + ATA_REG_STATUS);
    if (status == 0) {
        drive->present = false;
        drive->type = AtaDriveType::None;
        return false;
    }
    
    while (status & ATA_STATUS_BSY) {
        status = cpu::inb(ioBase + ATA_REG_STATUS);
    }
    
    u8 lbaMid = cpu::inb(ioBase + ATA_REG_LBA_MID);
    u8 lbaHi = cpu::inb(ioBase + ATA_REG_LBA_HI);
    
    if (lbaMid != 0 || lbaHi != 0) {
        if (lbaMid == 0x14 && lbaHi == 0xEB) {
            drive->present = true;
            drive->type = AtaDriveType::ATAPI;
            return false;
        }
        drive->present = false;
        drive->type = AtaDriveType::None;
        return false;
    }
    
    while (!(status & ATA_STATUS_DRQ) && !(status & ATA_STATUS_ERR)) {
        status = cpu::inb(ioBase + ATA_REG_STATUS);
    }
    
    if (status & ATA_STATUS_ERR) {
        drive->present = false;
        drive->type = AtaDriveType::None;
        return false;
    }
    
    u16 identifyData[256];
    for (int i = 0; i < 256; i++) {
        identifyData[i] = cpu::inw(ioBase + ATA_REG_DATA);
    }
    
    drive->present = true;
    drive->type = AtaDriveType::ATA;
    
    copyString(drive->serial, &identifyData[10], 10);
    copyString(drive->model, &identifyData[27], 20);
    
    if (identifyData[83] & (1 << 10)) {
        drive->supportsLba48 = true;
        drive->sectorCount = static_cast<u64>(identifyData[100]) |
                            (static_cast<u64>(identifyData[101]) << 16) |
                            (static_cast<u64>(identifyData[102]) << 32) |
                            (static_cast<u64>(identifyData[103]) << 48);
    } else {
        drive->supportsLba48 = false;
        drive->sectorCount = static_cast<u64>(identifyData[60]) |
                            (static_cast<u64>(identifyData[61]) << 16);
    }
    
    return true;
}

void ATA::selectDrive(u16 ioBase, bool isMaster, bool lba) {
    u8 driveSelect = isMaster ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE;
    if (lba) {
        driveSelect |= ATA_DRIVE_LBA;
    }
    cpu::outb(ioBase + ATA_REG_DRIVE, driveSelect);
    delay400ns(ioBase);
}

bool ATA::waitReady(u16 ioBase) {
    for (int i = 0; i < 100000; i++) {
        u8 status = cpu::inb(ioBase + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return true;
        }
    }
    return false;
}

bool ATA::waitDrq(u16 ioBase) {
    for (int i = 0; i < 100000; i++) {
        u8 status = cpu::inb(ioBase + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            return false;
        }
        if (status & ATA_STATUS_DRQ) {
            return true;
        }
    }
    return false;
}

void ATA::softReset(u16 ctrlBase) {
    cpu::outb(ctrlBase, 0x04);
    cpu::io_wait();
    cpu::io_wait();
    cpu::io_wait();
    cpu::io_wait();
    cpu::outb(ctrlBase, 0x00);
}

void ATA::delay400ns(u16 ioBase) {
    cpu::inb(ioBase + ATA_REG_STATUS);
    cpu::inb(ioBase + ATA_REG_STATUS);
    cpu::inb(ioBase + ATA_REG_STATUS);
    cpu::inb(ioBase + ATA_REG_STATUS);
}

bool ATA::read(u8 driveIndex, u64 lba, u32 sectorCount, void* buffer) {
    if (!sInitialized || driveIndex >= sDriveCount || !buffer) {
        return false;
    }
    
    AtaDrive* drive = &sDrives[driveIndex];
    if (!drive->present || drive->type != AtaDriveType::ATA) {
        return false;
    }
    
    if (lba + sectorCount > drive->sectorCount) {
        return false;
    }
    
    u8* buf = static_cast<u8*>(buffer);
    u64 currentLba = lba;
    u32 remaining = sectorCount;
    
    while (remaining > 0) {
        if (drive->supportsLba48) {
            u16 count = remaining > 65535 ? 65535 : static_cast<u16>(remaining);
            if (!readSectors48(drive, currentLba, count, buf)) {
                return false;
            }
            buf += count * SECTOR_SIZE;
            currentLba += count;
            remaining -= count;
        } else {
            u8 count = remaining > 255 ? 255 : static_cast<u8>(remaining);
            if (currentLba > 0x0FFFFFFF) {
                return false;
            }
            if (!readSectors28(drive, static_cast<u32>(currentLba), count, buf)) {
                return false;
            }
            buf += count * SECTOR_SIZE;
            currentLba += count;
            remaining -= count;
        }
    }
    
    return true;
}

bool ATA::write(u8 driveIndex, u64 lba, u32 sectorCount, const void* buffer) {
    if (!sInitialized || driveIndex >= sDriveCount || !buffer) {
        return false;
    }
    
    AtaDrive* drive = &sDrives[driveIndex];
    if (!drive->present || drive->type != AtaDriveType::ATA) {
        return false;
    }
    
    if (lba + sectorCount > drive->sectorCount) {
        return false;
    }
    
    const u8* buf = static_cast<const u8*>(buffer);
    u64 currentLba = lba;
    u32 remaining = sectorCount;
    
    while (remaining > 0) {
        if (drive->supportsLba48) {
            u16 count = remaining > 65535 ? 65535 : static_cast<u16>(remaining);
            if (!writeSectors48(drive, currentLba, count, buf)) {
                return false;
            }
            buf += count * SECTOR_SIZE;
            currentLba += count;
            remaining -= count;
        } else {
            u8 count = remaining > 255 ? 255 : static_cast<u8>(remaining);
            if (currentLba > 0x0FFFFFFF) {
                return false;
            }
            if (!writeSectors28(drive, static_cast<u32>(currentLba), count, buf)) {
                return false;
            }
            buf += count * SECTOR_SIZE;
            currentLba += count;
            remaining -= count;
        }
    }
    
    return true;
}

bool ATA::readSectors28(AtaDrive* drive, u32 lba, u8 count, void* buffer) {
    u16 ioBase = drive->ioBase;
    
    if (!waitReady(ioBase)) {
        return false;
    }
    
    u8 driveSelect = drive->isMaster ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE;
    driveSelect |= ATA_DRIVE_LBA;
    driveSelect |= (lba >> 24) & 0x0F;
    
    cpu::outb(ioBase + ATA_REG_DRIVE, driveSelect);
    delay400ns(ioBase);
    
    cpu::outb(ioBase + ATA_REG_SECCOUNT, count == 0 ? 0 : count);
    cpu::outb(ioBase + ATA_REG_LBA_LO, lba & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    
    cpu::outb(ioBase + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    u16* buf = static_cast<u16*>(buffer);
    u32 sectorsToRead = count == 0 ? 256 : count;
    
    for (u32 s = 0; s < sectorsToRead; s++) {
        if (!waitDrq(ioBase)) {
            return false;
        }
        
        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = cpu::inw(ioBase + ATA_REG_DATA);
        }
    }
    
    return true;
}

bool ATA::readSectors48(AtaDrive* drive, u64 lba, u16 count, void* buffer) {
    u16 ioBase = drive->ioBase;
    
    if (!waitReady(ioBase)) {
        return false;
    }
    
    u8 driveSelect = drive->isMaster ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE;
    driveSelect |= ATA_DRIVE_LBA;
    
    cpu::outb(ioBase + ATA_REG_DRIVE, driveSelect);
    delay400ns(ioBase);
    
    cpu::outb(ioBase + ATA_REG_SECCOUNT, (count >> 8) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_LO, (lba >> 24) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_MID, (lba >> 32) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_HI, (lba >> 40) & 0xFF);
    
    cpu::outb(ioBase + ATA_REG_SECCOUNT, count & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_LO, lba & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    
    cpu::outb(ioBase + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
    
    u16* buf = static_cast<u16*>(buffer);
    u32 sectorsToRead = count == 0 ? 65536 : count;
    
    for (u32 s = 0; s < sectorsToRead; s++) {
        if (!waitDrq(ioBase)) {
            return false;
        }
        
        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = cpu::inw(ioBase + ATA_REG_DATA);
        }
    }
    
    return true;
}

bool ATA::writeSectors28(AtaDrive* drive, u32 lba, u8 count, const void* buffer) {
    u16 ioBase = drive->ioBase;
    
    if (!waitReady(ioBase)) {
        return false;
    }
    
    u8 driveSelect = drive->isMaster ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE;
    driveSelect |= ATA_DRIVE_LBA;
    driveSelect |= (lba >> 24) & 0x0F;
    
    cpu::outb(ioBase + ATA_REG_DRIVE, driveSelect);
    delay400ns(ioBase);
    
    cpu::outb(ioBase + ATA_REG_SECCOUNT, count == 0 ? 0 : count);
    cpu::outb(ioBase + ATA_REG_LBA_LO, lba & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    
    cpu::outb(ioBase + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    const u16* buf = static_cast<const u16*>(buffer);
    u32 sectorsToWrite = count == 0 ? 256 : count;
    
    for (u32 s = 0; s < sectorsToWrite; s++) {
        if (!waitDrq(ioBase)) {
            return false;
        }
        
        for (int i = 0; i < 256; i++) {
            cpu::outw(ioBase + ATA_REG_DATA, buf[s * 256 + i]);
        }
    }
    
    cpu::outb(ioBase + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    waitReady(ioBase);
    
    return true;
}

bool ATA::writeSectors48(AtaDrive* drive, u64 lba, u16 count, const void* buffer) {
    u16 ioBase = drive->ioBase;
    
    if (!waitReady(ioBase)) {
        return false;
    }
    
    u8 driveSelect = drive->isMaster ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE;
    driveSelect |= ATA_DRIVE_LBA;
    
    cpu::outb(ioBase + ATA_REG_DRIVE, driveSelect);
    delay400ns(ioBase);
    
    cpu::outb(ioBase + ATA_REG_SECCOUNT, (count >> 8) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_LO, (lba >> 24) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_MID, (lba >> 32) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_HI, (lba >> 40) & 0xFF);
    
    cpu::outb(ioBase + ATA_REG_SECCOUNT, count & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_LO, lba & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    cpu::outb(ioBase + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    
    cpu::outb(ioBase + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);
    
    const u16* buf = static_cast<const u16*>(buffer);
    u32 sectorsToWrite = count == 0 ? 65536 : count;
    
    for (u32 s = 0; s < sectorsToWrite; s++) {
        if (!waitDrq(ioBase)) {
            return false;
        }
        
        for (int i = 0; i < 256; i++) {
            cpu::outw(ioBase + ATA_REG_DATA, buf[s * 256 + i]);
        }
    }
    
    cpu::outb(ioBase + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH_EXT);
    waitReady(ioBase);
    
    return true;
}

AtaDrive* ATA::getDrive(u8 index) {
    if (index >= sDriveCount) {
        return nullptr;
    }
    return &sDrives[index];
}

u8 ATA::driveCount() {
    return sDriveCount;
}

u64 ATA::sectorCount(u8 drive) {
    if (drive >= sDriveCount) {
        return 0;
    }
    return sDrives[drive].sectorCount;
}

u64 ATA::capacity(u8 drive) {
    return sectorCount(drive) * SECTOR_SIZE;
}

}
