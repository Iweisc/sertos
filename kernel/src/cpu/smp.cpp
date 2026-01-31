#include "../../include/cpu/smp.hpp"
#include "../../include/cpu/gdt.hpp"
#include "../../include/cpu/idt.hpp"
#include "../../include/memory/pmm.hpp"
#include "../../include/memory/vmm.hpp"

namespace sertos::cpu {

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

void ioWait() {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

void cpuPause() {
    asm volatile("pause");
}

u64 rdmsr(u32 msr) {
    u32 low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return (static_cast<u64>(high) << 32) | low;
}

void wrmsr(u32 msr, u64 value) {
    u32 low = static_cast<u32>(value);
    u32 high = static_cast<u32>(value >> 32);
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

}

CpuInfo SMP::sCpus[MAX_CPUS];
IoApic SMP::sIoApics[MAX_IOAPICS];
u64 SMP::sLapicBase = 0;
u32 SMP::sCpuCount = 0;
u32 SMP::sOnlineCpuCount = 0;
u32 SMP::sIoApicCount = 0;
u32 SMP::sBspId = 0;
u32 SMP::sTimerFrequency = 0;
u32 SMP::sTimerDivisor = 0;
volatile u32 SMP::sApStarted = 0;
bool SMP::sInitialized = false;

constexpr u32 IA32_APIC_BASE_MSR = 0x1B;
constexpr u64 IA32_APIC_BASE_ENABLE = 1ULL << 11;
constexpr u64 IA32_APIC_BASE_BSP = 1ULL << 8;

void SMP::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_CPUS; i++) {
        memset(&sCpus[i], 0, sizeof(CpuInfo));
        sCpus[i].state = CpuState::Offline;
        sCpus[i].cpuId = i;
    }
    
    for (u32 i = 0; i < MAX_IOAPICS; i++) {
        memset(&sIoApics[i], 0, sizeof(IoApic));
        sIoApics[i].active = false;
    }
    
    u64 apicBaseMsr = rdmsr(IA32_APIC_BASE_MSR);
    sLapicBase = apicBaseMsr & 0xFFFFF000;
    
    bool isBsp = (apicBaseMsr & IA32_APIC_BASE_BSP) != 0;
    
    parseMadt();
    
    initLapic();
    
    u32 bspApicId = lapicRead(LAPIC_ID) >> 24;
    sBspId = 0;
    
    for (u32 i = 0; i < sCpuCount; i++) {
        if (sCpus[i].apicId == bspApicId) {
            sBspId = i;
            sCpus[i].isBsp = true;
            sCpus[i].state = CpuState::Running;
            sOnlineCpuCount = 1;
            break;
        }
    }
    
    initIoApic();
    
    calibrateTimer();
    
    sInitialized = true;
}

bool SMP::startCpu(u32 cpuId) {
    if (!sInitialized) return false;
    if (cpuId >= sCpuCount) return false;
    if (sCpus[cpuId].state != CpuState::Offline) return false;
    if (sCpus[cpuId].isBsp) return true;
    
    CpuInfo* cpu = &sCpus[cpuId];
    
    void* stack = memory::PMM::allocatePages(4);
    if (!stack) return false;
    cpu->kernelStack = reinterpret_cast<u64>(stack) + 4 * memory::PAGE_SIZE - 8;
    
    cpu->state = CpuState::Starting;
    sApStarted = 0;
    
    u32 targetApicId = cpu->apicId;
    
    lapicWrite(LAPIC_ESR, 0);
    
    lapicWrite(LAPIC_ICR_HIGH, targetApicId << 24);
    lapicWrite(LAPIC_ICR_LOW, LAPIC_ICR_INIT | LAPIC_ICR_LEVEL | LAPIC_ICR_ASSERT);
    
    for (volatile int i = 0; i < 10000; i++) cpuPause();
    
    lapicWrite(LAPIC_ICR_HIGH, targetApicId << 24);
    lapicWrite(LAPIC_ICR_LOW, LAPIC_ICR_INIT | LAPIC_ICR_LEVEL | LAPIC_ICR_DEASSERT);
    
    for (volatile int i = 0; i < 100000; i++) cpuPause();
    
    for (int sipi = 0; sipi < 2; sipi++) {
        lapicWrite(LAPIC_ESR, 0);
        
        u8 vector = 0x08;
        lapicWrite(LAPIC_ICR_HIGH, targetApicId << 24);
        lapicWrite(LAPIC_ICR_LOW, LAPIC_ICR_STARTUP | vector);
        
        for (volatile int i = 0; i < 10000; i++) cpuPause();
        
        if (sApStarted) break;
    }
    
    for (volatile int i = 0; i < 1000000 && !sApStarted; i++) {
        cpuPause();
    }
    
    if (sApStarted) {
        cpu->state = CpuState::Running;
        sOnlineCpuCount++;
        return true;
    }
    
    cpu->state = CpuState::Offline;
    memory::PMM::freePages(stack, 4);
    return false;
}

