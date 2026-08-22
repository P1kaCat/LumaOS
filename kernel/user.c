/* user.c — User mode (Ring 3) process setup
 * Phase 4: separate stack/heap regions, pre-mapped initial stack page
 */
#include <stdint.h>
#include "cpu.h"
#include "sched.h"
#include "mem.h"

extern const unsigned char user_code_start[];
extern const unsigned char user_code_end[];

#define USER_CODE_ADDR   0x800000ULL   /* virtual address for user code (2MB page) */

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
    /* Pre-map initial stack page (top of stack region) */
    uint64_t stack_page1 = alloc_page();
    map_page(cr3_1, USER_STACK_TOP - PAGE_SIZE, stack_page1,
             PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    int pid1 = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3_1,
                                 USER_HEAP_BASE);
    serial_puts("[+] Process PID=1 (phys 0x800000, stack @0xBFF000, heap @0x1000000)\n");
    (void)pid1;

    /* Process 2: code at physical 0xC00000 */
    copy_user_code(PROC2_PHYS);
    uint64_t cr3_2 = create_user_pml4(1, PROC2_PHYS);
    uint64_t stack_page2 = alloc_page();
    map_page(cr3_2, USER_STACK_TOP - PAGE_SIZE, stack_page2,
             PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    int pid2 = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3_2,
                                 USER_HEAP_BASE);
    serial_puts("[+] Process PID=2 (phys 0xC00000, stack @0xBFF000, heap @0x1000000)\n");
    (void)pid2;
}
