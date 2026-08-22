/*
 * isr.S — Stubs ISR/IRQ + context switch (Phase 0C++)
 * Clang .intel_syntax (no NASM)
 */

.intel_syntax noprefix

.macro ISR_NOERR n
.global isr\n
isr\n:
    push 0
    push \n
    jmp isr_common
.endm

.macro ISR_ERR n
.global isr\n
isr\n:
    push \n
    jmp isr_common
.endm

.macro IRQ_ENTRY n, irq
.global irq\n
irq\n:
    push 0
    push \irq
    jmp irq_common
.endm

/* Exceptions 0-31 */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

/* IRQ 0-15 (vectors 32-47) */
IRQ_ENTRY 0,  32
IRQ_ENTRY 1,  33
IRQ_ENTRY 2,  34
IRQ_ENTRY 3,  35
IRQ_ENTRY 4,  36
IRQ_ENTRY 5,  37
IRQ_ENTRY 6,  38
IRQ_ENTRY 7,  39
IRQ_ENTRY 8,  40
IRQ_ENTRY 9,  41
IRQ_ENTRY 10, 42
IRQ_ENTRY 11, 43
IRQ_ENTRY 12, 44
IRQ_ENTRY 13, 45
IRQ_ENTRY 14, 46
IRQ_ENTRY 15, 47

/* Syscall gate (int 0x80) */
.global isr128
isr128:
    push 0
    push 128
    jmp isr_common

/* Common ISR handler */
isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call isr_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

/* Common IRQ handler — with scheduler context switch */
irq_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call irq_handler

    /* Check if scheduler requests a context switch */
    mov al, byte ptr [sched_switch_pending]
    test al, al
    jz .no_switch
    mov byte ptr [sched_switch_pending], 0

    /* Save current RSP → current task->rsp */
    mov rax, [sched_current]
    mov [rax], rsp

    /* Load next task RSP */
    mov rax, [sched_next]
    mov rsp, [rax]

    /* current = next */
    mov rax, [sched_next]
    mov [sched_current], rax

.no_switch:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

.section .text.isr
