OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x100000;

    .text : ALIGN(4K)
    {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    } :rodata

    .data : ALIGN(4K)
    {
        *(.data .data.*)
        
        __init_array_start = .;
        KEEP(*(.init_array))
        KEEP(*(.init_array.*))
        __init_array_end = .;
    } :data

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}