bool SMP::startAllCpus() {
    if (!sInitialized) return false;
    
    bool allStarted = true;
    for (u32 i = 0; i < sCpuCount; i++) {
        if (!sCpus[i].isBsp && sCpus[i].state == CpuState::Offline) {
            if (!startCpu(i)) {
                allStarted = false;
            }
        }
    }
    
    return allStarted;
}

void SMP::haltCpu(u32 cpuId) {
    if (!sInitialized) return;
    if (cpuId >= sCpuCount) return;
    
    if (cpuId == currentCpuId()) {
        sCpus[cpuId].state = CpuState::Halted;
        asm volatile("cli; hlt");
    } else {
        sendIpi(cpuId, 0xFE);
    }
}

void SMP::haltAllCpus() {
    if (!sInitialized) return;
    
    sendIpiAllButSelf(0xFE);
    
    sCpus[currentCpuId()].state = CpuState::Halted;
    asm volatile("cli; hlt");
}

u32 SMP::currentCpuId() {
    if (!sInitialized) return 0;
    
    u32 apicId = lapicRead(LAPIC_ID) >> 24;
    
    for (u32 i = 0; i < sCpuCount; i++) {
        if (sCpus[i].apicId == apicId) {
            return i;
        }
    }
    
    return 0;
}

CpuInfo* SMP::currentCpu() {
    return &sCpus[currentCpuId()];
}

CpuInfo* SMP::getCpu(u32 cpuId) {
    if (cpuId >= sCpuCount) return nullptr;
    return &sCpus[cpuId];
}

u32 SMP::cpuCount() {
    return sCpuCount;
}

u32 SMP::onlineCpuCount() {
    return sOnlineCpuCount;
}

bool SMP::isBsp() {
    return currentCpuId() == sBspId;
}

void SMP::sendIpi(u32 targetCpuId, u8 vector) {
    if (!sInitialized) return;
    if (targetCpuId >= sCpuCount) return;
    
    u32 targetApicId = sCpus[targetCpuId].apicId;
    
    while (lapicRead(LAPIC_ICR_LOW) & LAPIC_ICR_PENDING) {
        cpuPause();
    }
    
    lapicWrite(LAPIC_ICR_HIGH, targetApicId << 24);
    lapicWrite(LAPIC_ICR_LOW, vector);
}

void SMP::sendIpiAll(u8 vector) {
    if (!sInitialized) return;
    
    while (lapicRead(LAPIC_ICR_LOW) & LAPIC_ICR_PENDING) {
        cpuPause();
    }
    
    lapicWrite(LAPIC_ICR_HIGH, 0);
    lapicWrite(LAPIC_ICR_LOW, vector | (2 << 18));
}

void SMP::sendIpiAllButSelf(u8 vector) {
    if (!sInitialized) return;
    
    while (lapicRead(LAPIC_ICR_LOW) & LAPIC_ICR_PENDING) {
        cpuPause();
    }
    
    lapicWrite(LAPIC_ICR_HIGH, 0);
    lapicWrite(LAPIC_ICR_LOW, vector | (3 << 18));
}

