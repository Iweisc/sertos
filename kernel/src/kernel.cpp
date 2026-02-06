#include "../include/kernel.hpp"
#include "../include/graphics/framebuffer.hpp"
#include "../include/graphics/console.hpp"
#include "../include/memory/pmm.hpp"
#include "../include/memory/vmm.hpp"
#include "../include/cpu/gdt.hpp"
#include "../include/cpu/idt.hpp"
#include "../include/cpu/pic.hpp"
#include "../include/cpu/io.hpp"
#include "../include/cpu/smp.hpp"
#include "../include/disk/ata.hpp"
#include "../include/fs/sertfs.hpp"
#include "../include/input/keyboard.hpp"
#include "../include/input/mouse.hpp"
#include "../include/shell/shell.hpp"
#include "../include/process/process.hpp"
#include "../include/process/scheduler.hpp"
#include "../include/syscall/syscall.hpp"
#include "../include/ipc/ipc.hpp"
#include "../include/user/user.hpp"
#include "../include/security/security.hpp"
#include "../include/power/acpi.hpp"
#include "../include/drivers/usb.hpp"
#include "../include/drivers/audio.hpp"
#include "../include/drivers/gpu.hpp"
#include "../include/loader/dynamic.hpp"
#include "../include/wm/wm.hpp"
#include "../include/apps/app.hpp"
#include "../include/drivers/pci.hpp"
#include "../include/drivers/virtio_net.hpp"
#include "../include/net/net.hpp"
#include "../include/net/socket.hpp"
#include "../include/net/dns.hpp"
#include "../include/net/http.hpp"

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
    input::Keyboard::handleScancode(scancode);
    cpu::PIC::sendEOI(1);
}

}

