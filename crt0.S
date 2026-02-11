/*
 * RISC-V 64-bit EFI application entry point and PE/COFF header
 *
 * EFI calling convention for RISC-V:
 * - Arguments in a0-a7
 * - Return value in a0
 * - Caller-saved: t0-t6, a0-a7
 * - Callee-saved: s0-s11, sp, ra
 *
 * PE/COFF layout for EFI:
 * - DOS Header (64 bytes, only MZ signature and PE offset matter)
 * - PE Header + Optional Header + Section Table
 * - Section data (aligned to FileAlignment)
 */

    .section .pe_header,"a"
    .balign 4

    /*
     * DOS Header (64 bytes)
     */
    .globl ImageBase
ImageBase:
dos_header:
    .short  0x5A4D                      /* e_magic: MZ signature */
    .short  0                           /* e_cblp */
    .short  0                           /* e_cp */
    .short  0                           /* e_crlc */
    .short  0                           /* e_cparhdr */
    .short  0                           /* e_minalloc */
    .short  0                           /* e_maxalloc */
    .short  0                           /* e_ss */
    .short  0                           /* e_sp */
    .short  0                           /* e_csum */
    .short  0                           /* e_ip */
    .short  0                           /* e_cs */
    .short  0                           /* e_lfarlc */
    .short  0                           /* e_ovno */
    .quad   0                           /* e_res[4] */
    .short  0                           /* e_oemid */
    .short  0                           /* e_oeminfo */
    .space  20                          /* e_res2[10] */
    .long   pe_header - ImageBase       /* e_lfanew: offset to PE header */

    /*
     * PE Header
     */
    .balign 4
pe_header:
    .long   0x00004550                  /* PE\0\0 signature */

    /* COFF File Header (20 bytes) */
coff_header:
    .short  0x5064                      /* Machine: RISC-V 64-bit (0x5064) */
    .short  2                           /* NumberOfSections */
    .long   0                           /* TimeDateStamp */
    .long   0                           /* PointerToSymbolTable */
    .long   0                           /* NumberOfSymbols */
    .short  optional_header_end - optional_header   /* SizeOfOptionalHeader */
    .short  0x002E                      /* Characteristics: same as EDK2 shell */

    /* Optional Header (PE32+ for 64-bit) */
optional_header:
    .short  0x20B                       /* Magic: PE32+ */
    .byte   0                           /* MajorLinkerVersion */
    .byte   0                           /* MinorLinkerVersion */
    .long   _etext - _text              /* SizeOfCode */
    .long   _data_size                  /* SizeOfInitializedData */
    .long   0                           /* SizeOfUninitializedData */
    .long   _entry - ImageBase          /* AddressOfEntryPoint */
    .long   _text - ImageBase           /* BaseOfCode */

    /* PE32+ specific fields */
    .quad   ImageBase                   /* ImageBase (will be 0, relocated) */
    .long   0x40                        /* SectionAlignment (64 bytes, same as shell) */
    .long   0x40                        /* FileAlignment (64 bytes, same as shell) */
    .short  0                           /* MajorOperatingSystemVersion */
    .short  0                           /* MinorOperatingSystemVersion */
    .short  0                           /* MajorImageVersion */
    .short  0                           /* MinorImageVersion */
    .short  0                           /* MajorSubsystemVersion */
    .short  0                           /* MinorSubsystemVersion */
    .long   0                           /* Win32VersionValue */
    .long   _edata - ImageBase          /* SizeOfImage */
    .long   _text - ImageBase           /* SizeOfHeaders */
    .long   0                           /* CheckSum */
    .short  10                          /* Subsystem: EFI Application */
    .short  0                           /* DllCharacteristics */
    .quad   0                           /* SizeOfStackReserve */
    .quad   0                           /* SizeOfStackCommit */
    .quad   0                           /* SizeOfHeapReserve */
    .quad   0                           /* SizeOfHeapCommit */
    .long   0                           /* LoaderFlags */
    .long   16                          /* NumberOfRvaAndSizes (standard is 16) */

    /* Data Directories (16 entries, all zero for simple EFI app) */
    .quad   0                           /* 0: Export Table */
    .quad   0                           /* 1: Import Table */
    .quad   0                           /* 2: Resource Table */
    .quad   0                           /* 3: Exception Table */
    .quad   0                           /* 4: Certificate Table */
    .quad   0                           /* 5: BaseRelocation Table */
    .quad   0                           /* 6: Debug */
    .quad   0                           /* 7: Architecture */
    .quad   0                           /* 8: Global Ptr */
    .quad   0                           /* 9: TLS Table */
    .quad   0                           /* 10: Load Config Table */
    .quad   0                           /* 11: Bound Import */
    .quad   0                           /* 12: IAT */
    .quad   0                           /* 13: Delay Import Descriptor */
    .quad   0                           /* 14: CLR Runtime Header */
    .quad   0                           /* 15: Reserved */

optional_header_end:

    /* Section Headers */
    /* .text section */
section_text:
    .ascii  ".text\0\0\0"              /* Name (8 bytes) */
    .long   _etext - _text              /* VirtualSize */
    .long   _text - ImageBase           /* VirtualAddress */
    .long   _etext - _text              /* SizeOfRawData */
    .long   _text - ImageBase           /* PointerToRawData */
    .long   0                           /* PointerToRelocations */
    .long   0                           /* PointerToLinenumbers */
    .short  0                           /* NumberOfRelocations */
    .short  0                           /* NumberOfLinenumbers */
    .long   0x60000020                  /* Characteristics: CODE | EXECUTE | READ */

    /* .data section */
section_data:
    .ascii  ".data\0\0\0"              /* Name (8 bytes) */
    .long   _data_size                  /* VirtualSize */
    .long   _data - ImageBase           /* VirtualAddress */
    .long   _data_size                  /* SizeOfRawData */
    .long   _data - ImageBase           /* PointerToRawData */
    .long   0                           /* PointerToRelocations */
    .long   0                           /* PointerToLinenumbers */
    .short  0                           /* NumberOfRelocations */
    .short  0                           /* NumberOfLinenumbers */
    .long   0xC0000040                  /* Characteristics: DATA | READ | WRITE */

    /*
     * Code section
     */
    .section .text,"ax"
    .balign 4
    .globl _entry
    .type _entry, @function
_entry:
    /* Set up stack frame */
    addi    sp, sp, -32
    sd      ra, 24(sp)
    sd      s0, 16(sp)
    addi    s0, sp, 32

    /* Save arguments to globals for later use */
    lla     t0, _image_handle
    sd      a0, 0(t0)
    lla     t0, _system_table
    sd      a1, 0(t0)

    /* Call efi_main(ImageHandle, SystemTable) */
    call    efi_main

    /* Return to EFI firmware */
    ld      ra, 24(sp)
    ld      s0, 16(sp)
    addi    sp, sp, 32
    ret

    /*
     * Data section
     */
    .section .data,"aw"
    .balign 8
    .globl _image_handle
_image_handle:
    .quad   0

    .globl _system_table
_system_table:
    .quad   0
