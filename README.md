# SertOS

A hobby operating system written from scratch in C++ and x86_64 assembly. Boots via UEFI on modern x86_64 hardware.

## What It Does

SertOS is a functional operating system featuring:

### Core Features

- **UEFI Bootloader** - Pure assembly bootloader that initializes graphics, memory, and hands off to the kernel
- **C++ Kernel** - Modular kernel with proper GDT/IDT setup, interrupt handling, and memory management
- **Physical Memory Manager** - Bitmap-based page allocator with contiguous allocation support
- **Virtual Memory Manager** - Page table management for process isolation
- **Custom Filesystem (SertFS)** - Unix-like filesystem with inodes, directories, and file operations
- **ATA Disk Driver** - PIO-mode ATA driver for reading/writing to IDE disks
- **Interactive Shell** - Full shell with 18 commands for navigating and managing the system
- **PS/2 Keyboard Driver** - Interrupt-driven keyboard input with scancode translation

### Process Management

- **Process Manager** - Process creation, forking, and state management
- **Preemptive Scheduler** - Round-robin scheduling with priority support
- **ELF Loader** - Load and execute ELF64 binaries
- **Dynamic Linker** - Shared library support with symbol resolution and relocations
- **Syscall Interface** - Comprehensive system call infrastructure (77+ syscalls)
- **Userspace Support** - Run programs in ring 3 with a minimal libc

### Multi-User Support

- **User Management** - User and group management with UIDs/GIDs
- **Authentication** - Password-based authentication with hashing
- **Sessions** - Login sessions with session IDs
- **Capabilities** - Fine-grained capability-based permissions

### IPC Mechanisms

- **Pipes** - Unidirectional byte streams for process communication
- **Shared Memory** - POSIX-style shared memory segments (shmget/shmat/shmdt)
- **Message Queues** - POSIX-style message passing (msgget/msgsnd/msgrcv)
- **Signals** - POSIX signals with custom handlers (kill/signal/sigaction)

### Device Drivers

- **USB Support** - UHCI, OHCI, EHCI, and XHCI controller support
- **Audio** - HD Audio and AC97 sound card support
- **GPU Acceleration** - Bochs VGA, VirtIO GPU, and VMware SVGA support

### SMP Support

- **Multi-Processor** - LAPIC and IOAPIC initialization
- **CPU Discovery** - Automatic detection of available processors
- **IPI** - Inter-processor interrupts for coordination
- **Spinlocks** - SMP-safe synchronization primitives

### Power Management

- **ACPI** - ACPI table parsing (RSDP, RSDT, XSDT, FADT, MADT)
- **Shutdown** - Clean system shutdown via ACPI
- **Reboot** - System reboot support
- **Sleep States** - S1-S5 sleep state support

### Security

- **Memory Protection** - W^X enforcement, per-process page tables
- **ASLR** - Address space layout randomization
- **Capabilities** - Linux-style capability system
- **Access Control** - Discretionary and mandatory access control
- **Audit Logging** - Security event logging

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

### Process Management

| Number | Name    | Description            |
| ------ | ------- | ---------------------- |
| 0      | exit    | Terminate process      |
| 8      | getpid  | Get process ID         |
| 9      | fork    | Fork process           |
| 10     | exec    | Execute program        |
| 11     | wait    | Wait for child process |
| 12     | yield   | Yield CPU              |
| 13     | sleep   | Sleep for duration     |
| 57     | getppid | Get parent process ID  |
| 58     | getpgid | Get process group ID   |
| 59     | setpgid | Set process group ID   |
| 60     | setsid  | Create new session     |
| 61     | getsid  | Get session ID         |

### File Operations

| Number | Name   | Description               |
| ------ | ------ | ------------------------- |
| 1      | write  | Write to file descriptor  |
| 2      | read   | Read from file descriptor |
| 3      | open   | Open a file               |
| 4      | close  | Close a file descriptor   |
| 23     | pipe   | Create pipe               |
| 24     | dup    | Duplicate file descriptor |
| 25     | dup2   | Duplicate to specific fd  |
| 26     | lseek  | Seek in file              |
| 27     | stat   | Get file status           |
| 28     | fstat  | Get file status by fd     |
| 29     | mkdir  | Create directory          |
| 30     | rmdir  | Remove directory          |
| 31     | unlink | Remove file               |
| 32     | rename | Rename file               |
| 33     | chdir  | Change directory          |
| 34     | getcwd | Get current directory     |

### Memory Management

| Number | Name     | Description           |
| ------ | -------- | --------------------- |
| 5      | mmap     | Map memory            |
| 6      | munmap   | Unmap memory          |
| 7      | brk      | Adjust program break  |
| 56     | mprotect | Set memory protection |

### User/Group Management

| Number | Name    | Description            |
| ------ | ------- | ---------------------- |
| 15     | getuid  | Get user ID            |
| 16     | getgid  | Get group ID           |
| 17     | setuid  | Set user ID            |
| 18     | setgid  | Set group ID           |
| 19     | geteuid | Get effective user ID  |
| 20     | getegid | Get effective group ID |
| 21     | seteuid | Set effective user ID  |
| 22     | setegid | Set effective group ID |

### Signals

| Number | Name        | Description            |
| ------ | ----------- | ---------------------- |
| 35     | kill        | Send signal to process |
| 36     | signal      | Set signal handler     |
| 37     | sigaction   | Set signal action      |
| 38     | sigprocmask | Block/unblock signals  |
| 39     | sigsuspend  | Wait for signal        |

### IPC - Shared Memory

