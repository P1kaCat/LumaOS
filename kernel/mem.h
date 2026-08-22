/* mem.h — Paging + heap + page allocator + dynamic mapping */
#ifndef LUMAOS_MEM_H
#define LUMAOS_MEM_H

#include <stdint.h>
#include <stddef.h>
#include "../include/handoff.h"

void paging_init(void);
void heap_init(struct lumaos_handoff *ho);
void *kmalloc(uint64_t size);
void kfree(void *ptr);

/* Phase 3: per-process page tables */
uint64_t create_user_pml4(int idx, uint64_t user_phys_addr);

/* Phase 4: page allocator (4KB physical pages) */
#define PAGE_SIZE 4096ULL
void page_allocator_init(struct lumaos_handoff *ho);
uint64_t alloc_page(void);
void free_page(uint64_t phys);
uint64_t count_free_pages(void);

/* Phase 4: dynamic page mapping (4KB pages) */

/* Page table entry flags */
#define PTE_PRESENT  0x001
#define PTE_WRITABLE 0x002
#define PTE_USER     0x004
#define PTE_PS       0x080  /* Page Size bit (2MB in PD, 1GB in PDPT) */

/* map_page: create a 4KB mapping VA → PA in the page tables pointed by cr3.
   Returns 0 on success, -1 on error (can't allocate PT, or 2MB page exists),
   -2 if already mapped. */
int map_page(uint64_t cr3, uint64_t va, uint64_t pa, uint64_t flags);

/* unmap_page: remove a 4KB mapping. Returns 0 on success, -1 if not mapped. */
int unmap_page(uint64_t cr3, uint64_t va);

/* get_page: read the PTE for va. Returns 0 if unmapped. */
uint64_t get_page(uint64_t cr3, uint64_t va);

#endif
