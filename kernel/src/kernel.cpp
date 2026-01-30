#include "../include/kernel.hpp"
#include "../include/graphics/framebuffer.hpp"
#include "../include/graphics/console.hpp"
#include "../include/memory/pmm.hpp"
#include "../include/memory/vmm.hpp"
#include "../include/cpu/gdt.hpp"
#include "../include/cpu/idt.hpp"
#include "../include/cpu/pic.hpp"
#include "../include/cpu/io.hpp"

namespace sertos {

boot::BootInfo* Kernel::sBootInfo = nullptr;

void Kernel::initialize(boot::BootInfo* bootInfo) {
    sBootInfo = bootInfo;
}

boot::BootInfo* Kernel::bootInfo() {
    return sBootInfo;
}

void Kernel::panic(const char* message) {
    cpu::cli();
    
    if (graphics::Console::rows() > 0) {
        graphics::Console::setForeground(graphics::Color::red());
        graphics::Console::println("\n*** KERNEL PANIC ***");
        graphics::Console::setForeground(graphics::Color::white());
        graphics::Console::print("Reason: ");
        graphics::Console::println(message);
        graphics::Console::println("\nSystem halted.");
    }
    
    halt();
}

void Kernel::halt() {
    cpu::cli();
    while (true) {
        cpu::hlt();
    }
}

}

namespace {

const char* exceptionMessages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

void exceptionHandler(sertos::cpu::InterruptFrame* frame) {
    using namespace sertos;
    using namespace sertos::graphics;
    
    cpu::cli();
    
    Console::setForeground(Color::red());
    Console::println("\n*** CPU EXCEPTION ***");
    Console::setForeground(Color::white());
    
    Console::print("Exception: ");
    if (frame->interruptNumber < 32) {
        Console::println(exceptionMessages[frame->interruptNumber]);
    } else {
        Console::print("Unknown (");
        Console::printDec(frame->interruptNumber);
        Console::println(")");
    }
    
    Console::print("Error Code: ");
    Console::printHex(frame->errorCode);
    Console::println("");
    
    Console::print("RIP: ");
    Console::printHex(frame->rip);
    Console::println("");
    
    Console::print("RSP: ");
    Console::printHex(frame->rsp);
    Console::println("");
    
    Console::print("RFLAGS: ");
    Console::printHex(frame->rflags);
    Console::println("");
    
    if (frame->interruptNumber == 14) {
        u64 cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        Console::print("CR2 (Faulting Address): ");
        Console::printHex(cr2);
        Console::println("");
    }
    
    Console::println("\nSystem halted.");
    
    Kernel::halt();
}

void timerHandler(sertos::cpu::InterruptFrame*) {
    sertos::cpu::PIC::sendEOI(0);
}

void keyboardHandler(sertos::cpu::InterruptFrame*) {
    using namespace sertos;
    
    u8 scancode = cpu::inb(0x60);
    
    static const char scancodeToAscii[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };
    
    if (!(scancode & 0x80) && scancode < sizeof(scancodeToAscii)) {
        char c = scancodeToAscii[scancode];
        if (c) {
            graphics::Console::putChar(c);
        }
    }
    
    cpu::PIC::sendEOI(1);
}

}

extern "C" void kernelMain(sertos::boot::BootInfo* bootInfo) {
    using namespace sertos;
    using namespace sertos::graphics;
    using namespace sertos::memory;
    using namespace sertos::cpu;
    
    Kernel::initialize(bootInfo);
    
    Framebuffer::initialize(bootInfo->framebuffer);
    Console::initialize();
    
    Console::setForeground(Color::cyan());
    Console::println("========================================");
    Console::println("         SertOS - UEFI Edition          ");
    Console::println("========================================");
    Console::setForeground(Color::white());
    Console::println("");
    
    Console::print("Framebuffer: ");
    Console::printDec(Framebuffer::width());
    Console::print("x");
    Console::printDec(Framebuffer::height());
    Console::print(" @ ");
    Console::printHex(Framebuffer::address());
    Console::println("");
    
    Console::print("Total Memory: ");
    Console::printDec(bootInfo->totalMemory / MB);
    Console::println(" MB");
    
    Console::print("Usable Memory: ");
    Console::printDec(bootInfo->usableMemory / MB);
    Console::println(" MB");
    
    Console::println("");
    Console::print("Initializing GDT... ");
    GDT::initialize();
    Console::setForeground(Color::green());
    Console::println("OK");
    Console::setForeground(Color::white());
    
    Console::print("Initializing IDT... ");
    IDT::initialize();
    Console::setForeground(Color::green());
    Console::println("OK");
    Console::setForeground(Color::white());
    
    Console::print("Initializing PIC... ");
    PIC::initialize();
    Console::setForeground(Color::green());
    Console::println("OK");
    Console::setForeground(Color::white());
    
    for (int i = 0; i < 32; i++) {
        IDT::setHandler(i, exceptionHandler);
    }
    
    IDT::setHandler(IRQ_TIMER, timerHandler);
    IDT::setHandler(IRQ_KEYBOARD, keyboardHandler);
    
    Console::print("Initializing PMM... ");
    PMM::initialize(bootInfo);
    if (PMM::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
        
        Console::print("  Free Pages: ");
        Console::printDec(PMM::freePages());
        Console::print(" (");
        Console::printDec(PMM::freeMemory() / MB);
        Console::println(" MB)");
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    PIC::clearMask(0);
    PIC::clearMask(1);
    
    Console::println("");
    Console::setForeground(Color::yellow());
    Console::println("Enabling interrupts...");
    Console::setForeground(Color::white());
    IDT::enableInterrupts();
    
    Console::println("");
    Console::setForeground(Color::green());
    Console::println("SertOS initialized successfully!");
    Console::setForeground(Color::white());
    Console::println("Type something (keyboard input enabled):");
    Console::println("");
    
    while (true) {
        hlt();
    }
}
