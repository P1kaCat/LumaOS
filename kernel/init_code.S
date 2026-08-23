/* init_code.S — LumaOS init process (Phase 5/6)
 *
 * Init is the first user process (PID 1), launched directly by the kernel.
 * Responsibilities:
 *   1. Run the Phase 6 filesystem syscall regression test ([SYSCALL6])
 *   2. Run the Phase 6 cat test — read+print hello.txt ([CAT6])
 *   3. Spawn the shell process (PID 2) via syscall 11
 *   4. Print [INIT5] test passed marker for CI
 *   5. Exit
 *
 * Syscalls: write(0), exit(1), open(8), close(9), read(10), spawn(11)
 */
.text
.global init_code_start
.global init_code_end

init_code_start:
    /* ---- Phase 6: Filesystem syscall regression test ----
     * Exercises syscalls 8 (open), 9 (close), 10 (read) from Ring 3.
     * Marker: "[SYSCALL6] test passed" for CI.
     */
    xorq %r12, %r12          /* R12 = 0 (all passed) */

    leaq msg_syscall6_start(%rip), %rdi
    xorq %rax, %rax
    movq $32, %rsi
    int $0x80

    /* Test 1: open("hello.txt") */
    movq $8, %rax
    leaq path_hello(%rip), %rdi
    int $0x80
    cmpq $0, %rax
    jl .st_fail_open
    movq %rax, %r13          /* save fd */

    /* Test 2: read(fd, test_buf, 255) */
    movq $10, %rax
    movq %r13, %rdi
    leaq test_buf(%rip), %rsi
    movq $255, %rdx
    int $0x80
    cmpq $0, %rax
    jl .st_fail_read
    movq %rax, %r14          /* save byte count */
    leaq msg_st_read_ok(%rip), %rdi
    xorq %rax, %rax
    movq $22, %rsi
    int $0x80
    movq $0, %rax
    leaq test_buf(%rip), %rdi
    movq %r14, %rsi
    int $0x80
    leaq nl_str(%rip), %rdi
    xorq %rax, %rax
    movq $1, %rsi
    int $0x80

    /* Test 3: read again → 0 (EOF) */
    movq $10, %rax
    movq %r13, %rdi
    leaq test_buf(%rip), %rsi
    movq $1, %rdx
    int $0x80
    cmpq $0, %rax
    jne .st_fail_eof

    /* Test 4: close(fd) */
    movq $9, %rax
    movq %r13, %rdi
    int $0x80
    cmpq $0, %rax
    jne .st_fail_close

    /* Test 5: open("nonexist.txt") → negative */
    movq $8, %rax
    leaq path_nonexist(%rip), %rdi
    int $0x80
    cmpq $0, %rax
    jge .st_fail_badpath

    /* Test 6: read(99, buf, 10) → negative (BAD_FD) */
    movq $10, %rax
    movq $99, %rdi
    leaq test_buf(%rip), %rsi
    movq $10, %rdx
    int $0x80
    cmpq $0, %rax
    jge .st_fail_bfd_read

    /* Test 7: close(99) → negative (BAD_FD) */
    movq $9, %rax
    movq $99, %rdi
    int $0x80
    cmpq $0, %rax
    jge .st_fail_bfd_close

    /* Test 8: open("hello.txt") again for ptr test */
    movq $8, %rax
    leaq path_hello(%rip), %rdi
    int $0x80
    cmpq $0, %rax
    jl .st_fail_open2
    movq %rax, %r13

    /* Test 9: read(fd, 0x1, 10) → invalid pointer */
    movq $10, %rax
    movq %r13, %rdi
    movq $1, %rsi
    movq $10, %rdx
    int $0x80
    cmpq $0, %rax
    jge .st_fail_badptr

    /* Test 10: close(fd) cleanup */
    movq $9, %rax
    movq %r13, %rdi
    int $0x80

    /* All passed */
    testq %r12, %r12
    jnz .st_failed
    leaq msg_syscall6_pass(%rip), %rdi
    xorq %rax, %rax
    movq $23, %rsi
    int $0x80
    jmp .st_done

    /* Failure handlers */
.st_fail_open:
    movq $1, %r12
    leaq msg_st_open_fail(%rip), %rdi
    xorq %rax, %rax
    movq $18, %rsi
    int $0x80
    jmp .st_done

.st_fail_read:
    movq $1, %r12
    leaq msg_st_read_fail(%rip), %rdi
    xorq %rax, %rax
    movq $18, %rsi
    int $0x80
    jmp .st_done

.st_fail_eof:
    movq $1, %r12
    leaq msg_st_eof_fail(%rip), %rdi
    xorq %rax, %rax
    movq $20, %rsi
    int $0x80
    /* Close the fd */
    movq $9, %rax
    movq %r13, %rdi
    int $0x80
    jmp .st_done

.st_fail_close:
    movq $1, %r12
    leaq msg_st_close_fail(%rip), %rdi
    xorq %rax, %rax
    movq $19, %rsi
    int $0x80
    jmp .st_done

.st_fail_badpath:
    movq $1, %r12
    leaq msg_st_badpath_fail(%rip), %rdi
    xorq %rax, %rax
    movq $25, %rsi
    int $0x80
    jmp .st_done

