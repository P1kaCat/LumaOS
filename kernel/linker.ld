/*
 * linker.ld — Script de linkage pour le kernel LumaOS
 *
 * Le kernel est chargé à l'adresse physique 0x100000 (1 MB).
 * Pas de paging en Phase 0B, donc adresse virtuelle = adresse physique.
 *
 * _start est placé en premier dans .text.boot pour garantir
 * qu'il soit à l'adresse d'entrée (0x100000).
 */

ENTRY(_start)

SECTIONS {
    . = 0x100000;

    .text : {
        *(.text.boot)    /* _start d'abord */
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }

    _kernel_end = .;

    /* Discarder les sections inutiles */
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note .note.*)
        *(.debug .debug_*)
    }
}
