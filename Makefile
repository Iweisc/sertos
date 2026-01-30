CC = gcc
LD = ld
ASM = nasm
QEMU = qemu-system-i386

CFLAGS = -ffreestanding -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
         -nostartfiles -nodefaultlibs -Wall -Wextra -c -Ikernel/include \
         -fno-pie -fno-pic
LDFLAGS = -T kernel/linker.ld -nostdlib -m elf_i386
ASMFLAGS = -f elf32

BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = kernel

.PHONY: all clean run debug dirs

all: dirs $(BUILD_DIR)/os-image.bin

dirs:
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.bin: $(BOOT_DIR)/boot.asm $(BOOT_DIR)/gdt.asm $(BOOT_DIR)/switch_pm.asm
	$(ASM) -f bin $(BOOT_DIR)/boot.asm -o $@

$(BUILD_DIR)/kernel_entry.o: $(KERNEL_DIR)/kernel_entry.asm
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/isr.o: $(KERNEL_DIR)/cpu/isr.asm
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/kernel.o: $(KERNEL_DIR)/kernel.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/vga.o: $(KERNEL_DIR)/drivers/vga.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: $(KERNEL_DIR)/drivers/keyboard.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: $(KERNEL_DIR)/drivers/timer.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/idt.o: $(KERNEL_DIR)/cpu/idt.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/heap.o: $(KERNEL_DIR)/mm/heap.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: $(KERNEL_DIR)/lib/string.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/gui.o: $(KERNEL_DIR)/gui/gui.c
	$(CC) $(CFLAGS) $< -o $@

KERNEL_OBJS = $(BUILD_DIR)/kernel_entry.o \
              $(BUILD_DIR)/isr.o \
              $(BUILD_DIR)/kernel.o \
              $(BUILD_DIR)/vga.o \
              $(BUILD_DIR)/keyboard.o \
              $(BUILD_DIR)/timer.o \
              $(BUILD_DIR)/idt.o \
              $(BUILD_DIR)/heap.o \
              $(BUILD_DIR)/string.o \
              $(BUILD_DIR)/gui.o

$(BUILD_DIR)/kernel.bin: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/os-image.bin: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin
	cat $^ > $@
	@truncate -s 65536 $@

run: all
	$(QEMU) -drive format=raw,file=$(BUILD_DIR)/os-image.bin

debug: all
	$(QEMU) -drive format=raw,file=$(BUILD_DIR)/os-image.bin -s -S &
	gdb -ex "target remote localhost:1234" -ex "symbol-file $(BUILD_DIR)/kernel.bin"

clean:
	rm -rf $(BUILD_DIR)

install-deps:
	@echo "Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y nasm qemu-system-x86 gcc-multilib build-essential
