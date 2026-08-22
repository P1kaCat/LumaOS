#include <stdint.h>
#include "cpu.h"

extern const unsigned char user_code_start[];
extern const unsigned char user_code_end[];

#define USER_CODE_ADDR  0x40000000ULL
#define USER_STACK_TOP  0x40200000ULL

__attribute__((noinline, noreturn))
static void enter_ring3(void *entry, void *stack_top) {
    __asm__ volatile (
        "cli\n"
        "mov %[sp], %%rsp\n"
        "pushq $0x23\n"
        "pushq %[sp]\n"
        "pushq $0x202\n"
        "pushq $0x1B\n"
        "pushq %[rip]\n"
        "iretq\n"
        :
        : [sp] "r"((uint64_t)stack_top), [rip] "r"((uint64_t)entry)
        : "rsp", "memory"
    );
    __builtin_unreachable();
}

void user_init(void) {
    serial_puts("[*] Entering Ring 3 (page-level isolation)...\n");

    /* Copy position-independent user code to user memory */
    uint8_t *dst = (uint8_t *)(unsigned long)USER_CODE_ADDR;
    const uint8_t *src = user_code_start;
    uint64_t len = user_code_end - user_code_start;
    for (uint64_t i = 0; i < len; i++)
        dst[i] = src[i];

    serial_puts("[+] User code copied to 0x40000000\n");

    enter_ring3((void *)(unsigned long)USER_CODE_ADDR,
                (void *)(unsigned long)USER_STACK_TOP);
}
