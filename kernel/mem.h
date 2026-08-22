/* mem.h — Paging + heap allocator (Phase 0C+) */
#ifndef LUMAOS_MEM_H
#define LUMAOS_MEM_H

#include <stdint.h>
#include "handoff.h"

void paging_init(void);
void heap_init(struct lumaos_handoff *ho);
void *kmalloc(uint64_t size);
void kfree(void *ptr);  /* no-op for now (bump allocator) */

#endif
