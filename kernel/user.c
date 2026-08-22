/* user.c — User mode (Ring 3) process setup
 * Phase 4: separate stack/heap regions, pre-mapped initial stack page
 * Phase 5: single shell process (PID 1)
 */
#include <stdint.h>
#include "cpu.h"
#include "sched.h"
#include "mem.h"

extern const unsigned char user_code_start[];
extern const unsigned char user_code_end[];

#define USER_CODE_ADDR   0x800000ULL
#define PROC1_PHYS       0x800000ULL

static void copy_user_code(uint64_t phys_addr) {
    uint8_t *dst = (uint8_t *)(unsigned long)phys_addr;
    const uint8_t *src = user_code_start;
    uint64_t len = user_code_end - user_code_start;
    for (uint64_t i = 0; i < len; i++)
        dst[i] = src[i];
}

void user_init(void) {
    serial_puts("[*] Creating shell process...\n");

    copy_user_code(PROC1_PHYS);
    uint64_t cr3 = create_user_pml4(0, PROC1_PHYS);
    uint64_t stack_page = alloc_page();
    map_page(cr3, USER_STACK_TOP - PAGE_SIZE, stack_page,
             PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    int pid = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3, USER_HEAP_BASE);
    serial_puts("[+] Shell PID=1 (phys 0x800000, stack @0xBFF000, heap @0x1000000)\n");
    (void)pid;
}
