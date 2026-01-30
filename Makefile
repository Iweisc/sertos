# SertOS Makefile
# UEFI-based Operating System with C++ Kernel

# Toolchain - using mingw for native PE/COFF output
CXX := x86_64-w64-mingw32-g++
AS := nasm
LD := x86_64-w64-mingw32-ld
OBJCOPY := objcopy

# Flags for UEFI
CXXFLAGS := -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector \
            -fshort-wchar -mno-red-zone -mno-stack-arg-probe \
            -Wall -Wextra -O2 -std=c++17 \
            -I kernel/include -I boot/include \
            -D__KERNEL__ -DEFI_FUNCTION_WRAPPER

ASFLAGS := -f win64

LDFLAGS := -nostdlib -Wl,--subsystem,10 -Wl,--entry,efi_main \
           -Wl,--image-base,0x100000 -e efi_main

# Directories
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BOOT_DIR := boot
KERNEL_DIR := kernel
ISO_DIR := $(BUILD_DIR)/iso

# Output files
BOOTLOADER_EFI := $(BUILD_DIR)/BOOTX64.EFI
OS_IMG := $(BUILD_DIR)/sertos.img

# Source files
BOOT_CXX_SRCS := $(BOOT_DIR)/src/main.cpp
KERNEL_CXX_SRCS := $(shell find $(KERNEL_DIR)/src -name '*.cpp')
KERNEL_ASM_SRCS := $(shell find $(KERNEL_DIR)/src -name '*.asm')

# Object files
BOOT_CXX_OBJS := $(patsubst $(BOOT_DIR)/src/%.cpp,$(OBJ_DIR)/boot/%.o,$(BOOT_CXX_SRCS))
KERNEL_CXX_OBJS := $(patsubst $(KERNEL_DIR)/src/%.cpp,$(OBJ_DIR)/kernel/%.o,$(KERNEL_CXX_SRCS))
KERNEL_ASM_OBJS := $(patsubst $(KERNEL_DIR)/src/%.asm,$(OBJ_DIR)/kernel/%.o,$(KERNEL_ASM_SRCS))
ALL_OBJS := $(BOOT_CXX_OBJS) $(KERNEL_CXX_OBJS) $(KERNEL_ASM_OBJS)

.PHONY: all clean run debug kernel bootloader asm-boot

all: $(OS_IMG)

# Use assembly bootloader (simpler, works standalone)
asm-boot: $(BUILD_DIR)/sertos-asm.img

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile boot C++ sources
$(OBJ_DIR)/boot/%.o: $(BOOT_DIR)/src/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile C++ kernel sources
$(OBJ_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile assembly kernel sources (win64 format for mingw)
$(OBJ_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.asm | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Link everything into PE/COFF EFI executable
$(BOOTLOADER_EFI): $(ALL_OBJS) | $(BUILD_DIR)
	$(CXX) $(LDFLAGS) -o $@ $(ALL_OBJS)

# Build assembly-only bootloader (fallback)
$(BUILD_DIR)/BOOTX64-asm.EFI: $(BOOT_DIR)/src/minimal_efi.asm | $(BUILD_DIR)
	nasm -f bin $< -o $@

# Create UEFI bootable image with C++ kernel
$(OS_IMG): $(BOOTLOADER_EFI)
	@mkdir -p $(ISO_DIR)/EFI/BOOT
	cp $(BOOTLOADER_EFI) $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	mkfs.fat -F 32 $@ >/dev/null
	mkdir -p $(BUILD_DIR)/mnt
	sudo mount -o loop $@ $(BUILD_DIR)/mnt
	sudo mkdir -p $(BUILD_DIR)/mnt/EFI/BOOT
	sudo cp $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI $(BUILD_DIR)/mnt/EFI/BOOT/
	sudo umount $(BUILD_DIR)/mnt
	rmdir $(BUILD_DIR)/mnt
	@echo "Build complete: $(OS_IMG)"

# Create UEFI bootable image with assembly bootloader
$(BUILD_DIR)/sertos-asm.img: $(BUILD_DIR)/BOOTX64-asm.EFI
	@mkdir -p $(ISO_DIR)/EFI/BOOT
	cp $< $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	mkfs.fat -F 32 $@ >/dev/null
	mkdir -p $(BUILD_DIR)/mnt
	sudo mount -o loop $@ $(BUILD_DIR)/mnt
	sudo mkdir -p $(BUILD_DIR)/mnt/EFI/BOOT
	sudo cp $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI $(BUILD_DIR)/mnt/EFI/BOOT/
	sudo umount $(BUILD_DIR)/mnt
	rmdir $(BUILD_DIR)/mnt
	@echo "Build complete: $@"

# Create persistent storage disk
$(BUILD_DIR)/storage.img:
	dd if=/dev/zero of=$@ bs=1M count=128 2>/dev/null
	@echo "Created storage disk: $@"

# Run in QEMU with persistent storage
run: $(OS_IMG) $(BUILD_DIR)/storage.img
	qemu-system-x86_64 \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive file=$(OS_IMG),format=raw,if=ide \
		-drive file=$(BUILD_DIR)/storage.img,format=raw,if=ide \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown

# Run assembly bootloader version
run-asm: $(BUILD_DIR)/sertos-asm.img
	qemu-system-x86_64 \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive file=$<,format=raw \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown

# Run with debugging
debug: $(OS_IMG) $(BUILD_DIR)/storage.img
	qemu-system-x86_64 \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive file=$(OS_IMG),format=raw,if=ide \
		-drive file=$(BUILD_DIR)/storage.img,format=raw,if=ide \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown \
		-s -S

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Print variables for debugging
print-%:
	@echo $* = $($*)

# Show all source files
sources:
	@echo "Boot Sources:"
	@echo $(BOOT_CXX_SRCS) | tr ' ' '\n'
	@echo ""
	@echo "Kernel C++ Sources:"
	@echo $(KERNEL_CXX_SRCS) | tr ' ' '\n'
	@echo ""
	@echo "Kernel ASM Sources:"
	@echo $(KERNEL_ASM_SRCS) | tr ' ' '\n'
