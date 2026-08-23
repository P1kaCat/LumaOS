/* shell_code.S — LumaOS Shell v0.1 (Phase 5)
 *
 * Interactive shell — spawned by init (PID 2).
 * Built-in commands:
 *   help     - Show available commands
 *   pid      - Print current PID
 *   mem      - Show free page count
 *   sleep N  - Sleep N ticks (50Hz → 20ms each)
 *   exit     - Exit shell
 *
 * Syscalls: write(0), exit(1), getpid(2), sbrk(3),
 *           read(4), sleep(5), yield(6), getpages(7)
 */
.text
.global shell_code_start
.global shell_code_end

shell_code_start:
    /* ---- Print welcome ---- */
    leaq msg_welcome(%rip), %rdi
    xorq %rax, %rax
    movq $18, %rsi
    int $0x80

    leaq msg_help_hint(%rip), %rdi
    xorq %rax, %rax
    movq $27, %rsi
    int $0x80

    /* ---- Main shell loop ---- */
.shell_loop:
    leaq msg_prompt(%rip), %rdi
    xorq %rax, %rax
    movq $2, %rsi
    int $0x80

    xorq %r12, %r12              /* R12 = command length */

    /* ---- Read loop ---- */
.read_loop:
    movq $4, %rax               /* read(buf, 1) */
    leaq char_buf(%rip), %rdi
    movq $1, %rsi
    int $0x80
    cmpq $0, %rax
    je .no_char

    movzbl char_buf(%rip), %eax

    cmpl $0x0A, %eax             /* Enter */
    je .handle_enter

    cmpl $0x08, %eax             /* Backspace */
    je .handle_backspace

    cmpq $63, %r12               /* buffer full? */
    jae .read_loop

    /* Store char in cmd_buf BEFORE echo — the write syscall
       clobbers RAX (returns byte count), destroying AL. */
    leaq cmd_buf(%rip), %r15
    movb %al, (%r15,%r12)
    incq %r12

    /* Echo char (RAX gets clobbered here, but char already stored) */
    movq $0, %rax
    leaq char_buf(%rip), %rdi
    movq $1, %rsi
    int $0x80
    jmp .read_loop

.no_char:
    movq $6, %rax               /* yield */
    int $0x80
    jmp .read_loop

.handle_backspace:
    testq %r12, %r12
    jz .read_loop
    decq %r12
    leaq bs_seq(%rip), %rdi
    xorq %rax, %rax
    movq $3, %rsi
    int $0x80
    jmp .read_loop

.handle_enter:
    leaq nl_str(%rip), %rdi
    xorq %rax, %rax
    movq $1, %rsi
    int $0x80

    leaq cmd_buf(%rip), %r15
    movb $0, (%r15,%r12)  /* null-terminate */
    testq %r12, %r12
    jz .shell_loop               /* empty command */

    /* ---- Parse command ---- */
    leaq cmd_buf(%rip), %rdi

    leaq cmd_help(%rip), %rsi
    call strcmp
    testq %rax, %rax
    jz .do_help

    leaq cmd_pid(%rip), %rsi
    call strcmp
    testq %rax, %rax
    jz .do_pid

    leaq cmd_mem(%rip), %rsi
    call strcmp
    testq %rax, %rax
    jz .do_mem

    leaq cmd_exit(%rip), %rsi
    call strcmp
    testq %rax, %rax
    jz .do_exit

    leaq cmd_sleep(%rip), %rsi
    call starts_with
    testq %rax, %rax
    jnz .do_sleep

    leaq msg_unknown(%rip), %rdi
    xorq %rax, %rax
    movq $17, %rsi
    int $0x80
    jmp .shell_loop

    /* ---- Command handlers ---- */

.do_help:
    leaq msg_help_text(%rip), %rdi
    xorq %rax, %rax
    movq $120, %rsi
    int $0x80
    jmp .shell_loop

.do_pid:
    movq $2, %rax               /* getpid */
    int $0x80
    call itoa                   /* RDI = string ptr */
    movq %rdi, %r14             /* save itoa result */
    leaq msg_pid_prefix(%rip), %rdi
    xorq %rax, %rax
    movq $5, %rsi
    int $0x80
    movq %r14, %rdi             /* restore itoa string */
    call strlen                 /* RAX = length */
    movq %rax, %rsi
    movq %r14, %rdi
    xorq %rax, %rax
    int $0x80
    leaq nl_str(%rip), %rdi
    movq $1, %rsi
    xorq %rax, %rax
    int $0x80
    jmp .shell_loop

.do_mem:
    movq $7, %rax               /* getpages */
    int $0x80
    call itoa
    movq %rdi, %r14
    leaq msg_mem_prefix(%rip), %rdi
    xorq %rax, %rax
    movq $12, %rsi
    int $0x80
    movq %r14, %rdi
    call strlen
    movq %rax, %rsi
    movq %r14, %rdi
    xorq %rax, %rax
    int $0x80
    leaq nl_str(%rip), %rdi
    movq $1, %rsi
    xorq %rax, %rax
    int $0x80
    jmp .shell_loop

