#pragma once

#include "../types.hpp"

namespace sertos::cpu {

struct IdtEntry {
    u16 offsetLow;
    u16 selector;
    u8 ist;
    u8 typeAttr;
    u16 offsetMiddle;
    u32 offsetHigh;
    u32 reserved;
} __attribute__((packed));

struct IdtPointer {
    u16 limit;
    u64 base;
} __attribute__((packed));

struct InterruptFrame {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 interruptNumber, errorCode;
    u64 rip, cs, rflags, rsp, ss;
} __attribute__((packed));

constexpr u8 IDT_GATE_INTERRUPT = 0x8E;
constexpr u8 IDT_GATE_TRAP = 0x8F;
constexpr u8 IDT_GATE_USER = 0x60;

using InterruptHandler = void (*)(InterruptFrame* frame);

class IDT {
public:
    static void initialize();
    static void setHandler(u8 vector, InterruptHandler handler);
    static void enableInterrupts();
    static void disableInterrupts();
    static bool interruptsEnabled();

private:
    static void setEntry(u8 vector, u64 handler, u8 typeAttr, u8 ist = 0);
    static void loadIdt();
    
    static IdtEntry sEntries[256];
    static IdtPointer sPointer;
public:
    static InterruptHandler sHandlers[256];
};

extern "C" void isr_common_stub(InterruptFrame* frame);

constexpr u8 IRQ_BASE = 32;
constexpr u8 IRQ_TIMER = IRQ_BASE + 0;
constexpr u8 IRQ_KEYBOARD = IRQ_BASE + 1;
constexpr u8 IRQ_CASCADE = IRQ_BASE + 2;
constexpr u8 IRQ_COM2 = IRQ_BASE + 3;
constexpr u8 IRQ_COM1 = IRQ_BASE + 4;
constexpr u8 IRQ_LPT2 = IRQ_BASE + 5;
constexpr u8 IRQ_FLOPPY = IRQ_BASE + 6;
constexpr u8 IRQ_LPT1 = IRQ_BASE + 7;
constexpr u8 IRQ_RTC = IRQ_BASE + 8;
constexpr u8 IRQ_MOUSE = IRQ_BASE + 12;
constexpr u8 IRQ_FPU = IRQ_BASE + 13;
constexpr u8 IRQ_ATA_PRIMARY = IRQ_BASE + 14;
constexpr u8 IRQ_ATA_SECONDARY = IRQ_BASE + 15;

}
