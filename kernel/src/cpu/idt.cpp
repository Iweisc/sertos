#include "../../include/cpu/idt.hpp"
#include "../../include/cpu/gdt.hpp"

namespace sertos::cpu {

IdtEntry IDT::sEntries[256];
IdtPointer IDT::sPointer;
InterruptHandler IDT::sHandlers[256];

extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr3();
extern "C" void isr4();
extern "C" void isr5();
extern "C" void isr6();
extern "C" void isr7();
extern "C" void isr8();
extern "C" void isr9();
extern "C" void isr10();
extern "C" void isr11();
extern "C" void isr12();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void isr15();
extern "C" void isr16();
extern "C" void isr17();
extern "C" void isr18();
extern "C" void isr19();
extern "C" void isr20();
extern "C" void isr21();
extern "C" void isr22();
extern "C" void isr23();
extern "C" void isr24();
extern "C" void isr25();
extern "C" void isr26();
extern "C" void isr27();
extern "C" void isr28();
extern "C" void isr29();
extern "C" void isr30();
extern "C" void isr31();

extern "C" void irq0();
extern "C" void irq1();
extern "C" void irq2();
extern "C" void irq3();
extern "C" void irq4();
extern "C" void irq5();
extern "C" void irq6();
extern "C" void irq7();
extern "C" void irq8();
extern "C" void irq9();
extern "C" void irq10();
extern "C" void irq11();
extern "C" void irq12();
extern "C" void irq13();
extern "C" void irq14();
extern "C" void irq15();

void IDT::initialize() {
    for (int i = 0; i < 256; i++) {
        sEntries[i] = {0, 0, 0, 0, 0, 0, 0};
        sHandlers[i] = nullptr;
    }
    
    setEntry(0, reinterpret_cast<u64>(isr0), IDT_GATE_INTERRUPT);
    setEntry(1, reinterpret_cast<u64>(isr1), IDT_GATE_INTERRUPT);
    setEntry(2, reinterpret_cast<u64>(isr2), IDT_GATE_INTERRUPT);
    setEntry(3, reinterpret_cast<u64>(isr3), IDT_GATE_INTERRUPT);
    setEntry(4, reinterpret_cast<u64>(isr4), IDT_GATE_INTERRUPT);
    setEntry(5, reinterpret_cast<u64>(isr5), IDT_GATE_INTERRUPT);
    setEntry(6, reinterpret_cast<u64>(isr6), IDT_GATE_INTERRUPT);
    setEntry(7, reinterpret_cast<u64>(isr7), IDT_GATE_INTERRUPT);
    setEntry(8, reinterpret_cast<u64>(isr8), IDT_GATE_INTERRUPT, 1);
    setEntry(9, reinterpret_cast<u64>(isr9), IDT_GATE_INTERRUPT);
    setEntry(10, reinterpret_cast<u64>(isr10), IDT_GATE_INTERRUPT);
    setEntry(11, reinterpret_cast<u64>(isr11), IDT_GATE_INTERRUPT);
    setEntry(12, reinterpret_cast<u64>(isr12), IDT_GATE_INTERRUPT, 1);
    setEntry(13, reinterpret_cast<u64>(isr13), IDT_GATE_INTERRUPT);
    setEntry(14, reinterpret_cast<u64>(isr14), IDT_GATE_INTERRUPT);
    setEntry(15, reinterpret_cast<u64>(isr15), IDT_GATE_INTERRUPT);
    setEntry(16, reinterpret_cast<u64>(isr16), IDT_GATE_INTERRUPT);
    setEntry(17, reinterpret_cast<u64>(isr17), IDT_GATE_INTERRUPT);
    setEntry(18, reinterpret_cast<u64>(isr18), IDT_GATE_INTERRUPT);
    setEntry(19, reinterpret_cast<u64>(isr19), IDT_GATE_INTERRUPT);
    setEntry(20, reinterpret_cast<u64>(isr20), IDT_GATE_INTERRUPT);
    setEntry(21, reinterpret_cast<u64>(isr21), IDT_GATE_INTERRUPT);
    setEntry(22, reinterpret_cast<u64>(isr22), IDT_GATE_INTERRUPT);
    setEntry(23, reinterpret_cast<u64>(isr23), IDT_GATE_INTERRUPT);
    setEntry(24, reinterpret_cast<u64>(isr24), IDT_GATE_INTERRUPT);
    setEntry(25, reinterpret_cast<u64>(isr25), IDT_GATE_INTERRUPT);
    setEntry(26, reinterpret_cast<u64>(isr26), IDT_GATE_INTERRUPT);
    setEntry(27, reinterpret_cast<u64>(isr27), IDT_GATE_INTERRUPT);
    setEntry(28, reinterpret_cast<u64>(isr28), IDT_GATE_INTERRUPT);
    setEntry(29, reinterpret_cast<u64>(isr29), IDT_GATE_INTERRUPT);
    setEntry(30, reinterpret_cast<u64>(isr30), IDT_GATE_INTERRUPT);
    setEntry(31, reinterpret_cast<u64>(isr31), IDT_GATE_INTERRUPT);
    
    setEntry(32, reinterpret_cast<u64>(irq0), IDT_GATE_INTERRUPT);
    setEntry(33, reinterpret_cast<u64>(irq1), IDT_GATE_INTERRUPT);
    setEntry(34, reinterpret_cast<u64>(irq2), IDT_GATE_INTERRUPT);
    setEntry(35, reinterpret_cast<u64>(irq3), IDT_GATE_INTERRUPT);
    setEntry(36, reinterpret_cast<u64>(irq4), IDT_GATE_INTERRUPT);
    setEntry(37, reinterpret_cast<u64>(irq5), IDT_GATE_INTERRUPT);
    setEntry(38, reinterpret_cast<u64>(irq6), IDT_GATE_INTERRUPT);
    setEntry(39, reinterpret_cast<u64>(irq7), IDT_GATE_INTERRUPT);
    setEntry(40, reinterpret_cast<u64>(irq8), IDT_GATE_INTERRUPT);
    setEntry(41, reinterpret_cast<u64>(irq9), IDT_GATE_INTERRUPT);
    setEntry(42, reinterpret_cast<u64>(irq10), IDT_GATE_INTERRUPT);
    setEntry(43, reinterpret_cast<u64>(irq11), IDT_GATE_INTERRUPT);
    setEntry(44, reinterpret_cast<u64>(irq12), IDT_GATE_INTERRUPT);
    setEntry(45, reinterpret_cast<u64>(irq13), IDT_GATE_INTERRUPT);
    setEntry(46, reinterpret_cast<u64>(irq14), IDT_GATE_INTERRUPT);
    setEntry(47, reinterpret_cast<u64>(irq15), IDT_GATE_INTERRUPT);
    
    sPointer.limit = sizeof(sEntries) - 1;
    sPointer.base = reinterpret_cast<u64>(&sEntries);
    
    loadIdt();
}

void IDT::setHandler(u8 vector, InterruptHandler handler) {
    sHandlers[vector] = handler;
}

void IDT::enableInterrupts() {
    asm volatile("sti");
}

void IDT::disableInterrupts() {
    asm volatile("cli");
}

bool IDT::interruptsEnabled() {
    u64 flags;
    asm volatile("pushfq; pop %0" : "=r"(flags));
    return (flags & 0x200) != 0;
}

void IDT::setEntry(u8 vector, u64 handler, u8 typeAttr, u8 ist) {
    sEntries[vector].offsetLow = handler & 0xFFFF;
    sEntries[vector].selector = KERNEL_CODE_SELECTOR;
    sEntries[vector].ist = ist;
    sEntries[vector].typeAttr = typeAttr;
    sEntries[vector].offsetMiddle = (handler >> 16) & 0xFFFF;
    sEntries[vector].offsetHigh = (handler >> 32) & 0xFFFFFFFF;
    sEntries[vector].reserved = 0;
}

void IDT::loadIdt() {
    asm volatile("lidt %0" : : "m"(sPointer));
}

extern "C" void isr_handler(InterruptFrame* frame) {
    if (IDT::sHandlers[frame->interruptNumber]) {
        IDT::sHandlers[frame->interruptNumber](frame);
    }
}

}
