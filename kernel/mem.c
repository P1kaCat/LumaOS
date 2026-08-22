/* mem.c — Paging (4-level, 2MB pages) + bump heap allocator */
#include "mem.h"

#define COM1 0x3F8
static void serial_putc(char c) { __asm__ volatile ("outb %0, %1" : : "a"(c), "dN"((uint16_t)COM1)); }
static void serial_puts(const char *s) { while (*s) { if (*s=='\n') serial_putc('\r'); serial_putc(*s++); } }

static char *uitoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0;
    while (n) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0;
    return buf;
}

static char *uxtoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0; const char *hex="0123456789ABCDEF";
    while (n) { tmp[i++]=hex[n&0xF]; n>>=4; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0;
    return buf;
}

/* ===== Paging ===== */

static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd[4][512] __attribute__((aligned(4096)));  /* 4 PDs → 4GB */

/* Phase 3: per-process page tables (shared kernel PDs) */
#define MAX_PROCS 4
static uint64_t proc_pml4[MAX_PROCS][512] __attribute__((aligned(4096)));
static uint64_t proc_pdpt[MAX_PROCS][512] __attribute__((aligned(4096)));
static uint64_t proc_pd[MAX_PROCS][512] __attribute__((aligned(4096)));

void paging_init(void) {
    pml4[0] = (uint64_t)pdpt | 0x07;  /* present, writable */

    for (int i = 0; i < 4; i++) {
        /* PDPT[0] user-accessible (contains user region at 8MB), rest supervisor-only */
        pdpt[i] = (uint64_t)pd[i] | (i == 0 ? 0x07 : 0x03);
        for (int j = 0; j < 512; j++) {
            uint64_t addr = (uint64_t)i * 0x40000000ULL + (uint64_t)j * 0x200000ULL;
            /* Only 0x800000 (PD[0][4]) is user-accessible */
            pd[i][j] = addr | ((i == 0 && j == 4) ? 0x87 : 0x83);
        }
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint64_t)pml4));
    serial_puts("[+] Paging: kernel supervisor-only, user @0x800000 (2MB pages)\n");
}

/* Phase 3: create per-process page tables.
   Maps virtual 0x800000 (2MB user page) to user_phys_addr.
   Kernel space (0-1GB minus user page) is shared supervisor-only.
   Returns physical address of the new PML4. */
uint64_t create_user_pml4(int idx, uint64_t user_phys_addr) {
    if (idx < 0 || idx >= MAX_PROCS) return 0;

    /* PML4[0] → our PDPT (user-accessible so Ring 3 can traverse) */
    proc_pml4[idx][0] = (uint64_t)proc_pdpt[idx] | 0x07;

    /* PDPT[0] → our PD (user-accessible, contains user region) */
    proc_pdpt[idx][0] = (uint64_t)proc_pd[idx] | 0x07;
    /* PDPT[1-3] → shared kernel PDs (supervisor-only) */
    proc_pdpt[idx][1] = (uint64_t)pd[1] | 0x03;
    proc_pdpt[idx][2] = (uint64_t)pd[2] | 0x03;
    proc_pdpt[idx][3] = (uint64_t)pd[3] | 0x03;

    /* PD[0][0-511]: all supervisor-only except [4] = user page */
    for (int j = 0; j < 512; j++) {
        uint64_t addr = (uint64_t)j * 0x200000ULL;
        proc_pd[idx][j] = addr | ((j == 4) ? 0x87 : 0x83);
    }
    /* Override: map virtual 0x800000 to the process's physical page */
    proc_pd[idx][4] = user_phys_addr | 0x87;

    return (uint64_t)proc_pml4[idx];
}

/* ===== Heap (bump allocator) ===== */