void SMP::broadcastInit() {
    if (!sInitialized) return;
    
    lapicWrite(LAPIC_ICR_HIGH, 0);
    lapicWrite(LAPIC_ICR_LOW, LAPIC_ICR_INIT | (3 << 18) | LAPIC_ICR_LEVEL | LAPIC_ICR_ASSERT);
    
    for (volatile int i = 0; i < 10000; i++) cpuPause();
    
    lapicWrite(LAPIC_ICR_HIGH, 0);
    lapicWrite(LAPIC_ICR_LOW, LAPIC_ICR_INIT | (3 << 18) | LAPIC_ICR_LEVEL | LAPIC_ICR_DEASSERT);
}

void SMP::broadcastStartup(u8 vector) {
    if (!sInitialized) return;
    
    lapicWrite(LAPIC_ICR_HIGH, 0);
    lapicWrite(LAPIC_ICR_LOW, LAPIC_ICR_STARTUP | vector | (3 << 18));
}

void SMP::lapicWrite(u32 reg, u32 value) {
    volatile u32* lapic = reinterpret_cast<volatile u32*>(sLapicBase + reg);
    *lapic = value;
}

u32 SMP::lapicRead(u32 reg) {
    volatile u32* lapic = reinterpret_cast<volatile u32*>(sLapicBase + reg);
    return *lapic;
}

void SMP::lapicEoi() {
    lapicWrite(LAPIC_EOI, 0);
}

void SMP::lapicEnable() {
    u64 apicBase = rdmsr(IA32_APIC_BASE_MSR);
    wrmsr(IA32_APIC_BASE_MSR, apicBase | IA32_APIC_BASE_ENABLE);
    
    lapicWrite(LAPIC_SVR, lapicRead(LAPIC_SVR) | LAPIC_SVR_ENABLE | 0xFF);
}

void SMP::lapicDisable() {
    lapicWrite(LAPIC_SVR, lapicRead(LAPIC_SVR) & ~LAPIC_SVR_ENABLE);
}

void SMP::ioApicWrite(u32 ioApicId, u32 reg, u32 value) {
    if (ioApicId >= sIoApicCount) return;
    
    volatile u32* ioApic = reinterpret_cast<volatile u32*>(sIoApics[ioApicId].address);
    ioApic[0] = reg;
    ioApic[4] = value;
}

u32 SMP::ioApicRead(u32 ioApicId, u32 reg) {
    if (ioApicId >= sIoApicCount) return 0;
    
    volatile u32* ioApic = reinterpret_cast<volatile u32*>(sIoApics[ioApicId].address);
    ioApic[0] = reg;
    return ioApic[4];
}

void SMP::ioApicSetIrq(u32 irq, u32 vector, u32 targetCpu, bool masked) {
    if (sIoApicCount == 0) return;
    
    u32 ioApicId = 0;
    u32 localIrq = irq;
    
    for (u32 i = 0; i < sIoApicCount; i++) {
        if (irq >= sIoApics[i].gsiBase && 
            irq < sIoApics[i].gsiBase + sIoApics[i].maxRedirections) {
            ioApicId = i;
            localIrq = irq - sIoApics[i].gsiBase;
            break;
        }
    }
    
    u32 targetApicId = sCpus[targetCpu].apicId;
    
    u64 entry = vector;
    if (masked) {
        entry |= (1 << 16);
    }
    entry |= (static_cast<u64>(targetApicId) << 56);
    
    u32 regLow = 0x10 + localIrq * 2;
    u32 regHigh = 0x10 + localIrq * 2 + 1;
    
    ioApicWrite(ioApicId, regLow, static_cast<u32>(entry));
    ioApicWrite(ioApicId, regHigh, static_cast<u32>(entry >> 32));
}

