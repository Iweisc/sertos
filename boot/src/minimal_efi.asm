; SertOS UEFI Bootloader
; A complete UEFI bootloader that initializes graphics, memory, and kernel

BITS 64

org 0

ImageBase:
; DOS Header (64 bytes)
    dw 0x5A4D                   ; e_magic: MZ
    dw 0                        ; e_cblp
    dw 0                        ; e_cp
    dw 0                        ; e_crlc
    dw 0                        ; e_cparhdr
    dw 0                        ; e_minalloc
    dw 0                        ; e_maxalloc
    dw 0                        ; e_ss
    dw 0                        ; e_sp
    dw 0                        ; e_csum
    dw 0                        ; e_ip
    dw 0                        ; e_cs
    dw 0                        ; e_lfarlc
    dw 0                        ; e_ovno
    times 4 dw 0                ; e_res
    dw 0                        ; e_oemid
    dw 0                        ; e_oeminfo
    times 10 dw 0               ; e_res2
    dd pe_header                ; e_lfanew: PE header offset

; PE Header at offset 0x40
pe_header:
    dd 0x00004550               ; PE signature "PE\0\0"

; COFF Header (20 bytes)
coff_header:
    dw 0x8664                   ; Machine: AMD64
    dw 2                        ; NumberOfSections
    dd 0                        ; TimeDateStamp
    dd 0                        ; PointerToSymbolTable
    dd 0                        ; NumberOfSymbols
    dw optional_header_end - optional_header ; SizeOfOptionalHeader
    dw 0x0022                   ; Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE

; Optional Header PE32+ (112 bytes + data directories)
optional_header:
    dw 0x020B                   ; Magic: PE32+
    db 0                        ; MajorLinkerVersion
    db 0                        ; MinorLinkerVersion
    dd code_size                ; SizeOfCode
    dd data_size                ; SizeOfInitializedData
    dd 0                        ; SizeOfUninitializedData
    dd entry_point              ; AddressOfEntryPoint
    dd code_section             ; BaseOfCode

; PE32+ specific fields
    dq 0                        ; ImageBase (relocated by loader)
    dd 0x1000                   ; SectionAlignment
    dd 0x200                    ; FileAlignment
    dw 0                        ; MajorOperatingSystemVersion
    dw 0                        ; MinorOperatingSystemVersion
    dw 0                        ; MajorImageVersion
    dw 0                        ; MinorImageVersion
    dw 0                        ; MajorSubsystemVersion
    dw 0                        ; MinorSubsystemVersion
    dd 0                        ; Win32VersionValue
    dd image_size               ; SizeOfImage
    dd headers_size             ; SizeOfHeaders
    dd 0                        ; CheckSum
    dw 10                       ; Subsystem: EFI Application
    dw 0                        ; DllCharacteristics
    dq 0                        ; SizeOfStackReserve
    dq 0                        ; SizeOfStackCommit
    dq 0                        ; SizeOfHeapReserve
    dq 0                        ; SizeOfHeapCommit
    dd 0                        ; LoaderFlags
    dd 6                        ; NumberOfRvaAndSizes

; Data Directories (6 entries * 8 bytes = 48 bytes)
    dq 0                        ; Export Table
    dq 0                        ; Import Table
    dq 0                        ; Resource Table
    dq 0                        ; Exception Table
    dq 0                        ; Certificate Table
    dd reloc_section            ; Base Relocation Table RVA
    dd reloc_size               ; Base Relocation Table Size

optional_header_end:

; Section Headers
; .text section header
section_text_header:
    db ".text", 0, 0, 0         ; Name (8 bytes)
    dd code_vsize               ; VirtualSize
    dd code_section             ; VirtualAddress
    dd code_rawsize             ; SizeOfRawData
    dd code_fileoff             ; PointerToRawData
    dd 0                        ; PointerToRelocations
    dd 0                        ; PointerToLinenumbers
    dw 0                        ; NumberOfRelocations
    dw 0                        ; NumberOfLinenumbers
    dd 0x60000020               ; Characteristics: CODE | EXECUTE | READ

