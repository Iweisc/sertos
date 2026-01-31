#include "../../include/power/acpi.hpp"
#include "../../include/cpu/io.hpp"

namespace sertos::power {

namespace {

void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

bool memcmp(const void* s1, const void* s2, usize n) {
    const u8* p1 = reinterpret_cast<const u8*>(s1);
    const u8* p2 = reinterpret_cast<const u8*>(s2);
    for (usize i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return false;
    }
    return true;
}

void strcpy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void ioWait() {
    cpu::outb(0x80, 0);
}

}

RsdpDescriptor* ACPI::sRsdp = nullptr;
RsdpDescriptor20* ACPI::sRsdp20 = nullptr;
Rsdt* ACPI::sRsdt = nullptr;
Xsdt* ACPI::sXsdt = nullptr;
Fadt* ACPI::sFadt = nullptr;
Madt* ACPI::sMadt = nullptr;
AcpiTable ACPI::sTables[MAX_ACPI_TABLES];
u32 ACPI::sTableCount = 0;
PowerState ACPI::sCurrentState = PowerState::S0;
bool ACPI::sAcpiEnabled = false;
bool ACPI::sInitialized = false;
bool ACPI::sUseXsdt = false;

void ACPI::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_ACPI_TABLES; i++) {
        memset(&sTables[i], 0, sizeof(AcpiTable));
        sTables[i].valid = false;
    }
    
    if (!findRsdp()) {
        return;
    }
    
    if (sRsdp->revision >= 2 && sRsdp20) {
        sUseXsdt = true;
        if (!parseXsdt()) {
            sUseXsdt = false;
            if (!parseRsdt()) {
                return;
            }
        }
    } else {
        if (!parseRsdt()) {
            return;
        }
    }
    
    sFadt = reinterpret_cast<Fadt*>(findTable("FACP"));
    sMadt = reinterpret_cast<Madt*>(findTable("APIC"));
    
    sCurrentState = PowerState::S0;
    sInitialized = true;
}

bool ACPI::enable() {
    if (!sInitialized || !sFadt) return false;
    if (sAcpiEnabled) return true;
    
    if (sFadt->smiCommandPort == 0) {
        sAcpiEnabled = true;
        return true;
    }
    
    cpu::outb(static_cast<u16>(sFadt->smiCommandPort), sFadt->acpiEnable);
    
    for (int i = 0; i < 3000; i++) {
        u16 pm1aControl = cpu::inw(static_cast<u16>(sFadt->pm1aControlBlock));
        if (pm1aControl & 1) {
            sAcpiEnabled = true;
            return true;
        }
        ioWait();
    }
    
    return false;
}

bool ACPI::disable() {
    if (!sInitialized || !sFadt) return false;
    if (!sAcpiEnabled) return true;
    
    if (sFadt->smiCommandPort == 0) {
        sAcpiEnabled = false;
        return true;
    }
    
    cpu::outb(static_cast<u16>(sFadt->smiCommandPort), sFadt->acpiDisable);
    
    for (int i = 0; i < 3000; i++) {
        u16 pm1aControl = cpu::inw(static_cast<u16>(sFadt->pm1aControlBlock));
        if (!(pm1aControl & 1)) {
            sAcpiEnabled = false;
            return true;
        }
        ioWait();
    }
    
    return false;
}

bool ACPI::isEnabled() {
    return sAcpiEnabled;
}

bool ACPI::shutdown() {
    if (!sInitialized) return false;
    
    if (!sAcpiEnabled) {
        enable();
    }
    
    return enterSleepState(PowerState::S5);
}

bool ACPI::reboot() {
    if (!sInitialized) return false;
    
    if (sFadt && sFadt->header.length >= 129) {
        GenericAddress* resetReg = reinterpret_cast<GenericAddress*>(sFadt->resetReg);
        
        if (resetReg->addressSpace == 1) {
            cpu::outb(static_cast<u16>(resetReg->address), sFadt->resetValue);
            for (volatile int i = 0; i < 1000000; i++);
        } else if (resetReg->addressSpace == 0) {
            volatile u8* addr = reinterpret_cast<volatile u8*>(resetReg->address);
            *addr = sFadt->resetValue;
            for (volatile int i = 0; i < 1000000; i++);
        }
    }
    
    cpu::outb(0x64, 0xFE);
    for (volatile int i = 0; i < 1000000; i++);
    
    asm volatile("cli; hlt");
    
    return false;
}

bool ACPI::suspend(PowerState state) {
    if (!sInitialized) return false;
    
    if (state == PowerState::S0 || state == PowerState::S5) {
        return false;
    }
    
    return enterSleepState(state);
}

bool ACPI::resume() {
    if (!sInitialized) return false;
    
    wakeFromSleep();
    sCurrentState = PowerState::S0;
    
    return true;
}

PowerState ACPI::currentPowerState() {
    return sCurrentState;
}

bool ACPI::setPowerState(PowerState state) {
    if (!sInitialized) return false;
    
    if (state == PowerState::S5) {
        return shutdown();
    }
    
    if (state == PowerState::S0) {
        return resume();
    }
    
    return suspend(state);
}