.do_sleep:
    leaq cmd_buf+6(%rip), %rdi  /* skip "sleep " */
    call parse_num
    testq %rax, %rax
    jnz .sleep_have_num
    movq $50, %rax              /* default: 1 second */
.sleep_have_num:
    movq %rax, %r13
    leaq msg_sleeping(%rip), %rdi
    xorq %rax, %rax
    movq $12, %rsi
    int $0x80
    movq $5, %rax              /* sleep(ticks) */
    movq %r13, %rdi
    int $0x80
    leaq msg_done(%rip), %rdi
    xorq %rax, %rax
    movq $5, %rsi
    int $0x80
    jmp .shell_loop

.do_exit:
    movq $1, %rax
    int $0x80
1:
    pause
    jmp 1b

    /* ===== Helper functions ===== */

    /* strcmp: compare null-terminated strings at RDI and RSI */
    /* Returns: RAX = 0 if equal, 1 if different */
strcmp:
    pushq %rbx
.strcmp_loop:
    movb (%rdi), %al
    movb (%rsi), %bl
    testb %al, %al
    jz .strcmp_end
    cmpb %al, %bl
    jne .strcmp_diff
    incq %rdi
    incq %rsi
    jmp .strcmp_loop
.strcmp_end:
    testb %bl, %bl
    jz .strcmp_equal
.strcmp_diff:
    movq $1, %rax
    popq %rbx
    ret
.strcmp_equal:
    xorq %rax, %rax
    popq %rbx
    ret

    /* starts_with: check if string at RDI starts with prefix at RSI */
    /* Returns: RAX = prefix length if match, 0 if no match */
starts_with:
    xorq %rax, %rax
.sw_loop:
    movb (%rsi, %rax), %cl
    testb %cl, %cl
    jz .sw_match
    movb (%rdi, %rax), %dl
    cmpb %cl, %dl
    jne .sw_fail
    incq %rax
    jmp .sw_loop
.sw_match:
    ret
.sw_fail:
    xorq %rax, %rax
    ret

    /* itoa: convert RAX to decimal string in itoa_buf */
    /* Returns: RDI = pointer to start of string */
    /* Clobbers: RAX, RCX, RDX */
itoa:
    leaq itoa_buf+23(%rip), %rdi
    movb $0, (%rdi)             /* null terminator */
    testq %rax, %rax
    jnz .itoa_loop
    decq %rdi
    movb $0x30, (%rdi)          /* '0' */
    ret
.itoa_loop:
    xorq %rdx, %rdx
    movq $10, %rcx
    divq %rcx                   /* RDX:RAX / 10 */
    addb $0x30, %dl             /* remainder → ASCII */
    decq %rdi
    movb %dl, (%rdi)
    testq %rax, %rax
    jnz .itoa_loop
    ret

    /* strlen: length of string at RDI */
    /* Returns: RAX = length */
strlen:
    xorq %rax, %rax
.strlen_loop:
    movb (%rdi, %rax), %cl
    testb %cl, %cl
    jz .strlen_done
    incq %rax
    jmp .strlen_loop
.strlen_done:
    ret

    /* parse_num: parse decimal number from string at RDI */
    /* Returns: RAX = number (0 if no valid digits) */
parse_num:
    xorq %rax, %rax
.parse_loop:
    movb (%rdi), %cl
    testb %cl, %cl
    jz .parse_done
    subb $0x30, %cl
    jb .parse_done
    cmpb $9, %cl
    ja .parse_done
    imulq $10, %rax, %rax
    movzbq %cl, %rcx
    addq %rcx, %rax
    incq %rdi
    jmp .parse_loop
.parse_done:
    ret

    /* ===== Data ===== */

cmd_buf:
    .skip 64
char_buf:
    .byte 0
itoa_buf:
    .skip 24

msg_welcome:
    .asciz "LumaOS Shell v0.1\n"
msg_help_hint:
    .asciz "Type 'help' for commands\n\n"
msg_prompt:
    .asciz "> "
msg_unknown:
    .asciz "Unknown command\n"
msg_help_text:
    .asciz "Commands:\n  help    - Show this help\n  pid     - Print PID\n  mem     - Show free pages\n  sleep N - Sleep N ticks (50Hz)\n  exit    - Exit shell\n"
msg_pid_prefix:
    .asciz "PID: "
msg_mem_prefix:
    .asciz "Free pages: "
msg_sleeping:
    .asciz "Sleeping...\n"
msg_done:
    .asciz "Done\n"
nl_str:
    .asciz "\n"
bs_seq:
    .asciz "\b \b"

cmd_help:
    .asciz "help"
cmd_pid:
    .asciz "pid"
cmd_mem:
    .asciz "mem"
cmd_exit:
    .asciz "exit"
cmd_sleep:
    .asciz "sleep "

shell_code_end:
