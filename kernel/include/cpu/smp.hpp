#pragma once

#include "../types.hpp"

namespace sertos::cpu {

constexpr u32 MAX_CPUS = 256;
constexpr u32 MAX_IOAPICS = 8;

constexpr u32 LAPIC_ID = 0x020;
constexpr u32 LAPIC_VERSION = 0x030;
constexpr u32 LAPIC_TPR = 0x080;
constexpr u32 LAPIC_APR = 0x090;
constexpr u32 LAPIC_PPR = 0x0A0;
constexpr u32 LAPIC_EOI = 0x0B0;
constexpr u32 LAPIC_RRD = 0x0C0;
constexpr u32 LAPIC_LDR = 0x0D0;
constexpr u32 LAPIC_DFR = 0x0E0;
constexpr u32 LAPIC_SVR = 0x0F0;
constexpr u32 LAPIC_ISR = 0x100;
constexpr u32 LAPIC_TMR = 0x180;
constexpr u32 LAPIC_IRR = 0x200;
constexpr u32 LAPIC_ESR = 0x280;
constexpr u32 LAPIC_ICR_LOW = 0x300;
constexpr u32 LAPIC_ICR_HIGH = 0x310;
constexpr u32 LAPIC_LVT_TIMER = 0x320;
constexpr u32 LAPIC_LVT_THERMAL = 0x330;
constexpr u32 LAPIC_LVT_PERF = 0x340;
constexpr u32 LAPIC_LVT_LINT0 = 0x350;
constexpr u32 LAPIC_LVT_LINT1 = 0x360;
constexpr u32 LAPIC_LVT_ERROR = 0x370;
constexpr u32 LAPIC_TIMER_ICR = 0x380;
constexpr u32 LAPIC_TIMER_CCR = 0x390;
constexpr u32 LAPIC_TIMER_DCR = 0x3E0;

constexpr u32 LAPIC_SVR_ENABLE = 0x100;
constexpr u32 LAPIC_ICR_INIT = 0x500;
constexpr u32 LAPIC_ICR_STARTUP = 0x600;
constexpr u32 LAPIC_ICR_LEVEL = 0x8000;
constexpr u32 LAPIC_ICR_ASSERT = 0x4000;
constexpr u32 LAPIC_ICR_DEASSERT = 0x0000;
constexpr u32 LAPIC_ICR_PENDING = 0x1000;

constexpr u32 LAPIC_TIMER_PERIODIC = 0x20000;
constexpr u32 LAPIC_TIMER_ONESHOT = 0x00000;
constexpr u32 LAPIC_TIMER_TSC_DEADLINE = 0x40000;

enum class CpuState : u8 {
    Offline = 0,
    Starting,
    Running,
    Halted,
    Idle
};

struct CpuInfo {
    u32 apicId;
    u32 cpuId;
    CpuState state;
    bool isBsp;
    u64 kernelStack;
    u64 idleStack;
    void* currentProcess;
    void* currentThread;
    u64 tickCount;
    u64 idleTime;
    bool needsReschedule;
};

struct IoApic {
    u32 id;
    u64 address;
    u32 gsiBase;
    u32 maxRedirections;
    bool active;
};

struct MadtHeader {
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oemId[6];
    char oemTableId[8];
    u32 oemRevision;
    u32 creatorId;
    u32 creatorRevision;
    u32 lapicAddress;
    u32 flags;
} __attribute__((packed));

struct MadtEntry {
    u8 type;
    u8 length;
} __attribute__((packed));

struct MadtLapic {
    MadtEntry header;
    u8 processorId;
    u8 apicId;
    u32 flags;
} __attribute__((packed));

struct MadtIoApic {
    MadtEntry header;
    u8 ioApicId;
    u8 reserved;
    u32 address;
    u32 gsiBase;
} __attribute__((packed));

struct MadtIso {
    MadtEntry header;
    u8 busSource;
    u8 irqSource;
    u32 gsi;
    u16 flags;
} __attribute__((packed));

struct MadtNmi {
    MadtEntry header;
    u8 processorId;
    u16 flags;
    u8 lint;
} __attribute__((packed));

struct MadtLapicOverride {
    MadtEntry header;
    u16 reserved;
    u64 address;
} __attribute__((packed));

using IpiHandler = void (*)(void* data);

class SMP {
public:
    static void initialize();
    
    static bool startCpu(u32 cpuId);
    static bool startAllCpus();
    static void haltCpu(u32 cpuId);
    static void haltAllCpus();
    
    static u32 currentCpuId();
    static CpuInfo* currentCpu();
    static CpuInfo* getCpu(u32 cpuId);
    static u32 cpuCount();
    static u32 onlineCpuCount();
    static bool isBsp();
    
    static void sendIpi(u32 targetCpuId, u8 vector);
    static void sendIpiAll(u8 vector);
    static void sendIpiAllButSelf(u8 vector);
    static void broadcastInit();
    static void broadcastStartup(u8 vector);
    
    static void lapicWrite(u32 reg, u32 value);
    static u32 lapicRead(u32 reg);
    static void lapicEoi();
    static void lapicEnable();
    static void lapicDisable();
    
    static void ioApicWrite(u32 ioApicId, u32 reg, u32 value);
    static u32 ioApicRead(u32 ioApicId, u32 reg);
    static void ioApicSetIrq(u32 irq, u32 vector, u32 targetCpu, bool masked);
    
    static void setupTimer(u32 frequency);
    static void timerHandler();
    
    static void acquireSpinlock(volatile u32* lock);
    static void releaseSpinlock(volatile u32* lock);
    static bool trySpinlock(volatile u32* lock);
    
    static bool isInitialized();

private:
    static void parseMadt();
    static void initLapic();
    static void initIoApic();
    static void calibrateTimer();
    
    static void apStartup();
    
    static CpuInfo sCpus[MAX_CPUS];
    static IoApic sIoApics[MAX_IOAPICS];
    static u64 sLapicBase;
    static u32 sCpuCount;
    static u32 sOnlineCpuCount;
    static u32 sIoApicCount;
    static u32 sBspId;
    static u32 sTimerFrequency;
    static u32 sTimerDivisor;
    static volatile u32 sApStarted;
    static bool sInitialized;
};

extern "C" void ap_trampoline();
extern "C" void ap_trampoline_end();

}
