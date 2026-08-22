/* user_code.S — User program for Phase 4 testing
 *
 * Tests:
 *   1. Code page access (write/read own page)
 *   2. Heap allocation (sbrk syscall + lazy page fault mapping)
 *   3. Stack growth (access below initial mapping → PF → kernel maps page)
 *   4. Kernel memory protection (PID 1 reads 0x100000 → PF → terminated)
 */
.text
.global user_code_start
.global user_code_end

user_code_start:
    /* Get PID (syscall 2) */
    movq $2, %rax
    int $0x80
    movq %rax, %r12              /* R12 = PID (preserved across syscalls) */

    /* ---- Self-test: write/read own page ---- */
    leaq test_data(%rip), %rax   /* address within our own 2MB page */
    movq $0x41414141, (%rax)     /* write (bit 31=0, sign-extends cleanly) */
    movq (%rax), %rax            /* read back */
    cmpq $0x41414141, %rax
    jne .fail

    /* ---- Print "PID:X OK\n" (9 chars) ---- */
    subq $32, %rsp
    movl $0x3a444950, (%rsp)     /* "PID:" */
    movb %r12b, 4(%rsp)          /* PID (raw 1 or 2) */
    addb $0x30, 4(%rsp)          /* -> ASCII */
    movl $0x0a4b4f20, 5(%rsp)    /* " OK\n" */
    movb $0, 9(%rsp)             /* null terminator */

    xorq %rax, %rax
    leaq (%rsp), %rdi
    movq $9, %rsi
    int $0x80
    addq $32, %rsp

    /* ---- Test heap allocation (sbrk + lazy mapping) ---- */
    movq $3, %rax               /* syscall: sbrk */
    movq $4096, %rdi            /* increment = 1 page */
    int $0x80
    movq %rax, %r13             /* R13 = heap address (old limit = base) */

    /* Write to heap -- triggers lazy page fault, kernel maps page.
       Use 32-bit ops: 0xCAFEBABE has bit 31 set, so movq would
       sign-extend to 0xFFFFFFFF_CAFEBABE which clang rejects. */
    movl $0xCAFEBABE, (%r13)
    movl (%r13), %eax            /* read back */
    cmpl $0xCAFEBABE, %eax
    jne .heap_fail

    /* Print "HEAP OK\n" */
    leaq msg_heap_ok(%rip), %rdi
    xorq %rax, %rax
    movq $8, %rsi
    int $0x80
    jmp .stack_test

.heap_fail:
    leaq msg_heap_fail(%rip), %rdi
    xorq %rax, %rax
    movq $12, %rsi
    int $0x80

.stack_test:
    /* ---- Test stack growth (lazy page fault) ---- */
    subq $8192, %rsp             /* 2 pages below initial 4KB mapping */
    /* Use 32-bit ops for consistency (value fits in 32 bits) */
    movl $0x55555555, (%rsp)    /* write -- triggers PF, kernel maps page */
    movl (%rsp), %eax            /* read back */
    cmpl $0x55555555, %eax
    jne .stack_fail
    addq $8192, %rsp             /* restore stack */

    /* Print "STACK OK\n" */
    leaq msg_stack_ok(%rip), %rdi
    xorq %rax, %rax
    movq $9, %rsi
    int $0x80

    /* ---- If PID == 1: attempt forbidden kernel access ---- */
    cmpq $1, %r12
    jne .exit

    /* Print "PID:1 PF TEST\n" (14 chars) */
    leaq msg_pf(%rip), %rdi
    xorq %rax, %rax
    movq $14, %rsi
    int $0x80

    /* Try to read kernel memory at 0x100000 -- supervisor-only -> PF */
    movq $0x100000, %rax
    movq (%rax), %rax

    /* Should never reach here (page fault above) */
    jmp .exit

.stack_fail:
    leaq msg_stack_fail(%rip), %rdi
    xorq %rax, %rax
    movq $12, %rsi
    int $0x80
    jmp .exit

.fail:
    /* Self-test failed */
    leaq msg_fail(%rip), %rdi
    xorq %rax, %rax
    movq $5, %rsi
    int $0x80

.exit:
    /* Exit (syscall 1) */
    movq $1, %rax
    int $0x80

    /* Should never reach here */
1:
    pause
    jmp 1b

test_data:
    .quad 0

msg_heap_ok:
    .asciz "HEAP OK\n"
msg_heap_fail:
    .asciz "HEAP FAIL!\n"
msg_stack_ok:
    .asciz "STACK OK\n"
msg_stack_fail:
    .asciz "STACK FAIL!\n"
msg_pf:
    .asciz "PID:1 PF TEST\n"
msg_fail:
    .asciz "FAIL\n"

user_code_end:
