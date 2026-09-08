#include "surface.h"
#include "mem.h"
#include "sched.h"
#include "graphics.h"

#define SLOT_BYTES 65536ULL
struct surface {
    uint32_t generation, width, height, pages;
    int owner, reader;
    uint64_t physical[16];
};
static struct surface surfaces[LUMAOS_SURFACE_SLOTS];

static uint64_t address(unsigned slot) { return USER_SHARED_BASE + slot * SLOT_BYTES; }
static void unmap_view(struct surface *s, unsigned slot, int pid) {
    struct task *t = proc_find_user(pid);
    if (!t) return;
    for (unsigned i = 0; i < s->pages; i++) unmap_page(t->cr3, address(slot) + i * PAGE_SIZE);
    reclaim_empty_pt(t->cr3, address(slot));
}
static int map_view(struct surface *s, unsigned slot, int pid, int writable) {
    struct task *t = proc_find_user(pid);
    if (!t) return -1;
    unsigned i;
    for (i = 0; i < s->pages; i++) {
        if (map_page(t->cr3, address(slot) + i * PAGE_SIZE, s->physical[i],
                     PTE_USER | PTE_PRESENT | (writable ? PTE_WRITABLE : 0))) break;
    }
    if (i == s->pages) return 0;
    while (i) { --i; unmap_page(t->cr3, address(slot) + i * PAGE_SIZE); }
    reclaim_empty_pt(t->cr3, address(slot));
    return -1;
}
static void collect(struct surface *s) {
    if (s->owner || s->reader) return;
    for (unsigned i = 0; i < s->pages; i++) free_page(s->physical[i]);
    s->pages = 0;
}
void surface_cleanup(int pid) {
    for (unsigned i = 0; i < LUMAOS_SURFACE_SLOTS; i++) {
        struct surface *s = &surfaces[i];
        if (s->owner == pid || s->reader == pid) {
            unmap_view(s, i, pid);
            if (s->owner == pid) s->owner = 0;
            if (s->reader == pid) s->reader = 0;
            collect(s);
        }
    }
}
static void describe(struct lumaos_surface_request *r, struct surface *s, unsigned slot) {
    r->handle = (s->generation << 2) | slot;
    r->width = s->width; r->height = s->height; r->stride = s->width * 4;
    r->address = address(slot); r->peer = s->owner; r->index = slot;
}
int surface_request(struct lumaos_surface_request *r, int pid) {
    if (!proc_find_user(pid) || r->reserved || r->address || r->stride) return -1;
    if (r->op != LUMAOS_SURFACE_CREATE && (r->width || r->height)) return -1;
    if (r->op != LUMAOS_SURFACE_GRANT && r->peer) return -1;
    if (r->op != LUMAOS_SURFACE_ENUM && r->index) return -1;
    if (r->op == LUMAOS_SURFACE_PRESENTER) {
        if (r->handle) return -1;
        r->peer = graphics_owner_pid(); return 0;
    }
    if (r->op == LUMAOS_SURFACE_CREATE) {
        if (r->handle || !r->width || !r->height || r->width > 128 || r->height > 128) return -1;
        for (unsigned i = 0; i < LUMAOS_SURFACE_SLOTS; i++) {
            struct surface *s = &surfaces[i];
            /* Never wrap a handle generation into a stale capability. */
            if (s->pages || s->generation == 0x3fffffff) continue;
            s->width = r->width; s->height = r->height;
            unsigned count = (r->width * r->height * 4 + PAGE_SIZE - 1) / PAGE_SIZE;
            for (s->pages = 0; s->pages < count; s->pages++) {
                uint64_t page = alloc_page();
                if (!page) { collect(s); return -1; }
                s->physical[s->pages] = page;
                for (unsigned n = 0; n < PAGE_SIZE / 8; n++) ((uint64_t *)(uintptr_t)page)[n] = 0;
            }
            if (map_view(s, i, pid, 1)) { collect(s); return -1; }
            s->owner = pid; ++s->generation;
            describe(r, s, i); return 0;
        }
        return -1;
    }
    unsigned slot = r->op == LUMAOS_SURFACE_ENUM ? r->index : (r->handle & 3);
    if (slot >= LUMAOS_SURFACE_SLOTS) return -1;
    struct surface *s = &surfaces[slot];
    if (!s->pages || (s->owner != pid && s->reader != pid)) return -1;
    if (r->op == LUMAOS_SURFACE_ENUM) {
        if (r->handle) return -1;
    } else if ((r->handle >> 2) != s->generation) return -1;
    if (r->op == LUMAOS_SURFACE_GRANT) {
        if (s->owner != pid || !r->peer || r->peer == (unsigned)pid || s->reader ||
            !graphics_is_owner((int)r->peer) || map_view(s, slot, r->peer, 0)) return -1;
        s->reader = r->peer;
    } else if (r->op == LUMAOS_SURFACE_CLOSE) {
        unmap_view(s, slot, pid);
        if (s->owner == pid) s->owner = 0;
        if (s->reader == pid) s->reader = 0;
        collect(s); return 0;
    } else if (r->op != LUMAOS_SURFACE_INFO && r->op != LUMAOS_SURFACE_ENUM) return -1;
    describe(r, s, slot); return 0;
}
