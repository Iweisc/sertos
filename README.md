# SertOS - A Custom Operating System

SertOS is a fully custom operating system built from scratch without using any existing OS kernels like Linux, Unix, or FreeBSD. It features a graphical user interface with a taskbar, start menu, and real-time clock.

## Features

- **Custom Bootloader**: 16-bit real mode to 32-bit protected mode transition
- **Custom Kernel**: Written entirely in C and x86 assembly
- **VGA Graphics**: 320x200 256-color mode (Mode 13h)
- **Interrupt Handling**: Full IDT with ISR and IRQ support
- **Keyboard Driver**: PS/2 keyboard support with scancode translation
- **Timer/RTC**: Programmable Interval Timer and Real-Time Clock
- **Memory Management**: Simple heap allocator with malloc/free
- **GUI Framework**: Desktop environment with:
  - Desktop background
  - Taskbar at the bottom
  - Start menu (toggle with Space key)
  - Real-time clock display

## Project Structure

```
sertos/
├── boot/
│   ├── boot.asm          # Main bootloader
│   ├── gdt.asm           # Global Descriptor Table
│   └── switch_pm.asm     # Protected mode switch
├── kernel/
│   ├── include/          # Header files
│   │   ├── types.h       # Basic type definitions
│   │   ├── ports.h       # I/O port functions
│   │   ├── vga.h         # VGA driver header
│   │   ├── idt.h         # IDT definitions
│   │   ├── keyboard.h    # Keyboard driver header
│   │   ├── timer.h       # Timer/RTC header
│   │   ├── heap.h        # Memory allocator header
│   │   ├── string.h      # String functions header
│   │   └── gui.h         # GUI framework header
│   ├── drivers/
│   │   ├── vga.c         # VGA graphics driver
│   │   ├── keyboard.c    # Keyboard driver
│   │   └── timer.c       # Timer and RTC driver
│   ├── cpu/
│   │   ├── idt.c         # IDT implementation
│   │   └── isr.asm       # Interrupt service routines
│   ├── mm/
│   │   └── heap.c        # Heap memory allocator
│   ├── lib/
│   │   └── string.c      # String utility functions
│   ├── gui/
│   │   └── gui.c         # GUI implementation
│   ├── kernel.c          # Main kernel entry
│   ├── kernel_entry.asm  # Kernel entry point
│   └── linker.ld         # Linker script
├── scripts/
│   └── build-cross-compiler.sh  # Cross compiler build script
├── Makefile              # Build system
└── README.md             # This file
```

## Building

### Prerequisites

1. **NASM** - Netwide Assembler
2. **QEMU** - For testing (qemu-system-i386)
3. **i686-elf Cross Compiler** - GCC cross compiler for i686-elf target

### Installing Dependencies

On Ubuntu/Debian:

```bash
sudo apt-get install nasm qemu-system-x86 build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo wget
```

### Building the Cross Compiler

```bash
chmod +x scripts/build-cross-compiler.sh
./scripts/build-cross-compiler.sh
```

Add the cross compiler to your PATH:

```bash
export PATH="$HOME/opt/cross/bin:$PATH"
```

### Building the OS

```bash
make clean
make
```

### Running

```bash
make run
```

This will launch QEMU with the OS image.

### Debugging

```bash
make debug
```

This starts QEMU in debug mode and connects GDB.

## Usage

- **Space**: Toggle start menu
- **Escape**: Close start menu
- The clock in the taskbar shows the current time from the RTC

## Technical Details

### Boot Process

1. BIOS loads the bootloader at 0x7C00
2. Bootloader sets up the stack and loads the kernel from disk
3. Bootloader switches from 16-bit real mode to 32-bit protected mode
4. Control is transferred to the kernel at 0x1000

### Memory Map

- 0x0000 - 0x7BFF: Free (used by BIOS)
- 0x7C00 - 0x7DFF: Bootloader
- 0x1000 - 0xFFFF: Kernel
- 0x100000+: Heap memory

### Graphics

The OS uses VGA Mode 13h (320x200, 256 colors) for graphics. The GUI is rendered using a custom graphics library that provides:

- Pixel plotting
- Rectangle drawing (filled and outlined)
- Line drawing (Bresenham's algorithm)
- Text rendering (8x8 bitmap font)

### Interrupts

- IRQ0 (32): Timer - 100Hz tick for clock updates
- IRQ1 (33): Keyboard - PS/2 keyboard input

## License

This project is provided as-is for educational purposes.

## Author

Custom OS development project.