.st_fail_bfd_read:
    movq $1, %r12
    leaq msg_st_bfd_read_fail(%rip), %rdi
    xorq %rax, %rax
    movq $23, %rsi
    int $0x80
    jmp .st_done

.st_fail_bfd_close:
    movq $1, %r12
    leaq msg_st_bfd_close_fail(%rip), %rdi
    xorq %rax, %rax
    movq $24, %rsi
    int $0x80
    jmp .st_done

.st_fail_open2:
    movq $1, %r12
    leaq msg_st_open2_fail(%rip), %rdi
    xorq %rax, %rax
    movq $20, %rsi
    int $0x80
    jmp .st_done

.st_fail_badptr:
    movq $1, %r12
    leaq msg_st_badptr_fail(%rip), %rdi
    xorq %rax, %rax
    movq $24, %rsi
    int $0x80
    /* Close fd */
    movq $9, %rax
    movq %r13, %rdi
    int $0x80
    jmp .st_done

.st_failed:
    leaq msg_syscall6_fail(%rip), %rdi
    xorq %rax, %rax
    movq $23, %rsi
    int $0x80

.st_done:
    leaq nl_str(%rip), %rdi
    xorq %rax, %rax
    movq $1, %rsi
    int $0x80

    /* ---- Phase 6: Cat test ----
     * Reads hello.txt in a loop and prints content, mimicking cat.
     * Marker: "[CAT6] test passed" for CI.
     */
    xorq %r12, %r12          /* R12 = 0 (all passed) */

    movq $8, %rax            /* open("hello.txt") */
    leaq path_hello(%rip), %rdi
    int $0x80
    cmpq $0, %rax
    jl .cat_fail
    movq %rax, %r13          /* save fd */

.cat_read_loop:
    movq $10, %rax           /* read(fd, buf, 255) */
    movq %r13, %rdi
    leaq test_buf(%rip), %rsi
    movq $255, %rdx
    int $0x80
    cmpq $0, %rax
    jl .cat_fail_read
    je .cat_eof              /* EOF */

    /* Write read data to serial */
    movq %rax, %r14          /* save byte count */
    xorq %rax, %rax          /* write(1, buf, n) */
    leaq test_buf(%rip), %rdi
    movq %r14, %rsi
    int $0x80
    jmp .cat_read_loop

.cat_eof:
    /* Close file */
    movq $9, %rax
    movq %r13, %rdi
    int $0x80
    /* Print [CAT6] test passed */
    leaq msg_cat6_pass(%rip), %rdi
    xorq %rax, %rax
    movq $19, %rsi
    int $0x80
    jmp .cat_done

.cat_fail_read:
    /* Close fd */
    movq $9, %rax
    movq %r13, %rdi
    int $0x80

.cat_fail:
    movq $1, %r12
    leaq msg_cat6_fail(%rip), %rdi
    xorq %rax, %rax
    movq $19, %rsi
    int $0x80

.cat_done:
    leaq nl_str(%rip), %rdi
    xorq %rax, %rax
    movq $1, %rsi
    int $0x80

    /* ---- Spawn shell process (PID 2) ---- */
    movq $11, %rax            /* spawn(prog_id) */
    xorq %rdi, %rdi           /* prog_id = 0 (shell) */
    int $0x80
    cmpq $0, %rax
    jl .init_spawn_fail

    /* Print [INIT5] test passed */
    leaq msg_init5_pass(%rip), %rdi
    xorq %rax, %rax
    movq $20, %rsi
    int $0x80

    /* Exit — shell continues running as PID 2 */
    movq $1, %rax
    xorq %rdi, %rdi
    int $0x80

.init_spawn_fail:
    leaq msg_init5_fail(%rip), %rdi
    xorq %rax, %rax
    movq $19, %rsi
    int $0x80
    movq $1, %rax
    xorq %rdi, %rdi
    int $0x80

    /* ===== Data ===== */

nl_str:
    .asciz "\n"

msg_syscall6_start:
    .asciz "\n[*] Syscall FS regression test\n"
msg_syscall6_pass:
    .asciz "[SYSCALL6] test passed\n"
msg_syscall6_fail:
    .asciz "[SYSCALL6] test FAILED\n"
msg_st_read_ok:
    .asciz "  [+] Read from file: "
msg_st_open_fail:
    .asciz "  [!] open failed\n"
msg_st_read_fail:
    .asciz "  [!] read failed\n"
msg_st_eof_fail:
    .asciz "  [!] EOF test fail\n"
msg_st_close_fail:
    .asciz "  [!] close failed\n"
msg_st_badpath_fail:
    .asciz "  [!] bad path test fail\n"
msg_st_bfd_read_fail:
    .asciz "  [!] bad FD read fail\n"
msg_st_bfd_close_fail:
    .asciz "  [!] bad FD close fail\n"
msg_st_open2_fail:
    .asciz "  [!] reopen failed\n"
msg_st_badptr_fail:
    .asciz "  [!] bad ptr test fail\n"
path_hello:
    .asciz "hello.txt"
path_nonexist:
    .asciz "nonexist.txt"
test_buf:
    .skip 256

msg_cat6_pass:
    .asciz "[CAT6] test passed\n"
msg_cat6_fail:
    .asciz "[CAT6] test FAILED\n"

msg_init5_pass:
    .asciz "[INIT5] test passed\n"
msg_init5_fail:
    .asciz "[INIT5] spawn FAIL\n"

init_code_end:
