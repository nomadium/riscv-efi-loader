/*
 * Linker script for RISC-V 64-bit EFI application
 * Creates a flat binary suitable for PE/COFF with embedded header
 */

OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_entry)

SECTIONS
{
    . = 0;

    /* PE/COFF header */
    .pe_header : {
        *(.pe_header)
    }

    /* Align to 64 bytes (FileAlignment) */
    . = ALIGN(0x40);

    /* Code section */
    _text = .;
    .text : {
        *(.text .text.*)
    }
    . = ALIGN(0x40);
    _etext = .;

    /* Data section */
    . = ALIGN(0x40);
    _data = .;
    .data : {
        *(.data .data.*)
        *(.rodata .rodata.*)
        *(.sdata .sdata.*)
    }
    .bss : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
    }
    . = ALIGN(0x40);
    _edata = .;

    _data_size = _edata - _data;

    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
        *(.rela.*)
        *(.rel.*)
        *(.dynsym)
        *(.dynstr)
        *(.dynamic)
        *(.hash)
        *(.gnu.hash)
        *(.riscv.attributes)
    }
}