| Number | Name   | Description               |
| ------ | ------ | ------------------------- |
| 40     | shmget | Get shared memory segment |
| 41     | shmat  | Attach shared memory      |
| 42     | shmdt  | Detach shared memory      |
| 43     | shmctl | Shared memory control     |

### IPC - Message Queues

| Number | Name   | Description           |
| ------ | ------ | --------------------- |
| 44     | msgget | Get message queue     |
| 45     | msgsnd | Send message          |
| 46     | msgrcv | Receive message       |
| 47     | msgctl | Message queue control |

### Time

| Number | Name          | Description           |
| ------ | ------------- | --------------------- |
| 14     | gettime       | Get system time       |
| 64     | clock_gettime | Get clock time        |
| 65     | clock_settime | Set clock time        |
| 66     | nanosleep     | High-resolution sleep |

### System

| Number | Name    | Description            |
| ------ | ------- | ---------------------- |
| 70     | uname   | Get system information |
| 71     | sysinfo | Get system statistics  |
| 75     | reboot  | Reboot/shutdown system |
| 76     | sync    | Sync filesystems       |

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
│   │   │   ├── smp.cpp         # Symmetric Multi-Processing
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
│   │   ├── drivers/
│   │   │   ├── usb.cpp         # USB controller driver
│   │   │   ├── audio.cpp       # Audio driver (HDA/AC97)
│   │   │   └── gpu.cpp         # GPU acceleration driver
│   │   ├── fs/
│   │   │   └── sertfs.cpp      # SertFS filesystem
│   │   ├── input/
│   │   │   └── keyboard.cpp    # PS/2 keyboard driver
│   │   ├── shell/
│   │   │   └── shell.cpp       # Interactive shell
│   │   ├── process/
│   │   │   ├── process.cpp     # Process management
│   │   │   ├── scheduler.cpp   # Process scheduler
│   │   │   └── context.asm     # Context switching
│   │   ├── syscall/
│   │   │   ├── syscall.cpp     # System call handling
│   │   │   ├── handlers.cpp    # Syscall implementations
│   │   │   └── syscall_entry.asm # Syscall entry point
│   │   ├── ipc/
│   │   │   └── ipc.cpp         # IPC mechanisms
│   │   ├── user/
│   │   │   └── user.cpp        # User management
│   │   ├── security/
│   │   │   └── security.cpp    # Security subsystem
│   │   ├── power/
│   │   │   └── acpi.cpp        # ACPI/power management
│   │   ├── loader/
│   │   │   ├── elf.cpp         # ELF binary loader
│   │   │   └── dynamic.cpp     # Dynamic linker
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
- W^X (Write XOR Execute) enforcement
- ASLR for userspace programs

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
- User credentials (uid, gid, euid, egid)
- Process groups and sessions
- Signal handling with custom handlers

### Multi-User Support

- User database with password hashing
- Group membership
- Session management
- Capability-based permissions
- Root user (uid 0) with full privileges

### IPC Mechanisms

- **Pipes**: 4KB buffer, blocking/non-blocking modes
- **Shared Memory**: Up to 64 segments, key-based access
- **Message Queues**: Up to 256 messages per queue, typed messages
- **Signals**: 32 signals, custom handlers, signal masking

### SMP Support

- LAPIC initialization for each CPU
- IOAPIC for interrupt routing
- Inter-processor interrupts (IPI)
- Per-CPU data structures
- Spinlock synchronization

### Security Features

- Memory protection between processes
- W^X enforcement (no writable+executable pages)
- Capability system (CAP_SYS_ADMIN, CAP_NET_ADMIN, etc.)
- Discretionary access control (DAC)
- Mandatory access control (MAC) framework
- Security audit logging

### ELF Loader & Dynamic Linker

- Loads ELF64 executables
- Supports ET_EXEC and ET_DYN (PIE) binaries
- Maps PT_LOAD segments with proper permissions
- Dynamic linking with symbol resolution
- Supports DT_NEEDED, DT_RELA, DT_JMPREL relocations
- Lazy binding support

### Boot Process

1. UEFI firmware loads `BOOTX64.EFI` from EFI System Partition
2. Bootloader initializes GOP for graphics output
3. Retrieves UEFI memory map
4. Sets up minimal GDT
5. Jumps to kernel entry point with boot info
6. Kernel initializes GDT, IDT, PIC, PMM, VMM
7. Initializes process manager and scheduler
8. Sets up syscall infrastructure
9. Initializes IPC, user management, security
10. Probes for ACPI, SMP, USB, audio, GPU
11. Initializes ATA driver and mounts SertFS
12. Launches interactive shell

### Hardware Requirements

- x86_64 processor
- UEFI firmware (no legacy BIOS support)
- At least 64MB RAM
- IDE/ATA storage (for SertFS)

### Optional Hardware Support

- Multiple CPUs (SMP)
- USB controllers (UHCI/OHCI/EHCI/XHCI)
- HD Audio or AC97 sound cards
- Bochs VGA, VirtIO GPU, or VMware SVGA

## Userspace Example

```cpp
#include "libc/include/stdio.hpp"
#include "libc/include/unistd.hpp"

using namespace sertos::libc;

extern "C" void _start() {
    printf("Hello from userspace!\n");
    printf("My PID is: %d\n", getpid());
    printf("My UID is: %d\n", getuid());
    _exit(0);
}
```

## What's Next

- [ ] Network stack (TCP/IP)
- [ ] AHCI/NVMe support
- [ ] VFS layer
- [ ] More filesystem support (FAT32, ext2)
- [ ] GUI framework
- [ ] Port existing software

## License

Educational project. Do whatever you want with it.
