#pragma once

#include "../types.hpp"

namespace sertos::disk {

constexpr u16 ATA_PRIMARY_IO = 0x1F0;
constexpr u16 ATA_PRIMARY_CTRL = 0x3F6;
constexpr u16 ATA_SECONDARY_IO = 0x170;
constexpr u16 ATA_SECONDARY_CTRL = 0x376;

constexpr u16 ATA_REG_DATA = 0;
constexpr u16 ATA_REG_ERROR = 1;
constexpr u16 ATA_REG_FEATURES = 1;
constexpr u16 ATA_REG_SECCOUNT = 2;
constexpr u16 ATA_REG_LBA_LO = 3;
constexpr u16 ATA_REG_LBA_MID = 4;
constexpr u16 ATA_REG_LBA_HI = 5;
constexpr u16 ATA_REG_DRIVE = 6;
constexpr u16 ATA_REG_STATUS = 7;
constexpr u16 ATA_REG_COMMAND = 7;

constexpr u8 ATA_CMD_READ_PIO = 0x20;
constexpr u8 ATA_CMD_READ_PIO_EXT = 0x24;
constexpr u8 ATA_CMD_WRITE_PIO = 0x30;
constexpr u8 ATA_CMD_WRITE_PIO_EXT = 0x34;
constexpr u8 ATA_CMD_CACHE_FLUSH = 0xE7;
constexpr u8 ATA_CMD_CACHE_FLUSH_EXT = 0xEA;
constexpr u8 ATA_CMD_IDENTIFY = 0xEC;

constexpr u8 ATA_STATUS_ERR = 0x01;
constexpr u8 ATA_STATUS_DRQ = 0x08;
constexpr u8 ATA_STATUS_SRV = 0x10;
constexpr u8 ATA_STATUS_DF = 0x20;
constexpr u8 ATA_STATUS_RDY = 0x40;
constexpr u8 ATA_STATUS_BSY = 0x80;

constexpr u8 ATA_DRIVE_MASTER = 0xA0;
constexpr u8 ATA_DRIVE_SLAVE = 0xB0;
constexpr u8 ATA_DRIVE_LBA = 0x40;

constexpr usize SECTOR_SIZE = 512;

enum class AtaDriveType : u8 {
    None = 0,
    ATA = 1,
    ATAPI = 2
};

struct AtaDrive {
    bool present;
    AtaDriveType type;
    u16 ioBase;
    u16 ctrlBase;
    bool isMaster;
    bool supportsLba48;
    u64 sectorCount;
    char model[41];
    char serial[21];
};

class ATA {
public:
    static void initialize();
    static bool isInitialized();
    
    static bool read(u8 drive, u64 lba, u32 sectorCount, void* buffer);
    static bool write(u8 drive, u64 lba, u32 sectorCount, const void* buffer);
    
    static AtaDrive* getDrive(u8 index);
    static u8 driveCount();
    
    static u64 sectorCount(u8 drive);
    static u64 capacity(u8 drive);

private:
    static void detectDrive(u16 ioBase, u16 ctrlBase, bool isMaster);
    static bool identify(u16 ioBase, bool isMaster, AtaDrive* drive);
    static void selectDrive(u16 ioBase, bool isMaster, bool lba = true);
    static bool waitReady(u16 ioBase);
    static bool waitDrq(u16 ioBase);
    static void softReset(u16 ctrlBase);
    static void delay400ns(u16 ioBase);
    
    static bool readSectors28(AtaDrive* drive, u32 lba, u8 count, void* buffer);
    static bool readSectors48(AtaDrive* drive, u64 lba, u16 count, void* buffer);
    static bool writeSectors28(AtaDrive* drive, u32 lba, u8 count, const void* buffer);
    static bool writeSectors48(AtaDrive* drive, u64 lba, u16 count, const void* buffer);
    
    static constexpr u8 MAX_DRIVES = 4;
    static AtaDrive sDrives[MAX_DRIVES];
    static u8 sDriveCount;
    static bool sInitialized;
};

}
