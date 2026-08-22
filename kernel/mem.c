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

void paging_init(void) {
    pml4[0] = (uint64_t)pdpt | 0x07;  /* present, writable */

    for (int i = 0; i < 4; i++) {
        /* PDPT[1] user-accessible (contains user region), rest supervisor-only */
        pdpt[i] = (uint64_t)pd[i] | (i == 1 ? 0x07 : 0x03);
        for (int j = 0; j < 512; j++) {
            uint64_t addr = (uint64_t)i * 0x40000000ULL + (uint64_t)j * 0x200000ULL;
            /* Only 0x40000000 (PD[1][0]) is user-accessible */
            pd[i][j] = addr | ((i == 1 && j == 0) ? 0x87 : 0x83);
        }
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint64_t)pml4));
    serial_puts("[+] Paging: kernel supervisor-only, user @0x40000000 (2MB pages)\n");
}

/* ===== Heap (bump allocator) ===== */

static uint64_t heap_base = 0;
static uint64_t heap_size = 0;
static uint64_t heap_next = 0;
static uint64_t heap_allocs = 0;

struct efi_mem_desc {
    uint32_t type;
    uint32_t _pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

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
/* force rebuild */
