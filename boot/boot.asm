[org 0x7c00]
[bits 16]

KERNEL_OFFSET equ 0x1000
KERNEL_SECTORS equ 32

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    
    mov [BOOT_DRIVE], dl
    
    mov si, MSG_REAL_MODE
    call print_string_rm
    
    call load_kernel
    
    mov si, MSG_LOAD_DONE
    call print_string_rm
    
    call switch_to_pm
    
    jmp $

print_string_rm:
    pusha
    mov ah, 0x0e
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

load_kernel:
    mov si, MSG_LOAD_KERNEL
    call print_string_rm
    
    mov ax, 0
    mov es, ax
    mov bx, KERNEL_OFFSET
    
    mov ah, 0x02
    mov al, KERNEL_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [BOOT_DRIVE]
    
    int 0x13
    jc disk_error
    
    ret

disk_error:
    mov si, DISK_ERROR
    call print_string_rm
    mov ah, 0x00
    mov dl, [BOOT_DRIVE]
    int 0x13
    jmp load_kernel

[bits 32]
BEGIN_PM:
    mov ebx, MSG_PROT_MODE
    call print_string_pm
    
    call KERNEL_OFFSET
    
    jmp $

VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

print_string_pm:
    pusha
    mov edx, VIDEO_MEMORY
.loop:
    mov al, [ebx]
    mov ah, WHITE_ON_BLACK
    cmp al, 0
    je .done
    mov [edx], ax
    add ebx, 1
    add edx, 2
    jmp .loop
.done:
    popa
    ret

%include "boot/gdt.asm"
%include "boot/switch_pm.asm"

BOOT_DRIVE db 0
MSG_REAL_MODE db "SertOS Bootloader", 13, 10, 0
MSG_PROT_MODE db "Protected Mode OK", 0
MSG_LOAD_KERNEL db "Loading kernel...", 13, 10, 0
MSG_LOAD_DONE db "Done!", 13, 10, 0
DISK_ERROR db "Disk error, retrying...", 13, 10, 0

times 510-($-$$) db 0
dw 0xaa55
