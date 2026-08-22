/* mem.c — Paging (4-level, 2MB pages) + heap allocator + page allocator + dynamic mapping
 * Phase 4: dynamic page mapping, page cleanup on termination
 * Phase 5: debug output in free_user_pages to trace page leak
 */
#include "mem.h"

#define COM1 0x3F8
static void serial_putc(char c) { __asm__ volatile ("outb %0, %1" : : "a"(c), "dN"((uint16_t)COM1)); }
static void serial_puts(const char *s) { while (*s) { if (*s=='\n') serial_putc('\r'); serial_putc(*s++); } }

static char *uitoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0;
    while (n) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

static char *uxtoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0; const char *hex="0123456789ABCDEF";
    while (n) { tmp[i++]=hex[n&0xF]; n>>=4; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

/* ===== Paging ===== */

static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd[4][512] __attribute__((aligned(4096)));

#define MAX_PROCS 4
static uint64_t proc_pml4[MAX_PROCS][512] __attribute__((aligned(4096)));
static uint64_t proc_pdpt[MAX_PROCS][512] __attribute__((aligned(4096)));
static uint64_t proc_pd[MAX_PROCS][512] __attribute__((aligned(4096)));

void paging_init(void) {
    pml4[0] = (uint64_t)pdpt | 0x07;
    for (int i = 0; i < 4; i++) {
        pdpt[i] = (uint64_t)pd[i] | (i == 0 ? 0x07 : 0x03);
        for (int j = 0; j < 512; j++) {
            uint64_t addr = (uint64_t)i * 0x40000000ULL + (uint64_t)j * 0x200000ULL;
            pd[i][j] = addr | ((i == 0 && j == 4) ? 0x87 : 0x83);
        }
    }
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint64_t)pml4));
    serial_puts("[+] Paging: kernel supervisor-only, user @0x800000 (2MB pages)\n");
}

uint64_t create_user_pml4(int idx, uint64_t user_phys_addr) {
    if (idx < 0 || idx >= MAX_PROCS) return 0;
    proc_pml4[idx][0] = (uint64_t)proc_pdpt[idx] | 0x07;
    proc_pdpt[idx][0] = (uint64_t)proc_pd[idx] | 0x07;
    proc_pdpt[idx][1] = (uint64_t)pd[1] | 0x03;
    proc_pdpt[idx][2] = (uint64_t)pd[2] | 0x03;
    proc_pdpt[idx][3] = (uint64_t)pd[3] | 0x03;
    for (int j = 0; j < 512; j++) {
        if (j == 5 || j == 8 || j == 9) {
            proc_pd[idx][j] = 0;
        } else if (j == 4) {
            proc_pd[idx][j] = user_phys_addr | 0x87;
        } else {
            uint64_t addr = (uint64_t)j * 0x200000ULL;
            proc_pd[idx][j] = addr | 0x83;
        }
    }
    return (uint64_t)proc_pml4[idx];
}

/* ===== Heap ===== */
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
    size = (size + 15) & ~15ULL;
    if (heap_next + size > heap_base + heap_size) return (void *)0;
    void *ptr = (void *)(unsigned long)heap_next;
    heap_next += size;
    heap_allocs++;
    return ptr;
}

void kfree(void *ptr) { (void)ptr; }

/* ===== Page Allocator ===== */
#define MAX_PAGES 32768
static uint8_t page_bitmap[MAX_PAGES / 8];
static uint64_t total_free_pages = 0;

extern char _kernel_end[];

static void bm_set(uint64_t idx) { page_bitmap[idx / 8] |= (1 << (idx % 8)); }
static void bm_clear(uint64_t idx) { page_bitmap[idx / 8] &= ~(1 << (idx % 8)); }
static int bm_test(uint64_t idx) { return (page_bitmap[idx / 8] >> (idx % 8)) & 1; }

