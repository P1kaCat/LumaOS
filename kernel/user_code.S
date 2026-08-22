/* user_code.S — Position-independent user program (Phase 3) */
.text
.global user_code_start
.global user_code_end

user_code_start:
    /* Get PID (syscall 2) */
    movq $2, %rax
    int $0x80
    /* RAX = PID (1 or 2) */
    movq %rax, %rbx

    /* Build "PID:X\n" on stack (6 chars) */
    subq $16, %rsp
    movl $0x3a444950, (%rsp)    /* "PID:" little-endian */
    addb $0x30, %bl             /* PID -> ASCII digit */
    movb %bl, 4(%rsp)           /* store digit */
    movw $0x000a, 5(%rsp)       /* "\n\0" */

    /* Print (syscall 0: write_serial(ptr, len)) */
    xorq %rax, %rax
    leaq (%rsp), %rdi
    movq $6, %rsi
    int $0x80

    /* Exit (syscall 1) */
    movq $1, %rax
    int $0x80

    /* Should never reach here */
1:
    pause
    jmp 1b

user_code_end:
