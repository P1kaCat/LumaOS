/* userprog.ld — Linker script for standalone LumaOS user programs (ELF64)
 *
 * Produces a proper ELF64 executable with two PT_LOAD segments:
 *   - text segment (PF_R | PF_X): .text + .rodata
 *   - data segment (PF_R | PF_W): .data + .bss
 *
 * The ELF64 loader (spawn_file) parses program headers, maps each
 * PT_LOAD segment at its p_vaddr with the correct permissions,
 * copies p_filesz from the file, and zero-fills p_memsz - p_filesz.
 *
 * Entry point is _start; RIP is set to e_entry from the ELF header.
 */
ENTRY(_start)

SECTIONS {
    . = 0x800000;

    /* ----- Text segment: code + read-only data (R+X) ----- */
    .text : ALIGN(4096) {
        *(.text)
        *(.text.*)
    } :text

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :text

    /* ----- Data segment: writable data + bss (R+W) ----- */
    . = ALIGN(4096);

    .data : {
        *(.data)
        *(.data.*)
    } :data

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data
}

PHDRS {
    text PT_LOAD FLAGS(5);   /* PF_R | PF_X = 4 | 1 = 5 */
    data PT_LOAD FLAGS(6);   /* PF_R | PF_W = 4 | 2 = 6 */
}