; .reloc section header
section_reloc_header:
    db ".reloc", 0, 0           ; Name (8 bytes)
    dd reloc_vsize              ; VirtualSize
    dd reloc_section            ; VirtualAddress
    dd reloc_rawsize            ; SizeOfRawData
    dd reloc_fileoff            ; PointerToRawData
    dd 0                        ; PointerToRelocations
    dd 0                        ; PointerToLinenumbers
    dw 0                        ; NumberOfRelocations
    dw 0                        ; NumberOfLinenumbers
    dd 0x42000040               ; Characteristics: INITIALIZED_DATA | DISCARDABLE | READ

; Padding to FileAlignment (0x200)
    times 0x200 - ($ - ImageBase) db 0

headers_size equ $ - ImageBase

; Code Section (at file offset 0x200, VMA 0x1000)
code_fileoff equ $ - ImageBase
code_section equ 0x1000

entry_point equ code_section

; ============================================================================
; Entry point code
; RCX = EFI_HANDLE ImageHandle
; RDX = EFI_SYSTEM_TABLE* SystemTable
; ============================================================================
start:
    push rbx
    push rsi
    push rdi
    push rbp
    push r12
    push r13
    push r14
    push r15
    sub rsp, 0x28

    mov [rel image_handle], rcx
    mov [rel system_table], rdx
    mov rbx, rdx

    ; Clear screen
    mov rax, [rbx + 0x40]
    mov rcx, rax
    call [rax + 0x30]

    ; Print banner
    lea rdx, [rel msg_banner1]
    call print_string
    lea rdx, [rel msg_banner2]
    call print_string
    lea rdx, [rel msg_banner3]
    call print_string
    lea rdx, [rel msg_newline]
    call print_string

    ; Disable watchdog timer
    lea rdx, [rel msg_watchdog]
    call print_string
    mov rax, [rbx + 0x60]
    xor r9, r9
    xor r8, r8
    xor rdx, rdx
    xor rcx, rcx
    call [rax + 0x100]
    lea rdx, [rel msg_done]
    call print_string

    ; Get Graphics Output Protocol
    lea rdx, [rel msg_gop]
    call print_string
    call init_graphics
    test rax, rax
    jnz .gop_ok
    lea rdx, [rel msg_fail]
    call print_string
    jmp .gop_done
.gop_ok:
    lea rdx, [rel msg_done]
    call print_string
    
    ; Print resolution
    lea rdx, [rel msg_resolution]
    call print_string
    mov eax, [rel gop_width]
    call print_decimal
    lea rdx, [rel msg_x]
    call print_string
    mov eax, [rel gop_height]
    call print_decimal
    lea rdx, [rel msg_newline]
    call print_string
.gop_done:

    ; Get memory map
    lea rdx, [rel msg_memmap]
    call print_string
    call get_memory_map
    test rax, rax
    jnz .memmap_ok
    lea rdx, [rel msg_fail]
    call print_string
    jmp .memmap_done
.memmap_ok:
    lea rdx, [rel msg_done]
    call print_string
    
    ; Print memory info
    lea rdx, [rel msg_memsize]
    call print_string
    mov rax, [rel total_memory]
    shr rax, 20
    call print_decimal
    lea rdx, [rel msg_mb]
    call print_string
.memmap_done:

    ; Initialize kernel structures
    lea rdx, [rel msg_kernel_init]
    call print_string
    call init_kernel
    lea rdx, [rel msg_done]
    call print_string

    ; Print boot complete message
    lea rdx, [rel msg_newline]
    call print_string
    lea rdx, [rel msg_complete]
    call print_string
    lea rdx, [rel msg_newline]
    call print_string

    ; Draw something on framebuffer if we have GOP
    mov rax, [rel framebuffer_addr]
    test rax, rax
    jz .no_draw
    call draw_test_pattern
.no_draw:

    ; Enter kernel main loop
    lea rdx, [rel msg_entering]
    call print_string
    call kernel_main

    ; Should never reach here
    lea rdx, [rel msg_halt]
    call print_string
.halt:
    hlt
    jmp .halt

; ============================================================================
; print_string - Print a UTF-16 string
; Input: RDX = pointer to string
; ============================================================================
print_string:
    push rbx
    push rcx
    push rax
    sub rsp, 0x20
    mov rbx, [rel system_table]
    mov rax, [rbx + 0x40]
    mov rcx, rax
    call [rax + 0x08]
    add rsp, 0x20
    pop rax
    pop rcx
    pop rbx
    ret

