/* userprogs/hello.S — Standalone LumaOS user program (ELF64)
 *
 * Loaded from FAT32 disk via spawn_file() / syscall 12 (exec).
 * Linked as a proper ELF64 executable at 0x800000 with two PT_LOAD
 * segments (text R+X, data R+W) via userprog.ld.
 *
 * The ELF64 loader:
 *   - Reads PROG.ELF from FAT32 filesystem via VFS
 *   - Validates ELF header (magic, class, endianness, type, machine)
 *   - Parses PT_LOAD program headers
 *   - Allocates pages, maps at p_vaddr with PF_R/PF_W/PF_X permissions
 *   - Copies p_filesz bytes from file, zero-fills p_memsz - p_filesz
 *   - Sets RIP = e_entry, creates Ring 3 process
 *
 * Syscalls used: write(0), exit(1)
 *
 * Build:
 *   clang --target=x86_64-unknown-none -ffreestanding -nostdlib -c hello.S -o hello.o
 *   ld.lld -T userprog.ld -nostdlib --oformat elf64-x86-64 -o hello.elf hello.o
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
