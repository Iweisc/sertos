# SertOS

A hobby operating system written from scratch in C++ and x86_64 assembly. Boots via UEFI on modern x86_64 hardware.

## What It Does

SertOS is a functional operating system featuring:

- **UEFI Bootloader** - Pure assembly bootloader that initializes graphics, memory, and hands off to the kernel
- **C++ Kernel** - Modular kernel with proper GDT/IDT setup, interrupt handling, and memory management
- **Physical Memory Manager** - Bitmap-based page allocator with contiguous allocation support
- **Virtual Memory Manager** - Page table management for process isolation
- **Custom Filesystem (SertFS)** - Unix-like filesystem with inodes, directories, and file operations
- **ATA Disk Driver** - PIO-mode ATA driver for reading/writing to IDE disks
- **Interactive Shell** - Full shell with 18 commands for navigating and managing the system
- **PS/2 Keyboard Driver** - Interrupt-driven keyboard input with scancode translation
- **Process Management** - Process creation, forking, and state management
- **ELF Loader** - Load and execute ELF64 binaries
- **Syscall Interface** - System call infrastructure for userspace programs
- **Userspace Support** - Run programs in ring 3 with a minimal libc

## Shell Commands

```
help        Display available commands
clear       Clear the screen
echo        Print text to console
pwd         Print working directory
cd          Change directory
ls          List directory contents
mkdir       Create a directory
touch       Create an empty file
rm          Remove a file or empty directory
cat         Display file contents
write       Write text to a file
mv          Move/rename a file or directory
cp          Copy a file
stat        Display file information
tree        Display directory tree
mem         Display memory information
df          Display disk space information
disk        Display disk information
```

## System Calls

| Number | Name      | Description                    |
|--------|-----------|--------------------------------|
| 0      | exit      | Terminate process              |
| 1      | write     | Write to file descriptor       |
| 2      | read      | Read from file descriptor      |
| 3      | open      | Open a file                    |
| 4      | close     | Close a file descriptor        |
| 5      | mmap      | Map memory                     |
| 6      | munmap    | Unmap memory                   |
| 7      | brk       | Adjust program break           |
| 8      | getpid    | Get process ID                 |
| 9      | fork      | Fork process                   |
| 10     | exec      | Execute program                |
| 11     | wait      | Wait for child process         |
| 12     | yield     | Yield CPU                      |
| 13     | sleep     | Sleep for duration             |
| 14     | gettime   | Get system time                |

## Building

### Dependencies

**Debian/Ubuntu:**
```bash
sudo apt install nasm qemu-system-x86 ovmf dosfstools x86_64-w64-mingw32-g++
```

**Arch Linux:**
```bash
sudo pacman -S nasm qemu ovmf dosfstools mingw-w64-gcc
```

**Fedora:**
```bash
sudo dnf install nasm qemu ovmf dosfstools mingw64-gcc-c++
```

### Build & Run

```bash
# Build everything
make

# Run in QEMU
make run

# Run with GDB debugging
make debug

# Clean build artifacts
make clean
```

## Project Structure

```
sertos/
├── boot/
│   ├── src/
│   │   ├── main.cpp            # C++ UEFI entry point
│   │   ├── minimal_efi.asm     # Assembly UEFI bootloader
│   │   └── uefi_entry.asm      # UEFI entry stub
│   └── include/
│       └── bootinfo.hpp        # Boot information structure
├── kernel/
│   ├── src/
│   │   ├── kernel.cpp          # Kernel main and panic handler
│   │   ├── cpu/
│   │   │   ├── gdt.cpp         # Global Descriptor Table
│   │   │   ├── idt.cpp         # Interrupt Descriptor Table
│   │   │   ├── pic.cpp         # Programmable Interrupt Controller
│   │   │   └── isr.asm         # Interrupt service routines
│   │   ├── memory/
│   │   │   ├── pmm.cpp         # Physical memory manager
│   │   │   ├── vmm.cpp         # Virtual memory manager
│   │   │   └── new.cpp         # Operator new/delete
│   │   ├── graphics/
│   │   │   ├── framebuffer.cpp # GOP framebuffer driver
│   │   │   ├── console.cpp     # Text console
│   │   │   └── font.cpp        # Built-in font
│   │   ├── disk/
│   │   │   └── ata.cpp         # ATA disk driver
│   │   ├── fs/
│   │   │   └── sertfs.cpp      # SertFS filesystem
│   │   ├── input/
│   │   │   └── keyboard.cpp    # PS/2 keyboard driver
│   │   ├── shell/
│   │   │   └── shell.cpp       # Interactive shell
│   │   ├── process/
│   │   │   └── process.cpp     # Process management
│   │   ├── syscall/
│   │   │   └── syscall.cpp     # System call handling
│   │   ├── loader/
│   │   │   └── elf.cpp         # ELF binary loader
│   │   └── uefi/
│   │       └── uefi.cpp        # UEFI protocol wrappers
│   └── include/
│       └── ...                 # Headers for all modules
├── userspace/
│   ├── libc/
│   │   ├── include/
│   │   │   ├── stdio.hpp       # printf, puts, etc.
│   │   │   ├── unistd.hpp      # getpid, _exit, etc.
│   │   │   └── types.hpp       # Type definitions
│   │   └── src/
│   │       └── ...             # libc implementation
│   └── programs/
│       └── hello/              # Hello world userspace program
├── Makefile
└── README.md
```

## Technical Details

### Memory Management

- Page size: 4KB
- Bitmap allocator tracks all physical pages
- First 1MB reserved (legacy region)
- Supports single and contiguous multi-page allocation
- Per-process page tables for virtual memory isolation

### SertFS Filesystem

- Block size: 4KB
- Inode-based (similar to ext2)
- 12 direct blocks + indirect block per inode
- Directory entries with variable-length names
- Supports files up to ~50MB (direct + single indirect)

### Process Model

- Up to 256 concurrent processes
- States: Ready, Running, Blocked, Sleeping, Zombie, Terminated
- Per-process kernel and user stacks
- Full CPU context save/restore
- Parent-child relationships for wait/exit

### ELF Loader

- Loads ELF64 executables
- Supports ET_EXEC and ET_DYN (PIE) binaries
- Maps PT_LOAD segments with proper permissions
- Sets up userspace entry point

### Boot Process

1. UEFI firmware loads `BOOTX64.EFI` from EFI System Partition
2. Bootloader initializes GOP for graphics output
3. Retrieves UEFI memory map
4. Sets up minimal GDT
5. Jumps to kernel entry point with boot info
6. Kernel initializes GDT, IDT, PIC, PMM, VMM
7. Initializes ATA driver and mounts SertFS
8. Sets up syscall infrastructure
9. Launches interactive shell

### Hardware Requirements

- x86_64 processor
- UEFI firmware (no legacy BIOS support)
- At least 64MB RAM
- IDE/ATA storage (for SertFS)

## Userspace Example

```cpp
#include "libc/include/stdio.hpp"
#include "libc/include/unistd.hpp"

using namespace sertos::libc;

extern "C" void _start() {
    printf("Hello from userspace!\n");
    printf("My PID is: %d\n", getpid());
    _exit(0);
}
```

## What's Next

- [ ] Preemptive scheduler
- [ ] More syscalls (execve, pipe, dup)
- [ ] VFS layer
- [ ] Network stack
- [ ] AHCI/NVMe support
- [ ] USB support
- [ ] Sound

## License

Educational project. Do whatever you want with it.