; ============================================================================
; print_decimal - Print a decimal number
; Input: EAX = number to print
; ============================================================================
print_decimal:
    push rbx
    push rcx
    push rdx
    push rdi
    sub rsp, 0x40

    lea rdi, [rsp + 0x20]
    mov ecx, 10
    xor ebx, ebx

.convert_loop:
    xor edx, edx
    div ecx
    add dl, '0'
    mov [rdi + rbx * 2], dl
    mov byte [rdi + rbx * 2 + 1], 0
    inc ebx
    test eax, eax
    jnz .convert_loop

    ; Reverse the string
    lea rdi, [rel decimal_buffer]
    dec ebx
.reverse_loop:
    mov al, [rsp + 0x20 + rbx * 2]
    mov [rdi], al
    mov byte [rdi + 1], 0
    add rdi, 2
    dec ebx
    jns .reverse_loop
    mov word [rdi], 0

    lea rdx, [rel decimal_buffer]
    call print_string

    add rsp, 0x40
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

; ============================================================================
; init_graphics - Initialize Graphics Output Protocol
; Output: RAX = 1 on success, 0 on failure
; ============================================================================
init_graphics:
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    sub rsp, 0x40

    mov rbx, [rel system_table]
    mov rax, [rbx + 0x60]
    
    ; LocateProtocol(GOP_GUID, NULL, &gop)
    lea rcx, [rel gop_guid]
    xor rdx, rdx
    lea r8, [rel gop_protocol]
    call [rax + 0x140]
    
    test rax, rax
    jnz .fail

    ; Get current mode info
    mov rax, [rel gop_protocol]
    mov rcx, [rax + 0x18]
    mov [rel gop_mode], rcx
    
    mov rdx, [rcx + 0x08]
    mov [rel gop_mode_info], rdx
    
    ; Get resolution
    mov eax, [rdx + 0x04]
    mov [rel gop_width], eax
    mov eax, [rdx + 0x08]
    mov [rel gop_height], eax
    mov eax, [rdx + 0x0C]
    mov [rel gop_pixels_per_scanline], eax
    
    ; Get framebuffer
    mov rax, [rel gop_mode]
    mov rdx, [rax + 0x18]
    mov [rel framebuffer_addr], rdx
    mov rdx, [rax + 0x20]
    mov [rel framebuffer_size], rdx

    mov rax, 1
    jmp .done

.fail:
    xor rax, rax
.done:
    add rsp, 0x40
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    ret

; ============================================================================
; get_memory_map - Get UEFI memory map
; Output: RAX = 1 on success, 0 on failure
; ============================================================================
get_memory_map:
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    sub rsp, 0x40

    mov rbx, [rel system_table]
    mov rax, [rbx + 0x60]

    ; GetMemoryMap(&map_size, buffer, &map_key, &desc_size, &desc_version)
    lea rcx, [rel memmap_size]
    mov qword [rcx], 0x4000
    lea rdx, [rel memmap_buffer]
    lea r8, [rel memmap_key]
    lea r9, [rel memmap_desc_size]
    lea rax, [rel memmap_desc_version]
    mov [rsp + 0x20], rax
    mov rax, [rbx + 0x60]
    call [rax + 0x38]

    test rax, rax
    jnz .fail

    ; Calculate total memory
    xor r8, r8
    mov rcx, [rel memmap_size]
    mov rdx, [rel memmap_desc_size]
    lea rsi, [rel memmap_buffer]

.count_loop:
    cmp rcx, rdx
    jb .count_done
    
    ; Check if this is usable memory (type 7 = EfiConventionalMemory)
    mov eax, [rsi]
    cmp eax, 7
    jne .next_entry
    
    ; Add pages * 4096 to total
    mov rax, [rsi + 0x18]
    shl rax, 12
    add r8, rax

.next_entry:
    add rsi, rdx
    sub rcx, rdx
    jmp .count_loop

.count_done:
    mov [rel total_memory], r8
    mov rax, 1
    jmp .done

.fail:
    xor rax, rax
