/* user_code.S — Position-independent user program (copied to 0x40000000) */
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

    /* Message 2: "Testing isolation...\\n" (20 chars) */
    leaq msg2(%rip), %rdi
    xorq %rax, %rax
    movq $20, %rsi
    int $0x80

    /* Test: try to read kernel memory at 0x100000 — should #PF in Ring 3 */
    movabsq $0x100000, %rbx
    movq (%rbx), %rax

    /* Should never reach here */
1:
    pause
    jmp 1b

msg1:
    .asciz "Hello from Ring 3!\n"
msg2:
    .asciz "Testing isolation...\n"

user_code_end:
