; boot.asm — Point d'entrée du kernel LumaOS en assembly
;
; Le bootloader saute ici avec RDI = pointeur vers le handoff struct.
; On met en place la stack, puis on appelle kernel_main(handoff).
;
; ABI : System V AMD64 (le kernel est compilé en ELF)
;   - RDI = 1er argument (handoff pointer)
;
; NASM syntaxe Intel, format ELF64

section .text.boot
global _start

_start:
    ; Mettre en place la stack (16KB, 16-byte aligned)
    mov rsp, stack_top

    ; Appeler kernel_main(handoff)
    ; RDI contient déjà le handoff pointer (mis par le bootloader)
    call kernel_main

    ; kernel_main ne devrait jamais revenir, mais au cas où :
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384        ; 16 KB de stack
stack_top:
