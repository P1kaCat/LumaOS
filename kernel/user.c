/* user.c — User program (Ring 3) + enter_ring3 */
#include <stdint.h>
#include "cpu.h"

/* User stack — 16KB */
static uint64_t user_stack[2048];

/* User program runs in Ring 3. Can only use int 0x80 for syscalls. */
void user_program(void) {
    const char *msg1 = "Hello from Ring 3!\n";
    const char *msg2 = "Syscalls are working!\n";
    const char *msg3 = "Ring 3 is alive and isolated.\n";

    __asm__ volatile (
        "mov $0, %%rax\n"
        "mov %0, %%rdi\n"
        "mov $18, %%rsi\n"
        "int $0x80\n"
        : : "r"(msg1) : "rax", "rdi", "rsi"
    );
    __asm__ volatile (
        "mov $0, %%rax\n"
        "mov %0, %%rdi\n"
        "mov $22, %%rsi\n"
        "int $0x80\n"
        : : "r"(msg2) : "rax", "rdi", "rsi"
    );
    __asm__ volatile (
        "mov $0, %%rax\n"
        "mov %0, %%rdi\n"
        "mov $29, %%rsi\n"
        "int $0x80\n"
        : : "r"(msg3) : "rax", "rdi", "rsi"
    );

    /* Spin in user mode (pause is NOT privileged, hlt IS) */
    for (;;) __asm__ volatile ("pause");
}

/* Transition from Ring 0 to Ring 3 via iretq */
__attribute__((noinline, noreturn))
static void enter_ring3(void *entry, void *stack_top) {
    __asm__ volatile (
        "cli\n"
        "mov %[sp], %%rsp\n"    /* switch to user stack */
        "pushq $0x23\n"         /* SS = USER_DS */
        "pushq %[sp]\n"         /* RSP = user stack top */
        "pushq $0x202\n"        /* RFLAGS (IF=1) */
        "pushq $0x1B\n"         /* CS = USER_CS */
        "pushq %[rip]\n"        /* RIP = entry */
        "iretq\n"
        :
        : [sp] "r"((uint64_t)stack_top), [rip] "r"((uint64_t)entry)
        : "rsp", "memory"
    );
    __builtin_unreachable();
}

void user_init(void) {
    serial_puts("[*] Entering Ring 3...\n");
    enter_ring3((void *)user_program, (void *)&user_stack[2048]);
}