bool ACPI::setDevicePowerState(u32, DevicePowerState) {
    if (!sInitialized) return false;
    return true;
}

DevicePowerState ACPI::getDevicePowerState(u32) {
    return DevicePowerState::D0;
}

AcpiSdtHeader* ACPI::findTable(const char* signature) {
    if (!sInitialized) return nullptr;
    
    for (u32 i = 0; i < sTableCount; i++) {
        if (sTables[i].valid && memcmp(sTables[i].signature, signature, 4)) {
            return reinterpret_cast<AcpiSdtHeader*>(sTables[i].address);
        }
    }
    
    return nullptr;
}

Fadt* ACPI::getFadt() {
    return sFadt;
}

Madt* ACPI::getMadt() {
    return sMadt;
}

u32 ACPI::getPmTimer() {
    if (!sInitialized || !sFadt) return 0;
    
    return cpu::inl(static_cast<u16>(sFadt->pmTimerBlock));
}

void ACPI::pmTimerSleep(u32 microseconds) {
    if (!sInitialized || !sFadt || sFadt->pmTimerBlock == 0) return;
    
    u32 timerFreq = 3579545;
    u32 ticksNeeded = (microseconds * timerFreq) / 1000000;
    
    u32 startTicks = getPmTimer();
    u32 currentTicks;
    
    do {
        currentTicks = getPmTimer();
        u32 elapsed;
        
        if (currentTicks >= startTicks) {
            elapsed = currentTicks - startTicks;
        } else {
            elapsed = (0xFFFFFF - startTicks) + currentTicks + 1;
        }
        
        if (elapsed >= ticksNeeded) break;
        
    } while (true);
}

bool ACPI::isInitialized() {
    return sInitialized;
}

bool ACPI::findRsdp() {
    const char* rsdpSignature = "RSD PTR ";
    
    for (u64 addr = 0x000E0000; addr < 0x00100000; addr += 16) {
        if (memcmp(reinterpret_cast<void*>(addr), rsdpSignature, 8)) {
            sRsdp = reinterpret_cast<RsdpDescriptor*>(addr);
            
            if (!validateChecksum(sRsdp, sizeof(RsdpDescriptor))) {
                sRsdp = nullptr;
                continue;
            }
            
            if (sRsdp->revision >= 2) {
                sRsdp20 = reinterpret_cast<RsdpDescriptor20*>(addr);
                if (!validateChecksum(sRsdp20, sizeof(RsdpDescriptor20))) {
                    sRsdp20 = nullptr;
                }
            }
            
            return true;
        }
    }
    
    u64 ebdaBase = *reinterpret_cast<u16*>(0x40E) << 4;
    for (u64 addr = ebdaBase; addr < ebdaBase + 1024; addr += 16) {
        if (memcmp(reinterpret_cast<void*>(addr), rsdpSignature, 8)) {
            sRsdp = reinterpret_cast<RsdpDescriptor*>(addr);
            
            if (!validateChecksum(sRsdp, sizeof(RsdpDescriptor))) {
                sRsdp = nullptr;
                continue;
            }
            
            if (sRsdp->revision >= 2) {
                sRsdp20 = reinterpret_cast<RsdpDescriptor20*>(addr);
                if (!validateChecksum(sRsdp20, sizeof(RsdpDescriptor20))) {
                    sRsdp20 = nullptr;
                }
            }
            
            return true;
        }
    }
    
    return false;
}

bool ACPI::parseRsdt() {
    if (!sRsdp) return false;
    
    sRsdt = reinterpret_cast<Rsdt*>(static_cast<u64>(sRsdp->rsdtAddress));
    
    if (!validateChecksum(sRsdt, sRsdt->header.length)) {
        sRsdt = nullptr;
        return false;
    }
    
    u32 entries = (sRsdt->header.length - sizeof(AcpiSdtHeader)) / sizeof(u32);
    
    for (u32 i = 0; i < entries && sTableCount < MAX_ACPI_TABLES; i++) {
        AcpiSdtHeader* header = reinterpret_cast<AcpiSdtHeader*>(
            static_cast<u64>(sRsdt->tablePointers[i])
        );
        
        if (validateChecksum(header, header->length)) {
            sTables[sTableCount].address = reinterpret_cast<u64>(header);
            sTables[sTableCount].length = header->length;
            for (int j = 0; j < 4; j++) {
                sTables[sTableCount].signature[j] = header->signature[j];
            }
            sTables[sTableCount].signature[4] = '\0';
            sTables[sTableCount].valid = true;
            sTableCount++;
        }
    }
    
    return true;
}

