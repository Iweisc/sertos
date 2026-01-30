#pragma once

#include "../types.hpp"

namespace sertos::cpu {

struct GdtEntry {
    u16 limitLow;
    u16 baseLow;
    u8 baseMiddle;
    u8 access;
    u8 granularity;
    u8 baseHigh;
} __attribute__((packed));

struct GdtEntry64 {
    u16 limitLow;
    u16 baseLow;
    u8 baseMiddle;
    u8 access;
    u8 granularity;
    u8 baseHigh;
    u32 baseUpper;
    u32 reserved;
} __attribute__((packed));

struct GdtPointer {
    u16 limit;
    u64 base;
} __attribute__((packed));

struct Tss {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iopbOffset;
} __attribute__((packed));

constexpr u8 GDT_ACCESS_PRESENT = 0x80;
constexpr u8 GDT_ACCESS_RING0 = 0x00;
constexpr u8 GDT_ACCESS_RING3 = 0x60;
constexpr u8 GDT_ACCESS_SYSTEM = 0x00;
constexpr u8 GDT_ACCESS_CODE_DATA = 0x10;
constexpr u8 GDT_ACCESS_EXECUTABLE = 0x08;
constexpr u8 GDT_ACCESS_DC = 0x04;
constexpr u8 GDT_ACCESS_RW = 0x02;
constexpr u8 GDT_ACCESS_ACCESSED = 0x01;

constexpr u8 GDT_FLAG_GRANULARITY = 0x80;
constexpr u8 GDT_FLAG_SIZE = 0x40;
constexpr u8 GDT_FLAG_LONG = 0x20;

constexpr u16 KERNEL_CODE_SELECTOR = 0x08;
constexpr u16 KERNEL_DATA_SELECTOR = 0x10;
constexpr u16 USER_CODE_SELECTOR = 0x18 | 3;
constexpr u16 USER_DATA_SELECTOR = 0x20 | 3;
constexpr u16 TSS_SELECTOR = 0x28;

class GDT {
public:
    static void initialize();
    static void setKernelStack(u64 stack);

private:
    static void setEntry(u32 index, u32 base, u32 limit, u8 access, u8 granularity);
    static void setTssEntry(u32 index, u64 base, u32 limit);
    static void loadGdt();
    static void loadTss();
    
    static GdtEntry sEntries[7];
    static GdtEntry64 sTssEntry;
    static GdtPointer sPointer;
    static Tss sTss;
};

}
