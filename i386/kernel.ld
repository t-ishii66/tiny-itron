/* kernel.ld -- Linker script for TinyItron/386 kernel                      */
/*                                                                          */
/* Two LOAD segments:                                                       */
/*   kernel -- all kernel code, rodata, data, BSS at VMA/LMA 0x3400        */
/*   user   -- user task code, placed right after kernel BSS (VMA=LMA)     */
/*                                                                          */
/* The ELF-to-raw-binary converter (elf.c) uses LMA (p_paddr) to place     */
/* segments in the flat binary.  boot.s loads the flat binary at 0x3400.    */
/* All pages use identity mapping (virtual = physical).                     */

OUTPUT_FORMAT("elf32-i386")
ENTRY(run)

PHDRS
{
    kernel PT_LOAD FLAGS(7);   /* RWX */
    user   PT_LOAD FLAGS(7);   /* RWX -- .user_text + .user_data */
}

SECTIONS
{
    . = 0x3400;
    .text     : { *(EXCLUDE_FILE(*libc.a) .text .text.*) } :kernel
    .rodata   : { *(.rodata .rodata.*) } :kernel
    .eh_frame : { *(.eh_frame) } :kernel

    . = ALIGN(32);
    .data     : { *(.data .data.*) } :kernel
    .bss      : { *(.bss) *(COMMON) } :kernel

    . = ALIGN(0x1000);               /* page-align for user code */

    .user_text : {
        _user_text_start = .;
        *(.user_text)
        *libc.a:*(.text .text.*)
        _user_text_end = .;
    } :user

    .user_data : {
        _user_data_start = .;
        *(.user_data)
        _user_data_end = .;
    } :user

    /DISCARD/ : { *(.comment) *(.note*) }
}