struct efi_mem_desc {
    uint32_t type;
    uint32_t _pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

static uint64_t heap_base = 0;
static uint64_t heap_size = 0;
static uint64_t heap_next = 0;
static uint64_t heap_allocs = 0;

void heap_init(struct lumaos_handoff *ho) {
    uint8_t *mmap = (uint8_t *)(unsigned long)ho->memory_map;
    uint64_t mmap_size = ho->memory_map_size;
    uint64_t desc_size = ho->memory_map_desc_size;

    for (uint64_t off = 0; off + desc_size <= mmap_size; off += desc_size) {
        struct efi_mem_desc *d = (struct efi_mem_desc *)(mmap + off);
        if (d->type == 7 && d->physical_start >= 0x400000 && d->number_of_pages >= 512) {
            heap_base = d->physical_start;
            heap_size = d->number_of_pages * 0x1000;
            heap_next = heap_base;
            serial_puts("[+] Heap: ");
            char buf[32];
            serial_puts(uxtoa(heap_base, buf));
            serial_puts(" (");
            serial_puts(uitoa(heap_size / 1024, buf));
            serial_puts(" KB free)\n");
            return;
        }
    }
    serial_puts("[!] Heap: no conventional memory found\n");
}

void *kmalloc(uint64_t size) {
    /* Align to 16 bytes */
    size = (size + 15) & ~15ULL;
    if (heap_next + size > heap_base + heap_size) return (void *)0;
    void *ptr = (void *)(unsigned long)heap_next;
    heap_next += size;
    heap_allocs++;
    return ptr;
}

void kfree(void *ptr) {
    /* Bump allocator — no free for now */
    (void)ptr;
}
/* ===== Page Allocator (4KB pages, bitmap) ===== */
#define MAX_PAGES 32768  /* 128MB / 4KB */
static uint8_t page_bitmap[MAX_PAGES / 8];  /* 4096 bytes */
static uint64_t total_free_pages = 0;

extern char _kernel_end[];

static void bm_set(uint64_t idx) {
    page_bitmap[idx / 8] |= (1 << (idx % 8));
}

static void bm_clear(uint64_t idx) {
    page_bitmap[idx / 8] &= ~(1 << (idx % 8));
}

static int bm_test(uint64_t idx) {
    return (page_bitmap[idx / 8] >> (idx % 8)) & 1;
}

void page_allocator_init(struct lumaos_handoff *ho) {
    /* All pages initially used */
    for (int i = 0; i < MAX_PAGES / 8; i++) page_bitmap[i] = 0xFF;
    total_free_pages = 0;

    /* Walk UEFI memory map — mark conventional memory as free */
    uint8_t *mmap = (uint8_t *)(unsigned long)ho->memory_map;
    uint64_t mmap_size = ho->memory_map_size;
    uint64_t desc_size = ho->memory_map_desc_size;

    for (uint64_t off = 0; off + desc_size <= mmap_size; off += desc_size) {
        struct efi_mem_desc *d = (struct efi_mem_desc *)(mmap + off);
        if (d->type == 7) {  /* EfiConventionalMemory */
            uint64_t start = d->physical_start;
            uint64_t count = d->number_of_pages;
            for (uint64_t p = 0; p < count; p++) {
                uint64_t phys = start + p * PAGE_SIZE;
                uint64_t idx = phys / PAGE_SIZE;
                if (idx < MAX_PAGES) {
                    if (bm_test(idx)) {
                        bm_clear(idx);
                        total_free_pages++;
                    }
                }
            }
        }
    }

    /* Re-mark kernel image as used */
    uint64_t kend = (uint64_t)_kernel_end;
    for (uint64_t phys = 0x100000; phys < kend; phys += PAGE_SIZE) {
        uint64_t idx = phys / PAGE_SIZE;
        if (idx < MAX_PAGES && !bm_test(idx)) {
            bm_set(idx);
            total_free_pages--;
        }
    }

    /* Re-mark entire heap region as used */
    for (uint64_t phys = heap_base; phys < heap_base + heap_size; phys += PAGE_SIZE) {
        uint64_t idx = phys / PAGE_SIZE;
        if (idx < MAX_PAGES && !bm_test(idx)) {
            bm_set(idx);
            total_free_pages--;
        }
    }

    char buf[32];
    serial_puts("[+] Page allocator: ");
    serial_puts(uitoa(total_free_pages, buf));
    serial_puts(" free pages (4KB each)\n");
}

uint64_t alloc_page(void) {
    for (uint64_t i = 0; i < MAX_PAGES; i++) {
        if (!bm_test(i)) {
            bm_set(i);
            total_free_pages--;
            return i * PAGE_SIZE;
        }
    }
    return 0;  /* out of memory */
}

void free_page(uint64_t phys) {
    uint64_t idx = phys / PAGE_SIZE;
    if (idx < MAX_PAGES && bm_test(idx)) {
        bm_clear(idx);
        total_free_pages++;
    }
}

uint64_t count_free_pages(void) {
    return total_free_pages;
}

/* force rebuild */
