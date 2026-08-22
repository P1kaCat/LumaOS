/* user_code.S — Position-independent user program (Phase 3 isolation test) */
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
    movq $0x41414141, (%rax)     /* write */
    movq (%rax), %rax            /* read back */
    cmpq $0x41414141, %rax
    jne .fail

    /* ---- Print "PID:X OK\n" (9 chars) ---- */
    subq $32, %rsp
    movl $0x3a444950, (%rsp)     /* "PID:" */
    movb %r12b, 4(%rsp)          /* PID (raw 1 or 2) */
    addb $0x30, 4(%rsp)          /* → ASCII */
    movl $0x0a4b4f20, 5(%rsp)    /* " OK\n" */
    movb $0, 9(%rsp)             /* null terminator */

    xorq %rax, %rax
    leaq (%rsp), %rdi
    movq $9, %rsi
    int $0x80

    /* ---- If PID == 1: attempt forbidden access ---- */
    cmpq $1, %r12
    jne .exit

    /* Print "PID:1 PF TEST\n" (14 chars) */
    leaq msg_pf(%rip), %rdi
    xorq %rax, %rax
    movq $14, %rsi
    int $0x80

    /* Try to read 0xC00000 — PID 2's physical page.
       In PID 1's page tables, PD[0][6] is supervisor-only (0x83).
       Ring 3 access → page fault → PID 1 terminated. */
    movq $0xC00000, %rax
    movq (%rax), %rax

    /* Should never reach here (page fault above) */
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

msg_pf:
    .asciz "PID:1 PF TEST\n"

msg_fail:
    .asciz "FAIL\n"

user_code_end:
