/* UEFI PE/COFF Linker Script */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 0;
    
    ImageBase = .;
    
    .hash : { *(.hash) }
    
    . = ALIGN(4096);
    .text : {
        *(.text .text.*)
    }
    
    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
    }
    
    . = ALIGN(4096);
    .data : {
        *(.data .data.*)
        *(.got .got.*)
    }
    
    . = ALIGN(4096);
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }
    
    . = ALIGN(4096);
    .reloc : {
        *(.reloc)
    }
    
    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame*)
        *(.gnu.hash)
        *(.dynsym)
        *(.dynstr)
        *(.dynamic)
        *(.rela.*)
    }
}