.done:
    add rsp, 0x40
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    ret

; ============================================================================
; init_kernel - Initialize kernel structures
; ============================================================================
init_kernel:
    push rbx
    push rcx
    sub rsp, 0x20

    ; Set up GDT
    lgdt [rel gdt_ptr]

    ; Set up IDT (minimal - just load empty IDT for now)
    lidt [rel idt_ptr]

    add rsp, 0x20
    pop rcx
    pop rbx
    ret

; ============================================================================
; draw_test_pattern - Draw a test pattern on the framebuffer
; ============================================================================
draw_test_pattern:
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi

    mov rdi, [rel framebuffer_addr]
    mov ecx, [rel gop_width]
    mov edx, [rel gop_height]
    mov esi, [rel gop_pixels_per_scanline]

    ; Draw a blue gradient bar at the top
    xor ebx, ebx
.draw_row:
    cmp ebx, 50
    jge .draw_done
    
    push rbx
    push rcx
    
    imul eax, ebx, 4
    imul eax, esi
    add rdi, rax
    
    xor ecx, ecx
.draw_pixel:
    cmp ecx, [rel gop_width]
    jge .next_row
    
    ; Blue gradient: 0x00RRGGBB
    mov eax, ecx
    and eax, 0xFF
    shl eax, 16
    or eax, 0x000080FF
    mov [rdi + rcx * 4], eax
    
    inc ecx
    jmp .draw_pixel

.next_row:
    pop rcx
    pop rbx
    mov rdi, [rel framebuffer_addr]
    inc ebx
    jmp .draw_row

.draw_done:
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

; ============================================================================
; kernel_main - Main kernel loop
; ============================================================================
kernel_main:
    push rbx
    sub rsp, 0x20

    ; Simple kernel loop - just wait for interrupts
    mov ecx, 0
.loop:
    hlt
    inc ecx
    cmp ecx, 100
    jl .loop

    add rsp, 0x20
    pop rbx
    ret

; ============================================================================
; Data Section
; ============================================================================

; GDT
align 16
gdt_start:
    dq 0                        ; Null descriptor
gdt_code:
    dw 0xFFFF                   ; Limit low
    dw 0                        ; Base low
    db 0                        ; Base middle
    db 0x9A                     ; Access: present, ring 0, code, readable
    db 0xAF                     ; Flags: 64-bit, limit high
    db 0                        ; Base high
gdt_data:
    dw 0xFFFF                   ; Limit low
    dw 0                        ; Base low
    db 0                        ; Base middle
    db 0x92                     ; Access: present, ring 0, data, writable
    db 0xCF                     ; Flags: 32-bit, limit high
    db 0                        ; Base high
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dq gdt_start

; IDT (empty for now)
align 16
idt_start:
    times 256 dq 0, 0           ; 256 entries, 16 bytes each
idt_end:

idt_ptr:
    dw idt_end - idt_start - 1
    dq idt_start

; GOP GUID: 9042a9de-23dc-4a38-96fb-7aded080516a
align 8
gop_guid:
    dd 0x9042a9de
    dw 0x23dc
    dw 0x4a38
    db 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a

; Variables
align 8
image_handle:       dq 0
system_table:       dq 0
gop_protocol:       dq 0
gop_mode:           dq 0
gop_mode_info:      dq 0
gop_width:          dd 0
gop_height:         dd 0
gop_pixels_per_scanline: dd 0
framebuffer_addr:   dq 0
framebuffer_size:   dq 0
total_memory:       dq 0
memmap_size:        dq 0x4000
memmap_key:         dq 0
memmap_desc_size:   dq 0
memmap_desc_version: dq 0

; Decimal conversion buffer
decimal_buffer:     times 24 dw 0

; Message strings (UTF-16LE)
msg_banner1:
    dw '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '='
    dw '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '='
    dw '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '='
    dw 0x0D, 0x0A, 0

msg_banner2:
    dw ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
    dw 'S', 'e', 'r', 't', 'O', 'S', ' ', 'v', '1', '.', '0'
    dw 0x0D, 0x0A, 0

msg_banner3:
    dw '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '='
    dw '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '='
    dw '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '='
    dw 0x0D, 0x0A, 0