bool ACPI::parseXsdt() {
    if (!sRsdp20) return false;
    
    sXsdt = reinterpret_cast<Xsdt*>(sRsdp20->xsdtAddress);
    
    if (!validateChecksum(sXsdt, sXsdt->header.length)) {
        sXsdt = nullptr;
        return false;
    }
    
    u32 entries = (sXsdt->header.length - sizeof(AcpiSdtHeader)) / sizeof(u64);
    
    for (u32 i = 0; i < entries && sTableCount < MAX_ACPI_TABLES; i++) {
        AcpiSdtHeader* header = reinterpret_cast<AcpiSdtHeader*>(sXsdt->tablePointers[i]);
        
        if (validateChecksum(header, header->length)) {
            sTables[sTableCount].address = reinterpret_cast<u64>(header);
            sTables[sTableCount].length = header->length;
            for (int j = 0; j < 4; j++) {
                sTables[sTableCount].signature[j] = header->signature[j];
            }
            sTables[sTableCount].signature[4] = '\0';
            sTables[sTableCount].valid = true;
            sTableCount++;
        }
    }
    
    return true;
}

bool ACPI::validateChecksum(void* table, usize length) {
    u8* bytes = reinterpret_cast<u8*>(table);
    u8 sum = 0;
    
    for (usize i = 0; i < length; i++) {
        sum += bytes[i];
    }
    
    return sum == 0;
}

bool ACPI::enterSleepState(PowerState state) {
    if (!sFadt) return false;
    
    prepareForSleep(state);
    
    u16 slpTypA = 0;
    u16 slpTypB = 0;
    
    switch (state) {
        case PowerState::S1:
            slpTypA = 0;
            slpTypB = 0;
            break;
        case PowerState::S2:
            slpTypA = 1;
            slpTypB = 1;
            break;
        case PowerState::S3:
            slpTypA = 1;
            slpTypB = 1;
            break;
        case PowerState::S4:
            slpTypA = 2;
            slpTypB = 2;
            break;
        case PowerState::S5:
            slpTypA = 2;
            slpTypB = 2;
            break;
        default:
            return false;
    }
    
    u16 pm1aValue = (slpTypA << 10) | (1 << 13);
    u16 pm1bValue = (slpTypB << 10) | (1 << 13);
    
    asm volatile("cli");
    
    cpu::outw(static_cast<u16>(sFadt->pm1aControlBlock), pm1aValue);
    if (sFadt->pm1bControlBlock) {
        cpu::outw(static_cast<u16>(sFadt->pm1bControlBlock), pm1bValue);
    }
    
    for (volatile int i = 0; i < 1000000; i++);
    
    sCurrentState = state;
    
    return true;
}

void ACPI::prepareForSleep(PowerState) {
}

void ACPI::wakeFromSleep() {
}

void (*PowerManager::sIdleCallback)() = nullptr;
u32 PowerManager::sBatteryLevel = 100;
bool PowerManager::sOnBattery = false;
bool PowerManager::sCharging = false;
bool PowerManager::sPowerSaving = false;
bool PowerManager::sInitialized = false;

void PowerManager::initialize() {
    if (sInitialized) return;
    
    ACPI::initialize();
    
    sBatteryLevel = 100;
    sOnBattery = false;
    sCharging = false;
    sPowerSaving = false;
    sIdleCallback = nullptr;
    
    sInitialized = true;
}

bool PowerManager::shutdown() {
    if (!sInitialized) return false;
    
    if (ACPI::isInitialized()) {
        return ACPI::shutdown();
    }
    
    cpu::outb(0x64, 0xFE);
    
    asm volatile("cli; hlt");
    
    return false;
}

bool PowerManager::reboot() {
    if (!sInitialized) return false;
    
    if (ACPI::isInitialized()) {
        return ACPI::reboot();
    }
    
    cpu::outb(0x64, 0xFE);
    
    asm volatile("cli; hlt");
    
    return false;
}

bool PowerManager::sleep() {
    if (!sInitialized) return false;
    
    if (ACPI::isInitialized()) {
        return ACPI::suspend(PowerState::S3);
    }
    
    return false;
}

bool PowerManager::hibernate() {
    if (!sInitialized) return false;
    
    if (ACPI::isInitialized()) {
        return ACPI::suspend(PowerState::S4);
    }
    
    return false;
}

bool PowerManager::suspend() {
    if (!sInitialized) return false;
    
    if (ACPI::isInitialized()) {
        return ACPI::suspend(PowerState::S1);
    }
    
    return false;
}

void PowerManager::setIdleCallback(void (*callback)()) {
    sIdleCallback = callback;
}

void PowerManager::idle() {
    if (sIdleCallback) {
        sIdleCallback();
    } else {
        asm volatile("hlt");
    }
}

u32 PowerManager::getBatteryLevel() {
    return sBatteryLevel;
}

bool PowerManager::isOnBattery() {
    return sOnBattery;
}

bool PowerManager::isCharging() {
    return sCharging;
}

void PowerManager::setCpuFrequency(u32) {
}

u32 PowerManager::getCpuFrequency() {
    return 0;
}

u32 PowerManager::getMaxCpuFrequency() {
    return 0;
}

u32 PowerManager::getMinCpuFrequency() {
    return 0;
}

void PowerManager::enablePowerSaving() {
    sPowerSaving = true;
}

void PowerManager::disablePowerSaving() {
    sPowerSaving = false;
}

bool PowerManager::isPowerSavingEnabled() {
    return sPowerSaving;
}

bool PowerManager::isInitialized() {
    return sInitialized;
}

}
