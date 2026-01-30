/* UEFI PE/COFF Linker Script for SertOS */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0;
    ImageBase = .;

    . = 0x200;
    
    .text ALIGN(0x1000) : {
        *(.text .text.*)
    }
    
    .rodata ALIGN(0x1000) : {
        *(.rodata .rodata.*)
    }
    
    .data ALIGN(0x1000) : {
        *(.data .data.*)
        *(.bss .bss.*)
        *(COMMON)
    }
    
    .reloc ALIGN(0x1000) : {
        *(.reloc)
    }
    
    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame*)
        *(.gnu.hash)
        *(.hash)
        *(.dynsym)
        *(.dynstr)
        *(.dynamic)
        *(.rela.*)
        *(.got*)
    }
}