msg_newline:
    dw 0x0D, 0x0A, 0

msg_watchdog:
    dw '[', '.', '.', '.', '.', ']', ' '
    dw 'D', 'i', 's', 'a', 'b', 'l', 'i', 'n', 'g', ' '
    dw 'w', 'a', 't', 'c', 'h', 'd', 'o', 'g', ' '
    dw 't', 'i', 'm', 'e', 'r'
    dw 0x0D, 0x0A, 0

msg_gop:
    dw '[', '.', '.', '.', '.', ']', ' '
    dw 'I', 'n', 'i', 't', 'i', 'a', 'l', 'i', 'z', 'i', 'n', 'g', ' '
    dw 'g', 'r', 'a', 'p', 'h', 'i', 'c', 's'
    dw 0x0D, 0x0A, 0

msg_memmap:
    dw '[', '.', '.', '.', '.', ']', ' '
    dw 'G', 'e', 't', 't', 'i', 'n', 'g', ' '
    dw 'm', 'e', 'm', 'o', 'r', 'y', ' '
    dw 'm', 'a', 'p'
    dw 0x0D, 0x0A, 0

msg_kernel_init:
    dw '[', '.', '.', '.', '.', ']', ' '
    dw 'I', 'n', 'i', 't', 'i', 'a', 'l', 'i', 'z', 'i', 'n', 'g', ' '
    dw 'k', 'e', 'r', 'n', 'e', 'l'
    dw 0x0D, 0x0A, 0

msg_done:
    dw ' ', ' ', ' ', ' ', ' ', ' ', ' '
    dw '[', ' ', 'O', 'K', ' ', ']'
    dw 0x0D, 0x0A, 0

msg_fail:
    dw ' ', ' ', ' ', ' ', ' ', ' ', ' '
    dw '[', 'F', 'A', 'I', 'L', ']'
    dw 0x0D, 0x0A, 0

msg_resolution:
    dw ' ', ' ', ' ', ' ', ' ', ' ', ' '
    dw 'R', 'e', 's', 'o', 'l', 'u', 't', 'i', 'o', 'n', ':', ' ', 0

msg_x:
    dw 'x', 0

msg_memsize:
    dw ' ', ' ', ' ', ' ', ' ', ' ', ' '
    dw 'T', 'o', 't', 'a', 'l', ' ', 'R', 'A', 'M', ':', ' ', 0

msg_mb:
    dw ' ', 'M', 'B', 0x0D, 0x0A, 0

msg_complete:
    dw 'B', 'o', 'o', 't', ' ', 'c', 'o', 'm', 'p', 'l', 'e', 't', 'e', '!'
    dw 0x0D, 0x0A, 0

msg_entering:
    dw 'E', 'n', 't', 'e', 'r', 'i', 'n', 'g', ' '
    dw 'k', 'e', 'r', 'n', 'e', 'l', ' '
    dw 'm', 'a', 'i', 'n', ' '
    dw 'l', 'o', 'o', 'p', '.', '.', '.', 0x0D, 0x0A, 0

msg_halt:
    dw 0x0D, 0x0A
    dw 'S', 'y', 's', 't', 'e', 'm', ' '
    dw 'h', 'a', 'l', 't', 'e', 'd', '.'
    dw 0x0D, 0x0A, 0

; Memory map buffer (16KB)
align 16
memmap_buffer:
    times 0x4000 db 0

; Padding to FileAlignment
    times 0x200 - (($ - ImageBase - code_fileoff) % 0x200) db 0

code_rawsize equ $ - ImageBase - code_fileoff
code_vsize equ code_rawsize
code_size equ code_rawsize

; Relocation Section (at next file offset, VMA 0x8000)
reloc_fileoff equ $ - ImageBase
reloc_section equ 0x8000

; Empty relocation table
reloc_block:
    dd 0
    dd 8

; Padding to FileAlignment
    times 0x200 - (($ - ImageBase - reloc_fileoff) % 0x200) db 0

reloc_rawsize equ $ - ImageBase - reloc_fileoff
reloc_vsize equ reloc_rawsize
reloc_size equ 8

data_size equ reloc_rawsize

; Image size (aligned to SectionAlignment)
image_size equ 0x10000
