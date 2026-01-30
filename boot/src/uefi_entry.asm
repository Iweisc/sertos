; UEFI Entry Point for SertOS
; Creates a PE/COFF header and calls efi_main

BITS 64
DEFAULT REL

; External C++ function
extern efi_main

section .header progbits alloc noexec nowrite

global ImageBase
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
    dd pe_header - ImageBase    ; e_lfanew: PE header offset

; PE Header
pe_header:
    dd 0x00004550               ; PE signature "PE\0\0"

; COFF Header (20 bytes)
    dw 0x8664                   ; Machine: AMD64
    dw 3                        ; NumberOfSections
    dd 0                        ; TimeDateStamp
    dd 0                        ; PointerToSymbolTable
    dd 0                        ; NumberOfSymbols
    dw optional_header_end - optional_header ; SizeOfOptionalHeader
    dw 0x0022                   ; Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE

; Optional Header PE32+
optional_header:
    dw 0x020B                   ; Magic: PE32+
    db 0                        ; MajorLinkerVersion
    db 0                        ; MinorLinkerVersion
    dd 0                        ; SizeOfCode (filled by linker)
    dd 0                        ; SizeOfInitializedData (filled by linker)
    dd 0                        ; SizeOfUninitializedData
    dd _start - ImageBase       ; AddressOfEntryPoint
    dd 0x1000                   ; BaseOfCode

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
    dd 0                        ; SizeOfImage (filled by linker)
    dd header_end - ImageBase   ; SizeOfHeaders
    dd 0                        ; CheckSum
    dw 10                       ; Subsystem: EFI Application
    dw 0                        ; DllCharacteristics
    dq 0                        ; SizeOfStackReserve
    dq 0                        ; SizeOfStackCommit
    dq 0                        ; SizeOfHeapReserve
    dq 0                        ; SizeOfHeapCommit
    dd 0                        ; LoaderFlags
    dd 6                        ; NumberOfRvaAndSizes

; Data Directories
    dq 0                        ; Export Table
    dq 0                        ; Import Table
    dq 0                        ; Resource Table
    dq 0                        ; Exception Table
    dq 0                        ; Certificate Table
    dd 0                        ; Base Relocation Table RVA (filled by linker)
    dd 0                        ; Base Relocation Table Size

optional_header_end:

; Section Headers will be added by linker

; Padding to 0x200
    times 0x200 - ($ - ImageBase) db 0

header_end:

section .text progbits alloc exec nowrite

global _start
_start:
    ; UEFI entry point
    ; RCX = EFI_HANDLE ImageHandle
    ; RDX = EFI_SYSTEM_TABLE* SystemTable
    
    ; Save callee-saved registers (Windows x64 ABI)
    push rbx
    push rsi
    push rdi
    push rbp
    push r12
    push r13
    push r14
    push r15
    sub rsp, 0x28               ; Shadow space + alignment

    ; Call efi_main(ImageHandle, SystemTable)
    ; Arguments already in RCX, RDX (Windows x64 ABI)
    call efi_main

    ; Restore registers
    add rsp, 0x28
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret

section .data progbits alloc noexec write

section .reloc progbits alloc noexec nowrite
    ; Empty relocation block
    dd 0                        ; Page RVA
    dd 8                        ; Block size

section .note.GNU-stack noalloc noexec nowrite progbits
