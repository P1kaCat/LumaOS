/* userprogs/hello.S — Standalone LumaOS user program (ELF64)
 *
 * Loaded from FAT32 disk via spawn_file() / syscall 12 (exec).
 * Linked as a proper ELF64 executable at 0x800000 with two PT_LOAD
 * segments (text R+X, data R+W) via userprog.ld.
 *
 * Phase 9 also uses this real Ring 3 program to regression-test the graphics
 * ABI: information query, exclusive compositor ownership and bounded surface
 * presentation with invalid-pointer/geometry rejection.
 */
.text
.global _start
_start:
    /* Stack layout (160 bytes):
     *   +0   graphics_info (16)
     *   +16  overwrite canary (8)
     *   +32  lumaos_surface_present (48)
     *   +80  2x2 native pixel surface (16)
     */
    subq $160, %rsp

    /* Query from a real Ring 3 stack, with an adjacent overwrite canary. */
    movq $0x12345678, 16(%rsp)
    movq %rsp, %rdi
    movq $16, %rsi
    movq $13, %rax
    int $0x80
    testq %rax, %rax
    jne graphics_fail
    cmpl $2, 0(%rsp)
    jne graphics_fail
    cmpl $0, 4(%rsp)
    je graphics_fail
    cmpl $0, 8(%rsp)
    je graphics_fail
    cmpl $1, 12(%rsp)
    ja graphics_fail
    cmpq $0x12345678, 16(%rsp)
    jne graphics_fail

    /* Wrong info size, kernel memory, read-only text and a cross-page buffer. */
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

    /* Build a tiny 2x2 native-format surface and presentation request. */
    movl $0x00204080, 80(%rsp)
    movl $0x00804020, 84(%rsp)
    movl $0x00408020, 88(%rsp)
    movl $0x00802040, 92(%rsp)
    leaq 80(%rsp), %rax
    movq %rax, 32(%rsp)          /* pixels */
    movl $2, 40(%rsp)            /* surface_width */
    movl $2, 44(%rsp)            /* surface_height */
    movl $8, 48(%rsp)            /* stride_bytes */
    movl $0, 52(%rsp)            /* src_x */
    movl $0, 56(%rsp)            /* src_y */
    movl $4, 60(%rsp)            /* dst_x */
    movl $4, 64(%rsp)            /* dst_y */
    movl $2, 68(%rsp)            /* width */
    movl $2, 72(%rsp)            /* height */
    movl $0, 76(%rsp)            /* reserved */

    /* Exactly one live process may own presentation. */
    movq $14, %rax
    int $0x80
    testq %rax, %rax
    je graphics_owner
    cmpq $-1, %rax
    je graphics_non_owner
    jmp graphics_fail

graphics_non_owner:
    /* A second process can submit valid memory, but not present it. */
    leaq 32(%rsp), %rdi
    movq $48, %rsi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    leaq msg_non_owner(%rip), %rdi
    xorq %rax, %rax
    xorq %rsi, %rsi
    int $0x80
    addq $160, %rsp
    movq $1, %rax
    xorq %rdi, %rdi
    int $0x80
    jmp graphics_fail

graphics_owner:
    /* Valid bounded presentation. */
    leaq 32(%rsp), %rdi
    movq $48, %rsi
    movq $15, %rax
    int $0x80
    testq %rax, %rax
    jne graphics_fail

    /* Wrong request size. */
    leaq 32(%rsp), %rdi
    movq $47, %rsi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail

    /* Stride too small for the declared surface. */
    movl $4, 48(%rsp)
    leaq 32(%rsp), %rdi
    movq $48, %rsi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    movl $8, 48(%rsp)

    /* Source rectangle exceeds the surface. */
    movl $2, 52(%rsp)
    leaq 32(%rsp), %rdi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    movl $0, 52(%rsp)

    /* Destination rectangle exceeds the logical graphics area. */
    movl 4(%rsp), %eax
    movl %eax, 60(%rsp)
    leaq 32(%rsp), %rdi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    movl $4, 60(%rsp)

    /* 32-bit stride cannot represent a 0x40000000-pixel row. */
    movl $0x40000000, 40(%rsp)
    movl $0xFFFFFFFF, 48(%rsp)
    movl $1, 68(%rsp)
    leaq 32(%rsp), %rdi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    movl $2, 40(%rsp)
    movl $8, 48(%rsp)
    movl $2, 68(%rsp)

    /* Pixel row crosses from the mapped stack page into an inaccessible page. */
    movq $0xBFFFFC, 32(%rsp)
    movl $1, 44(%rsp)
    movl $1, 72(%rsp)
    leaq 32(%rsp), %rdi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    leaq 80(%rsp), %rax
    movq %rax, 32(%rsp)
    movl $2, 44(%rsp)
    movl $2, 72(%rsp)

    /* Bad request pointer and request crossing the user stack boundary. */
    movq $0x100000, %rdi
    movq $48, %rsi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail
    movq $0xBFFFF0, %rdi
    movq $15, %rax
    int $0x80
    cmpq $-1, %rax
    jne graphics_fail

    /* Spawn the same ELF while ownership is held. The child must take the
     * graphics_non_owner path, proving process isolation. sleep() marks the
     * owner non-runnable. We only need to remain in userspace long enough for
     * the first 50 Hz timer IRQ; after that, the five-tick sleep gives the
     * child ample time to run before this owner can release the display. */
    leaq msg_prog(%rip), %rdi
    movq $12, %rax
    int $0x80
    cmpq $0, %rax
    jl graphics_fail
    movq $5, %rax
    movq $5, %rdi
    int $0x80
    movq $1000000, %rcx
1:
    pause
    loop 1b

    /* Voluntary release and immediate reacquire are both controlled. */
    movq $16, %rax
    int $0x80
    testq %rax, %rax
    jne graphics_fail
    movq $14, %rax
    int $0x80
    testq %rax, %rax
    jne graphics_fail
    movq $16, %rax
    int $0x80
    testq %rax, %rax
    jne graphics_fail

    leaq msg_surface(%rip), %rdi
    xorq %rax, %rax
    xorq %rsi, %rsi
    int $0x80

    leaq msg_graphics(%rip), %rdi
    xorq %rax, %rax
    int $0x80
    addq $160, %rsp

    /* Write "Hello from loaded program!\n" to serial */
    leaq msg_hello(%rip), %rdi
    xorq %rax, %rax
    movq $27, %rsi
    int $0x80

    /* Write CI marker "[EXEC12] test passed\n" */
    leaq msg_marker(%rip), %rdi
    xorq %rax, %rax
    movq $21, %rsi
    int $0x80

    /* Exit cleanly */
    movq $1, %rax
    xorq %rdi, %rdi
    int $0x80

    hlt
    jmp _start

msg_hello:
    .asciz "Hello from loaded program!\n"
msg_marker:
    .asciz "[EXEC12] test passed\n"
msg_prog:
    .asciz "prog.elf"
msg_graphics:
    .asciz "[GFX9] info query and pointer checks passed\n"
msg_surface:
    .asciz "[GFX9] surface presentation and ownership checks passed\n"
msg_non_owner:
    .asciz "[GFX9] non-owner presentation rejected\n"

graphics_fail:
    leaq msg_graphics_fail(%rip), %rdi
    xorq %rax, %rax
    xorq %rsi, %rsi
    int $0x80
    addq $160, %rsp
    movq $1, %rax
    movq $1, %rdi
    int $0x80
    jmp graphics_fail
msg_graphics_fail:
    .asciz "[GFX9] graphics surface test FAILED\n"
