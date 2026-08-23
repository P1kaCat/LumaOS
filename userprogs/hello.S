/* userprogs/hello.S — Standalone LumaOS user program
 *
 * Loaded from FAT32 disk via spawn_file() / syscall 12 (exec).
 * Flat binary linked at 0x800000 (USER_CODE_ADDR).
 *
 * Demonstrates file-based program loading:
 *   - Kernel reads PROG.BIN from FAT32 filesystem
 *   - Maps it into a new process address space
 *   - Process executes and prints a message
 *   - Process exits cleanly
 *
 * Syscalls used: write(0), exit(1)
 *
 * Build:
 *   clang --target=x86_64-unknown-none -ffreestanding -nostdlib -c hello.S -o hello.o
 *   ld.lld -T userprog.ld -nostdlib --oformat elf64-x86-64 -o hello.elf hello.o
 *   llvm-objcopy -O binary hello.elf hello.bin
 */
.text
.global _start
_start:
    /* Write "Hello from loaded program!\n" to serial */
    leaq msg_hello(%rip), %rdi
    xorq %rax, %rax              /* syscall 0 = write(buf, len) */
    movq $27, %rsi               /* length */
    int $0x80

    /* Write CI marker "[EXEC12] test passed\n" */
    leaq msg_marker(%rip), %rdi
    xorq %rax, %rax
    movq $21, %rsi               /* length */
    int $0x80

    /* Exit cleanly */
    movq $1, %rax                /* syscall 1 = exit */
    xorq %rdi, %rdi
    int $0x80

    /* Should never reach here */
    hlt
    jmp _start

    /* ===== Data ===== */
msg_hello:
    .asciz "Hello from loaded program!\n"
msg_marker:
    .asciz "[EXEC12] test passed\n"
