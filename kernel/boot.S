/*
 * boot.S — Point d'entrée du kernel LumaOS en assembly
 *
 * Clang assembler intégré (pas besoin de NASM).
 * Syntaxe Intel via .intel_syntax noprefix.
 *
 * Le bootloader saute ici avec RDI = pointeur vers le handoff struct.
 */

.intel_syntax noprefix

.section .text.boot
.global _start

_start:
    mov rsp, offset stack_top   /* Stack 16 KB */
    call kernel_main             /* RDI déjà = handoff pointer */
.hang:
    cli
    hlt
    jmp .hang

.section .bss
.align 16
stack_bottom:
    .skip 16384                  /* 16 KB de stack */
stack_top:
