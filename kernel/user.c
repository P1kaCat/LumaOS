/* user.c — User mode (Ring 3) process setup
 * Phase 5: init → shell separation
 * Kernel launches init (PID 1), init spawns shell (PID 2) via syscall 11.
 */
#include <stdint.h>
#include "cpu.h"
#include "sched.h"
#include "mem.h"

extern const unsigned char init_code_start[];
extern const unsigned char init_code_end[];
extern const unsigned char shell_code_start[];
extern const unsigned char shell_code_end[];

#define USER_CODE_ADDR   0x800000ULL
#define INIT_PHYS        0x800000ULL

static int next_proc_idx = 0;

static void copy_code(uint64_t phys_addr, const unsigned char *start, const unsigned char *end) {
    uint8_t *dst = (uint8_t *)(unsigned long)phys_addr;
    uint64_t len = end - start;
    for (uint64_t i = 0; i < len; i++)
        dst[i] = start[i];
}

void user_init(void) {
    serial_puts("[*] Creating init process...\n");

    copy_code(INIT_PHYS, init_code_start, init_code_end);
    uint64_t cr3 = create_user_pml4(next_proc_idx++, INIT_PHYS);
    uint64_t stack_page = alloc_page();
    map_page(cr3, USER_STACK_TOP - PAGE_SIZE, stack_page,
             PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    int pid = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3, USER_HEAP_BASE);
    serial_puts("[+] Init PID=1 (phys 0x800000, stack @0xBFF000, heap @0x1000000)\n");
    (void)pid;
}

int spawn_shell(void) {
    serial_puts("[*] Spawning shell process...\n");

    if (next_proc_idx >= MAX_PROCS) return -1;

    /* Allocate a 4KB page for shell code */
    uint64_t code_phys = alloc_page();
    if (!code_phys) return -1;

    copy_code(code_phys, shell_code_start, shell_code_end);

    /* Create page tables — no 2MB code page (pass 0) */
    uint64_t cr3 = create_user_pml4(next_proc_idx++, 0);
    if (!cr3) {
        free_page(code_phys);
        return -1;
    }

    /* Map shell code at USER_CODE_ADDR with a 4KB page */
    if (map_page(cr3, USER_CODE_ADDR, code_phys,
                 PTE_PRESENT | PTE_WRITABLE | PTE_USER) != 0) {
        free_page(code_phys);
        return -1;
    }

    /* Allocate and map stack page */
    uint64_t stack_page = alloc_page();
    if (!stack_page) {
        free_page(code_phys);
        return -1;
    }
    map_page(cr3, USER_STACK_TOP - PAGE_SIZE, stack_page,
             PTE_PRESENT | PTE_WRITABLE | PTE_USER);

    int pid = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3, USER_HEAP_BASE);
    serial_puts("[+] Shell process spawned\n");
    return pid;
}
