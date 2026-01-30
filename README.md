# SertOS - A UEFI-Based Operating System

SertOS is a minimal operating system built from scratch following LFS (Linux From Scratch) principles. It boots via UEFI (not legacy BIOS) and is designed with modern x86_64 architecture in mind.

## Features

- **UEFI Boot**: Native UEFI bootloader with PE/COFF executable format
- **Graphics Output Protocol (GOP)**: Direct framebuffer access for graphics
- **Memory Management**: UEFI memory map parsing and total RAM detection
- **GDT/IDT Setup**: Proper x86_64 descriptor tables
- **64-bit Long Mode**: Full 64-bit operation from the start

## Project Structure

```
sertos/
├── boot/
│   └── src/
│       └── minimal_efi.asm    # UEFI bootloader (assembly)
├── kernel/
│   ├── include/               # C++ headers (for future expansion)
│   │   ├── types.hpp
│   │   ├── kernel.hpp
│   │   ├── uefi/             # UEFI protocol definitions
│   │   ├── graphics/         # Framebuffer and console
│   │   ├── memory/           # PMM and VMM
│   │   └── cpu/              # GDT, IDT, interrupts
│   └── src/                   # C++ implementation files
├── build/                     # Build output directory
├── Makefile                   # Build system
└── README.md                  # This file
```

## Requirements

### Build Dependencies

- **NASM**: Netwide Assembler for x86_64 assembly
- **QEMU**: For testing (qemu-system-x86_64)
- **OVMF**: UEFI firmware for QEMU
- **dosfstools**: For creating FAT32 filesystem (mkfs.fat)

### Installing Dependencies

**Debian/Ubuntu:**

```bash
sudo apt install nasm qemu-system-x86 ovmf dosfstools
```

**Arch Linux:**

```bash
sudo pacman -S nasm qemu ovmf dosfstools
```

**Fedora:**

```bash
sudo dnf install nasm qemu ovmf dosfstools
```

## Building

```bash
# Build the OS image
make

# Clean build artifacts
make clean
```

## Running

```bash
# Run in QEMU with UEFI
make run

# Run with debugging (GDB stub enabled)
make debug
```

## Boot Output

When SertOS boots successfully, you'll see:

```
==========================================
              SertOS v1.0
==========================================

[....] Disabling watchdog timer
       [ OK ]
[....] Initializing graphics
       [ OK ]
       Resolution: 1280x800
[....] Getting memory map
       [ OK ]
       Total RAM: 206 MB
[....] Initializing kernel
       [ OK ]

Boot complete!

Entering kernel main loop...
```

## Technical Details

### UEFI Bootloader

The bootloader is a self-contained PE/COFF executable written in x86_64 assembly. It:

1. Creates a valid PE header with DOS stub
2. Clears the screen using UEFI ConOut protocol
3. Disables the watchdog timer
4. Locates and initializes the Graphics Output Protocol (GOP)
5. Retrieves the UEFI memory map
6. Sets up GDT and IDT
7. Enters the kernel main loop

### Memory Layout

- **PE Image Base**: Relocated by UEFI loader
- **Code Section**: 0x1000 (virtual address)
- **Relocation Section**: 0x8000 (virtual address)
- **Framebuffer**: Provided by GOP (typically at high physical address)

### Calling Convention

The bootloader uses the Microsoft x64 calling convention as required by UEFI:

- First 4 arguments: RCX, RDX, R8, R9
- Return value: RAX
- Caller-saved: RAX, RCX, RDX, R8, R9, R10, R11
- Callee-saved: RBX, RBP, RDI, RSI, R12-R15

### UEFI Protocols Used

- **EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL**: Console output
- **EFI_GRAPHICS_OUTPUT_PROTOCOL**: Framebuffer access
- **EFI_BOOT_SERVICES**: Memory map, watchdog timer

## Architecture

### Why Assembly?

The bootloader is written in pure assembly to:

1. Have complete control over the PE/COFF header format
2. Avoid C runtime dependencies
3. Ensure proper UEFI ABI compliance
4. Minimize binary size

### Future C++ Integration

The kernel directory contains C++ headers and source files for future expansion. To integrate C++ code:

1. Compile C++ to position-independent object files
2. Link with a custom linker script
3. Call from the assembly bootloader after exiting boot services

## Extending SertOS

### Adding Interrupt Handlers

1. Populate the IDT entries in the assembly code
2. Write interrupt service routines
3. Load the IDT with `lidt`

### Adding Filesystem Support

1. Use UEFI Simple File System Protocol before exiting boot services
2. Load kernel modules from the EFI System Partition
3. Implement a VFS layer in the kernel

### Adding Process Management

1. Implement a scheduler
2. Set up TSS (Task State Segment)
3. Implement context switching

## License

This project is provided as-is for educational purposes.

## References

- [UEFI Specification](https://uefi.org/specifications)
- [OSDev Wiki](https://wiki.osdev.org/)
- [Intel 64 and IA-32 Architectures Software Developer's Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