extern "C" void kernelMain(sertos::boot::BootInfo* bootInfo) {
    using namespace sertos;
    using namespace sertos::graphics;
    using namespace sertos::memory;
    using namespace sertos::cpu;
    using namespace sertos::fs;
    using namespace sertos::disk;
    using namespace sertos::input;
    using namespace sertos::shell;
    using namespace sertos::process;
    using namespace sertos::ipc;
    using namespace sertos::user;
    using namespace sertos::security;
    using namespace sertos::power;
    using namespace sertos::drivers;
    using namespace sertos::loader;
    
    Kernel::initialize(bootInfo);
    
    Framebuffer::initialize(bootInfo->framebuffer);
    Console::initialize();
    
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
    // Keyboard uses polling instead of IRQ to avoid dual-read race on port 0x60
    
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
        Kernel::panic("Failed to initialize physical memory manager");
    }
    
    Console::print("Initializing Keyboard... ");
    Keyboard::initialize();
    Console::setForeground(Color::green());
    Console::println("OK");
    Console::setForeground(Color::white());
    
    Console::print("Initializing Mouse... ");
    Mouse::initialize();
    if (Mouse::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::yellow());
        Console::println("Not available");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing ATA... ");
    ATA::initialize();
    if (ATA::driveCount() > 0) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
        
        Console::print("  Drives found: ");
        Console::printDec(ATA::driveCount());
        Console::println("");
        
        for (u8 i = 0; i < ATA::driveCount(); i++) {
            AtaDrive* drive = ATA::getDrive(i);
            if (drive && drive->present) {
                Console::print("  Drive ");
                Console::printDec(i);
                Console::print(": ");
                Console::print(drive->model);
                Console::print(" (");
                Console::printDec(ATA::capacity(i) / MB);
                Console::println(" MB)");
            }
        }
    } else {
        Console::setForeground(Color::yellow());
        Console::println("No drives found");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing SertFS... ");
    bool fsReady = false;
    
    u8 storageDrive = ATA::driveCount() > 1 ? 1 : 0;
    
    if (ATA::driveCount() > 0) {
        if (SertFs::initialize(storageDrive)) {
            if (SertFs::mount("/")) {
                Console::setForeground(Color::green());
                Console::println("OK (mounted existing filesystem)");
                Console::setForeground(Color::white());
                fsReady = true;
            } else {
                Console::setForeground(Color::yellow());
                Console::println("Formatting...");
                Console::setForeground(Color::white());
                
                if (SertFs::format(storageDrive, "SertOS")) {
                    if (SertFs::mount("/")) {
                        Console::print("  ");
                        Console::setForeground(Color::green());
                        Console::println("Filesystem created and mounted");
                        Console::setForeground(Color::white());
                        fsReady = true;
                    }
                }
            }
        }
    }
    
    if (!fsReady) {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
        Console::println("  Warning: No persistent storage available");
    }
    
    Console::print("Initializing VMM... ");
    VMM::initialize();
    if (VMM::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing Process Manager... ");
    PM::initialize();
    if (PM::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing Scheduler... ");
    Scheduler::initialize();
    if (Scheduler::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing Syscalls... ");
    syscall::Syscall::initialize();
    syscall::registerAllHandlers();
    Console::setForeground(Color::green());
    Console::println("OK");
    Console::setForeground(Color::white());
    
    Console::print("Initializing IPC... ");
    IPC::initialize();
    if (IPC::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing User Manager... ");
    UserManager::initialize();
    if (UserManager::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing Security Manager... ");
    SecurityManager::initialize();
    if (SecurityManager::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing ACPI/Power Management... ");
    PowerManager::initialize();
    if (PowerManager::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::yellow());
        Console::println("Not available");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing SMP... ");
    SMP::initialize();
    if (SMP::isInitialized()) {
        Console::setForeground(Color::green());
        Console::print("OK (");
        Console::printDec(SMP::cpuCount());
        Console::println(" CPUs)");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::yellow());
        Console::println("Single CPU mode");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing USB... ");
    UsbDriver::initialize();
    if (UsbDriver::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::yellow());
        Console::println("No USB controller found");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing Audio... ");
    AudioDriver::initialize();
    if (AudioDriver::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::yellow());
        Console::println("No audio device found");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing GPU... ");
    GpuDriver::initialize();
    if (GpuDriver::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::yellow());
        Console::println("Using framebuffer only");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing Dynamic Linker... ");
    DynamicLinker::initialize();
    if (DynamicLinker::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::yellow());
        Console::println("Static linking only");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing Window Manager... ");
    wm::WindowManager::initialize();
    if (wm::WindowManager::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing Taskbar... ");
    wm::Taskbar::initialize();
    if (wm::Taskbar::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    Console::print("Initializing App Manager... ");
    apps::AppManager::initialize();
    if (apps::AppManager::isInitialized()) {
        Console::setForeground(Color::green());
        Console::println("OK");
        Console::setForeground(Color::white());
    } else {
        Console::setForeground(Color::red());
        Console::println("FAILED");
        Console::setForeground(Color::white());
    }
    
    // Initialize networking
    Console::print("Initializing PCI... ");
    drivers::Pci::initialize();
    Console::setForeground(Color::green());
    Console::print("OK");
    Console::setForeground(Color::white());
    Console::print(" (");
    Console::printDec(drivers::Pci::deviceCount());
    Console::println(" devices)");

    Console::print("Initializing Network Stack... ");
    net::NetworkStack::initialize();
    net::SocketManager::initialize();
    Console::setForeground(Color::green());
    Console::println("OK");
    Console::setForeground(Color::white());

    Console::print("Initializing VirtIO Network... ");
    drivers::VirtioNet::initialize();
    auto* pciNet = drivers::Pci::findDevice(0x1AF4, 0x1000);
    if (pciNet) {
        drivers::VirtioNet::probe(pciNet->bus, pciNet->device, pciNet->function);
        if (drivers::VirtioNet::init(0)) {
            auto* iface = net::NetworkStack::getDefaultInterface();
            if (iface) {
                net::NetworkStack::configureInterface(iface->id,
                    net::IPv4Address(10, 0, 2, 15),
                    net::IPv4Address(255, 255, 255, 0),
                    net::IPv4Address(10, 0, 2, 2));
                iface->dns = net::IPv4Address(10, 0, 2, 3);
                net::NetworkStack::setInterfaceUp(iface->id);
            }
            net::MacAddress mac = drivers::VirtioNet::getMacAddress(0);
            Console::setForeground(Color::green());
            Console::print("OK");
            Console::setForeground(Color::white());
            Console::print(" (MAC: ");
            for (int mi = 0; mi < 6; mi++) {
                if (mi > 0) Console::print(":");
                Console::printHex(mac.bytes[mi]);
            }
            Console::println(")");
        } else {
            Console::setForeground(Color::red());
            Console::println("INIT FAILED");
            Console::setForeground(Color::white());
        }
    } else {
        Console::setForeground(Color::yellow());
        Console::println("No VirtIO NIC found");
        Console::setForeground(Color::white());
    }

    Console::print("Initializing DNS... ");
    net::DnsResolver::initialize();
    Console::setForeground(Color::green());
    Console::println("OK");
    Console::setForeground(Color::white());

    Console::print("Initializing HTTP... ");
    net::HttpClient::initialize();
    Console::setForeground(Color::green());
    Console::println("OK");
    Console::setForeground(Color::white());

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
    
    if (fsReady) {
        Console::print("Disk space: ");
        Console::printDec(SertFs::freeSpace() / MB);
        Console::print(" MB free / ");
        Console::printDec(SertFs::totalSpace() / MB);
        Console::println(" MB total");
    }
    
    Console::println("");
    Console::println("Starting graphical interface...");
    
    wm::WindowManager::setDesktopColor(Color(0, 80, 120));
    
    static u32 nextWindowX = 100;
    static u32 nextWindowY = 50;
    
    auto launchApp = [](wm::AppType appType) {
        using namespace wm;
        
        WindowFlags flags = WindowFlags::Visible |
                           WindowFlags::Movable |
                           WindowFlags::Resizable |
                           WindowFlags::HasTitlebar |
                           WindowFlags::HasBorder;
        
        const char* title = "Window";
        u32 width = 600;
        u32 height = 400;
        Color bgColor(240, 240, 240);
        
        switch (appType) {
            case AppType::Terminal:
                title = "Terminal";
                width = 800;
                height = 500;
                bgColor = Color(30, 30, 30);
                break;
            case AppType::FileManager:
                title = "File Manager";
                width = 700;
                height = 450;
                bgColor = Color(250, 250, 250);
                break;
            case AppType::TextEditor:
                title = "Text Editor";
                width = 650;
                height = 500;
                bgColor = Color(255, 255, 255);
                break;
            case AppType::Settings:
                title = "Settings";
                width = 500;
                height = 400;
                bgColor = Color(245, 245, 245);
                break;
            case AppType::About:
                title = "About SertOS";
                width = 400;
                height = 300;
                bgColor = Color(240, 240, 250);
                break;
            case AppType::Browser:
                title = "Web Browser";
                width = 800;
                height = 600;
                bgColor = Color(250, 250, 250);
                break;
        }
        
        u32 winId = WindowManager::createWindow(title,
            static_cast<i32>(nextWindowX),
            static_cast<i32>(nextWindowY),
            width, height, flags);
        
        if (winId != 0) {
            WindowManager::setWindowBackgroundColor(winId, bgColor);
            WindowManager::focusWindow(winId);
            
            sertos::apps::AppManager::createApp(appType, winId);
            
            nextWindowX += 30;
            nextWindowY += 30;
            if (nextWindowX > 400) nextWindowX = 100;
            if (nextWindowY > 300) nextWindowY = 50;
        }
    };
    
    wm::StartMenu::setLaunchCallback(launchApp);
    
    wm::WindowManager::render();
    wm::Taskbar::render();
    
    bool needsRedraw = true;
    i32 lastMouseX = Mouse::x();
    i32 lastMouseY = Mouse::y();
    i32 lastClickX = 0;
    i32 lastClickY = 0;
    u32 lastClickWindow = 0;
    
    while (true) {
        bool inputProcessed = false;
        
        Mouse::poll();
        
        while (Mouse::hasEvent()) {
            MouseEvent mouseEvent = Mouse::getEvent();
            i32 mx = Mouse::x();
            i32 my = Mouse::y();
            
            bool taskbarHandled = false;
            if (hasButton(mouseEvent.pressed, MouseButton::Left)) {
                taskbarHandled = wm::Taskbar::handleClick(mx, my);
            }
            
            if (!taskbarHandled) {
                wm::WindowManager::handleMouseEvent(mouseEvent);
                if (hasButton(mouseEvent.pressed, MouseButton::Left)) {
                    u32 focused = wm::WindowManager::focusedWindow();
                    if (focused != 0) {
                        bool sameSpot = (mx - lastClickX) * (mx - lastClickX) + (my - lastClickY) * (my - lastClickY) <= 9;
                        bool doubleClick = (focused == lastClickWindow) && sameSpot;
                        lastClickX = mx;
                        lastClickY = my;
                        lastClickWindow = focused;
                        apps::AppManager::handleMouseClick(focused, mx, my, doubleClick);
                    }
                }
            }
            
            inputProcessed = true;
        }
        
        if (Mouse::x() != lastMouseX || Mouse::y() != lastMouseY) {
            lastMouseX = Mouse::x();
            lastMouseY = Mouse::y();
            wm::Taskbar::handleMouseMove(lastMouseX, lastMouseY);
            needsRedraw = true;
        }
        
        // Poll keyboard data from PS/2 controller
        while (true) {
            u8 kbStatus = inb(0x64);
            if ((kbStatus & 0x01) && !(kbStatus & 0x20)) {
                u8 scancode = inb(0x60);
                Keyboard::handleScancode(scancode);
            } else {
                break;
            }
        }

        // Dispatch buffered key events to apps
        while (Keyboard::hasKey()) {
            KeyEvent event = Keyboard::getKey();
            if (event.pressed) {
                bool windowAction = false;

                if (event.alt) {
                    i32 moveStep = wm::WindowManager::MOVE_STEP;
                    switch (event.code) {
                        case KeyCode::Up:
                            wm::WindowManager::moveFocusedWindow(0, -moveStep);
                            windowAction = true;
                            break;
                        case KeyCode::Down:
                            wm::WindowManager::moveFocusedWindow(0, moveStep);
                            windowAction = true;
                            break;
                        case KeyCode::Left:
                            wm::WindowManager::moveFocusedWindow(-moveStep, 0);
                            windowAction = true;
                            break;
                        case KeyCode::Right:
                            wm::WindowManager::moveFocusedWindow(moveStep, 0);
                            windowAction = true;
                            break;
                        default:
                            break;
                    }
                } else if (event.ctrl) {
                    i32 resizeStep = wm::WindowManager::RESIZE_STEP;
                    switch (event.code) {
                        case KeyCode::Up:
                            wm::WindowManager::resizeFocusedWindow(0, -resizeStep);
                            windowAction = true;
                            break;
                        case KeyCode::Down:
                            wm::WindowManager::resizeFocusedWindow(0, resizeStep);
                            windowAction = true;
                            break;
                        case KeyCode::Left:
                            wm::WindowManager::resizeFocusedWindow(-resizeStep, 0);
                            windowAction = true;
                            break;
                        case KeyCode::Right:
                            wm::WindowManager::resizeFocusedWindow(resizeStep, 0);
                            windowAction = true;
                            break;
                        default:
                            break;
                    }
                }

                if (windowAction) {
                    needsRedraw = true;
                } else if (event.code == KeyCode::Tab) {
                    wm::WindowManager::cycleWindowFocus();
                    needsRedraw = true;
                } else {
                    u32 focused = wm::WindowManager::focusedWindow();
                    if (focused != 0) {
                        apps::App* focusedApp = apps::AppManager::getApp(focused);
                        if (focusedApp != nullptr) {
                            apps::AppManager::handleKeyPress(focused, event.code, event.ascii, event.ctrl, event.alt, event.shift);
                            needsRedraw = true;
                        }
                    }
                }
            }
        }
        
        // Poll network for incoming packets
        drivers::VirtioNet::receivePackets(0);

        if (inputProcessed) {
            needsRedraw = true;
        }

        if (needsRedraw) {
            wm::WindowManager::render();
            
            wm::Taskbar::render();
            wm::WindowManager::renderCursor();
            needsRedraw = false;
        }
        
        asm volatile("pause");
    }
}
