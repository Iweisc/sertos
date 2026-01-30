#include "../../include/cpu/gdt.hpp"

namespace sertos::cpu {

GdtEntry GDT::sEntries[7];
GdtEntry64 GDT::sTssEntry;
GdtPointer GDT::sPointer;
Tss GDT::sTss;

void GDT::initialize() {
    for (u64 i = 0; i < sizeof(sTss); i++) {
        reinterpret_cast<u8*>(&sTss)[i] = 0;
    }
    sTss.iopbOffset = sizeof(Tss);
    
    setEntry(0, 0, 0, 0, 0);
    
    setEntry(1, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
        GDT_FLAG_GRANULARITY | GDT_FLAG_LONG);
    
    setEntry(2, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_CODE_DATA | GDT_ACCESS_RW,
        GDT_FLAG_GRANULARITY | GDT_FLAG_LONG);
    
    setEntry(3, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
        GDT_FLAG_GRANULARITY | GDT_FLAG_LONG);
    
    setEntry(4, 0, 0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODE_DATA | GDT_ACCESS_RW,
        GDT_FLAG_GRANULARITY | GDT_FLAG_LONG);
    
    setTssEntry(5, reinterpret_cast<u64>(&sTss), sizeof(Tss) - 1);
    
    sPointer.limit = sizeof(sEntries) + sizeof(sTssEntry) - 1;
    sPointer.base = reinterpret_cast<u64>(&sEntries);
    
    loadGdt();
    loadTss();
}

void GDT::setKernelStack(u64 stack) {
    sTss.rsp0 = stack;
}

void GDT::setEntry(u32 index, u32 base, u32 limit, u8 access, u8 granularity) {
    sEntries[index].baseLow = base & 0xFFFF;
    sEntries[index].baseMiddle = (base >> 16) & 0xFF;
    sEntries[index].baseHigh = (base >> 24) & 0xFF;
    
    sEntries[index].limitLow = limit & 0xFFFF;
    sEntries[index].granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    
    sEntries[index].access = access;
}

void GDT::setTssEntry(u32 index, u64 base, u32 limit) {
    auto* entry = reinterpret_cast<GdtEntry64*>(&sEntries[index]);
    
    entry->limitLow = limit & 0xFFFF;
    entry->baseLow = base & 0xFFFF;
    entry->baseMiddle = (base >> 16) & 0xFF;
    entry->access = 0x89;
    entry->granularity = ((limit >> 16) & 0x0F);
    entry->baseHigh = (base >> 24) & 0xFF;
    entry->baseUpper = (base >> 32) & 0xFFFFFFFF;
    entry->reserved = 0;
}

void GDT::loadGdt() {
    asm volatile(
        "lgdt (%0)\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        : : "r"(&sPointer) : "rax", "memory"
    );
}

void GDT::loadTss() {
    asm volatile("ltr %0" : : "r"(static_cast<u16>(TSS_SELECTOR)));
}

}
