/* user_code.S — Position-independent user program (copied to 0x800000) */
.text
.global user_code_start
.global user_code_end

user_code_start:
    /* Syscall 0: write_serial(rdi=ptr, rsi=len) */

    /* Message 1: "Hello from Ring 3!\\n" (19 chars) */
    leaq msg1(%rip), %rdi
    xorq %rax, %rax
    movq $19, %rsi
    int $0x80

    /* Message 2: "Exiting normally.\\n" (18 chars) */
    leaq msg2(%rip), %rdi
    xorq %rax, %rax
    movq $18, %rsi
    int $0x80

    /* Exit (syscall 1) */
    movq $1, %rax
    int $0x80

    /* Should never reach here */
1:
    pause
    jmp 1b

msg1:
    .asciz "Hello from Ring 3!\n"
msg2:
    .asciz "Exiting normally.\n"

user_code_end:
