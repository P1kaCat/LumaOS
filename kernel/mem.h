/* mem.h — Paging + heap allocator */
#ifndef LUMAOS_MEM_H
#define LUMAOS_MEM_H

#include <stdint.h>
#include "../include/handoff.h"

void paging_init(void);
void heap_init(struct lumaos_handoff *ho);
void *kmalloc(uint64_t size);
void kfree(void *ptr);

/* Phase 3: per-process page tables */
uint64_t create_user_pml4(int idx, uint64_t user_phys_addr);

#endif
