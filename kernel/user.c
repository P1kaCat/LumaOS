#include <stdint.h>
#include "cpu.h"
#include "sched.h"
#include "mem.h"

extern const unsigned char user_code_start[];
extern const unsigned char user_code_end[];

#define USER_CODE_ADDR   0x800000ULL   /* virtual address for all processes */
#define USER_STACK_TOP   0xA00000ULL   /* top of 2MB user page */

/* Physical addresses for each process's user code copy */
#define PROC1_PHYS  0x800000ULL   /* 8MB */
#define PROC2_PHYS 0xC00000ULL   /* 12MB */

static void copy_user_code(uint64_t phys_addr) {
    uint8_t *dst = (uint8_t *)(unsigned long)phys_addr;
    const uint8_t *src = user_code_start;
    uint64_t len = user_code_end - user_code_start;
    for (uint64_t i = 0; i < len; i++)
        dst[i] = src[i];
}

void user_init(void) {
    serial_puts("[*] Creating user processes...\n");

    /* Process 1: code at physical 0x800000 */
    copy_user_code(PROC1_PHYS);
    uint64_t cr3_1 = create_user_pml4(0, PROC1_PHYS);
    int pid1 = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3_1);
    serial_puts("[+] Process PID=1 (phys 0x800000, CR3 created)\n");
    (void)pid1;

    /* Process 2: code at physical 0xC00000 */
    copy_user_code(PROC2_PHYS);
    uint64_t cr3_2 = create_user_pml4(1, PROC2_PHYS);
    int pid2 = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3_2);
    serial_puts("[+] Process PID=2 (phys 0xC00000, CR3 created)\n");
    (void)pid2;
}