void page_allocator_init(struct lumaos_handoff *ho) {
    for (int i = 0; i < MAX_PAGES / 8; i++) page_bitmap[i] = 0xFF;
    total_free_pages = 0;
    uint8_t *mmap = (uint8_t *)(unsigned long)ho->memory_map;
    uint64_t mmap_size = ho->memory_map_size;
    uint64_t desc_size = ho->memory_map_desc_size;
    for (uint64_t off = 0; off + desc_size <= mmap_size; off += desc_size) {
        struct efi_mem_desc *d = (struct efi_mem_desc *)(mmap + off);
        if (d->type == 7) {
            uint64_t start = d->physical_start;
            uint64_t count = d->number_of_pages;
            for (uint64_t p = 0; p < count; p++) {
                uint64_t phys = start + p * PAGE_SIZE;
                uint64_t idx = phys / PAGE_SIZE;
                if (idx < MAX_PAGES) {
                    if (bm_test(idx)) { bm_clear(idx); total_free_pages++; }
                }
            }
        }
    }
    uint64_t kend = (uint64_t)_kernel_end;
    for (uint64_t phys = 0x100000; phys < kend; phys += PAGE_SIZE) {
        uint64_t idx = phys / PAGE_SIZE;
        if (idx < MAX_PAGES && !bm_test(idx)) { bm_set(idx); total_free_pages--; }
    }
    for (uint64_t phys = heap_base; phys < heap_base + heap_size; phys += PAGE_SIZE) {
        uint64_t idx = phys / PAGE_SIZE;
        if (idx < MAX_PAGES && !bm_test(idx)) { bm_set(idx); total_free_pages--; }
    }
    char buf[32];
    serial_puts("[+] Page allocator: ");
    serial_puts(uitoa(total_free_pages, buf));
    serial_puts(" free pages (4KB each)\n");
}

uint64_t alloc_page(void) {
    for (uint64_t i = 0; i < MAX_PAGES; i++) {
        if (!bm_test(i)) { bm_set(i); total_free_pages--; return i * PAGE_SIZE; }
    }
    return 0;
}

void free_page(uint64_t phys) {
    uint64_t idx = phys / PAGE_SIZE;
    if (idx < MAX_PAGES && bm_test(idx)) { bm_clear(idx); total_free_pages++; }
}

uint64_t count_free_pages(void) { return total_free_pages; }

/* ===== Dynamic Page Mapping ===== */

static uint64_t *walk_pt(uint64_t cr3, uint64_t va, int create, uint64_t flags) {
    uint64_t *table = (uint64_t *)(unsigned long)cr3;
    uint64_t idx4 = (va >> 39) & 0x1FF;
    uint64_t idx3 = (va >> 30) & 0x1FF;
    uint64_t idx2 = (va >> 21) & 0x1FF;
    uint64_t idx1 = (va >> 12) & 0x1FF;

    uint64_t entry = table[idx4];
    if (!(entry & PTE_PRESENT)) {
        if (!create) return NULL;
        uint64_t page = alloc_page();
        if (!page) return NULL;
        uint64_t *newt = (uint64_t *)(unsigned long)page;
        for (int i = 0; i < 512; i++) newt[i] = 0;
        table[idx4] = page | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
        entry = table[idx4];
    }
    table = (uint64_t *)(unsigned long)(entry & ~0xFFFULL);

    entry = table[idx3];
    if (!(entry & PTE_PRESENT)) {
        if (!create) return NULL;
        uint64_t page = alloc_page();
        if (!page) return NULL;
        uint64_t *newt = (uint64_t *)(unsigned long)page;
        for (int i = 0; i < 512; i++) newt[i] = 0;
        table[idx3] = page | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
        entry = table[idx3];
    } else if (entry & PTE_PS) {
        return NULL;
    }
    table = (uint64_t *)(unsigned long)(entry & ~0xFFFULL);

    entry = table[idx2];
    if (!(entry & PTE_PRESENT)) {
        if (!create) return NULL;
        uint64_t page = alloc_page();
        if (!page) return NULL;
        uint64_t *newt = (uint64_t *)(unsigned long)page;
        for (int i = 0; i < 512; i++) newt[i] = 0;
        table[idx2] = page | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
        entry = table[idx2];
    } else if (entry & PTE_PS) {
        return NULL;
    }
    table = (uint64_t *)(unsigned long)(entry & ~0xFFFULL);
    return &table[idx1];
}

int map_page(uint64_t cr3, uint64_t va, uint64_t pa, uint64_t flags) {
    uint64_t *pte = walk_pt(cr3, va, 1, flags);
    if (!pte) return -1;
    if (*pte & PTE_PRESENT) return -2;
    *pte = (pa & ~0xFFFULL) | (flags & 0xFFF) | PTE_PRESENT;
    return 0;
}

