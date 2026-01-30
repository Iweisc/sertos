# SertOS Makefile
# UEFI-based Operating System

# Toolchain
AS := nasm

# Directories
BUILD_DIR := build
BOOT_DIR := boot
ISO_DIR := $(BUILD_DIR)/iso

# Output files
BOOTLOADER_EFI := $(BUILD_DIR)/BOOTX64.EFI
OS_IMG := $(BUILD_DIR)/sertos.img

.PHONY: all clean run debug

all: $(OS_IMG)

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build UEFI bootloader from assembly
$(BOOTLOADER_EFI): $(BOOT_DIR)/src/minimal_efi.asm | $(BUILD_DIR)
	$(AS) -f bin $< -o $@

# Create UEFI bootable image
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

# Run in QEMU
run: $(OS_IMG)
	qemu-system-x86_64 \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive file=$(OS_IMG),format=raw \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown

# Run with debugging
debug: $(OS_IMG)
	qemu-system-x86_64 \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive file=$(OS_IMG),format=raw \
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
