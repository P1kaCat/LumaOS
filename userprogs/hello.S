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
    /* Query from a real Ring 3 stack, with an adjacent overwrite canary. */
    subq $32, %rsp
    movq $0x12345678, 16(%rsp)
    movq %rsp, %rdi
    movq $16, %rsi
    movq $13, %rax
    int $0x80
    testq %rax, %rax
    jne graphics_fail
    cmpl $1, 0(%rsp)
    jne graphics_fail
    cmpl $0, 4(%rsp)
    je graphics_fail
    cmpl $0, 8(%rsp)
    je graphics_fail
    cmpl $1, 12(%rsp)
    ja graphics_fail
    cmpq $0x12345678, 16(%rsp)
    jne graphics_fail

    /* Wrong size, kernel memory, read-only text and a cross-page buffer. */
    xorq %rsi, %rsi
    movq $13, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    movq $16, %rsi
    movq $0x100000, %rdi
    movq $13, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    leaq msg_hello(%rip), %rdi
    movq $13, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    movq $0xBFFFF8, %rdi
    movq $13, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    leaq msg_graphics(%rip), %rdi
    xorq %rax, %rax
    int $0x80
    addq $32, %rsp
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

graphics_fail:
    leaq msg_graphics_fail(%rip), %rdi
    xorq %rax, %rax
    int $0x80
    movq $1, %rax
    movq $1, %rdi
    int $0x80
    jmp graphics_fail
msg_graphics:
    .asciz "[GFX9] info query and pointer checks passed\n"
msg_graphics_fail:
    .asciz "[GFX9] info query FAILED\n"