void SMP::setupTimer(u32 frequency) {
    if (!sInitialized) return;
    
    sTimerFrequency = frequency;
    
    lapicWrite(LAPIC_TIMER_DCR, 0x03);
    
    u32 ticksPerMs = sTimerDivisor / 1000;
    u32 initialCount = ticksPerMs * (1000 / frequency);
    
    lapicWrite(LAPIC_LVT_TIMER, 0x20 | LAPIC_TIMER_PERIODIC);
    lapicWrite(LAPIC_TIMER_ICR, initialCount);
}

void SMP::timerHandler() {
    if (!sInitialized) return;
    
    CpuInfo* cpu = currentCpu();
    cpu->tickCount++;
    
    lapicEoi();
}

void SMP::acquireSpinlock(volatile u32* lock) {
    while (true) {
        if (__sync_lock_test_and_set(lock, 1) == 0) {
            return;
        }
        
        while (*lock) {
            cpuPause();
        }
    }
}

void SMP::releaseSpinlock(volatile u32* lock) {
    __sync_lock_release(lock);
}

bool SMP::trySpinlock(volatile u32* lock) {
    return __sync_lock_test_and_set(lock, 1) == 0;
}

bool SMP::isInitialized() {
    return sInitialized;
}

void SMP::parseMadt() {
    sCpuCount = 1;
    sCpus[0].apicId = 0;
    sCpus[0].cpuId = 0;
    sCpus[0].state = CpuState::Offline;
}

void SMP::initLapic() {
    u64 apicBase = rdmsr(IA32_APIC_BASE_MSR);
    wrmsr(IA32_APIC_BASE_MSR, apicBase | IA32_APIC_BASE_ENABLE);
    
    lapicWrite(LAPIC_DFR, 0xFFFFFFFF);
    lapicWrite(LAPIC_LDR, (lapicRead(LAPIC_LDR) & 0x00FFFFFF) | 1);
    
    lapicWrite(LAPIC_LVT_TIMER, 0x10000);
    lapicWrite(LAPIC_LVT_THERMAL, 0x10000);
    lapicWrite(LAPIC_LVT_PERF, 0x10000);
    lapicWrite(LAPIC_LVT_LINT0, 0x10000);
    lapicWrite(LAPIC_LVT_LINT1, 0x10000);
    lapicWrite(LAPIC_LVT_ERROR, 0x10000);
    
    lapicWrite(LAPIC_TPR, 0);
    
    lapicWrite(LAPIC_SVR, LAPIC_SVR_ENABLE | 0xFF);
}

void SMP::initIoApic() {
    if (sIoApicCount == 0) return;
    
    for (u32 i = 0; i < sIoApicCount; i++) {
        u32 version = ioApicRead(i, 1);
        sIoApics[i].maxRedirections = ((version >> 16) & 0xFF) + 1;
        
        for (u32 j = 0; j < sIoApics[i].maxRedirections; j++) {
            u32 regLow = 0x10 + j * 2;
            u32 regHigh = 0x10 + j * 2 + 1;
            
            ioApicWrite(i, regLow, 0x10000);
            ioApicWrite(i, regHigh, 0);
        }
    }
}

void SMP::calibrateTimer() {
    lapicWrite(LAPIC_TIMER_DCR, 0x03);
    
    lapicWrite(LAPIC_TIMER_ICR, 0xFFFFFFFF);
    
    for (volatile int i = 0; i < 1000000; i++) {
        cpuPause();
    }
    
    u32 ticksElapsed = 0xFFFFFFFF - lapicRead(LAPIC_TIMER_CCR);
    
    sTimerDivisor = ticksElapsed * 100;
    
    lapicWrite(LAPIC_LVT_TIMER, 0x10000);
}

void SMP::apStartup() {
    initLapic();
    
    u32 cpuId = currentCpuId();
    sCpus[cpuId].state = CpuState::Running;
    
    sApStarted = 1;
    
    while (true) {
        asm volatile("hlt");
    }
}

}