int unmap_page(uint64_t cr3, uint64_t va) {
    uint64_t *pte = walk_pt(cr3, va, 0, 0);
    if (!pte) return -1;
    if (!(*pte & PTE_PRESENT)) return -1;
    *pte = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
    return 0;
}

uint64_t get_page(uint64_t cr3, uint64_t va) {
    uint64_t *pte = walk_pt(cr3, va, 0, 0);
    if (!pte) return 0;
    return *pte;
}

void free_user_pages(uint64_t cr3, uint64_t va_start, uint64_t va_end) {
    uint64_t *pml4_tbl = (uint64_t *)(unsigned long)cr3;
    char buf[32];
    uint64_t freed = 0;

    serial_puts("[DBG] free_user_pages(0x");
    serial_puts(uxtoa(cr3, buf));
    serial_puts(", 0x");
    serial_puts(uxtoa(va_start, buf));
    serial_puts(", 0x");
    serial_puts(uxtoa(va_end, buf));
    serial_puts(")\n");

    for (uint64_t va = va_start; va < va_end; ) {
        uint64_t idx4 = (va >> 39) & 0x1FF;
        uint64_t idx3 = (va >> 30) & 0x1FF;
        uint64_t idx2 = (va >> 21) & 0x1FF;

        if (!(pml4_tbl[idx4] & PTE_PRESENT)) {
            serial_puts("[DBG]   PML4[");
            serial_puts(uitoa(idx4, buf));
            serial_puts("] not present — skip\n");
            va = ((va >> 39) + 1) << 39;
            continue;
        }
        uint64_t *pdpt_tbl = (uint64_t *)(unsigned long)(pml4_tbl[idx4] & ~0xFFFULL);

        if (!(pdpt_tbl[idx3] & PTE_PRESENT)) {
            serial_puts("[DBG]   PDPT[");
            serial_puts(uitoa(idx3, buf));
            serial_puts("] not present — skip\n");
            va = ((va >> 30) + 1) << 30;
            continue;
        }
        if (pdpt_tbl[idx3] & PTE_PS) {
            va = ((va >> 30) + 1) << 30;
            continue;
        }
        uint64_t *pd_tbl = (uint64_t *)(unsigned long)(pdpt_tbl[idx3] & ~0xFFFULL);

        if (!(pd_tbl[idx2] & PTE_PRESENT)) {
            serial_puts("[DBG]   PD[");
            serial_puts(uitoa(idx2, buf));
            serial_puts("] not present — skip\n");
            va = ((va >> 21) + 1) << 21;
            continue;
        }
        if (pd_tbl[idx2] & PTE_PS) {
            va = ((va >> 21) + 1) << 21;
            continue;
        }

        uint64_t pt_phys = pd_tbl[idx2] & ~0xFFFULL;
        uint64_t *pt = (uint64_t *)(unsigned long)pt_phys;

        serial_puts("[DBG]   PD[");
        serial_puts(uitoa(idx2, buf));
        serial_puts("] present, pt_phys=0x");
        serial_puts(uxtoa(pt_phys, buf));
        serial_puts("\n");

        for (int i = 0; i < 512; i++) {
            if (pt[i] & PTE_PRESENT) {
                uint64_t pa = pt[i] & ~0xFFFULL;
                serial_puts("[DBG]     PT[");
                serial_puts(uitoa(i, buf));
                serial_puts("] = 0x");
                serial_puts(uxtoa(pt[i], buf));
                serial_puts(" → free_page(0x");
                serial_puts(uxtoa(pa, buf));
                serial_puts(")\n");
                free_page(pa);
                freed++;
                pt[i] = 0;
            }
        }
        free_page(pt_phys);
        freed++;
        serial_puts("[DBG]     freed PT page 0x");
        serial_puts(uxtoa(pt_phys, buf));
        serial_puts("\n");
        pd_tbl[idx2] = 0;
        va = ((va >> 21) + 1) << 21;
    }

    serial_puts("[DBG] free_user_pages: freed ");
    serial_puts(uitoa(freed, buf));
    serial_puts(" pages\n");
}
