#include "surface.h"
#include "mem.h"
#include "sched.h"
#include "graphics.h"

#define SLOT_BYTES 65536ULL
struct surface {
    uint32_t generation, width, height, pages;
    int owner, reader;
    uint64_t physical[16];
    uint64_t published[16];
    uint32_t serial, pending, locked, x, y, right, bottom;
    struct lumaos_input_event events[LUMAOS_CLIENT_QUEUE];
    unsigned first, used;
};
static struct surface surfaces[LUMAOS_SURFACE_SLOTS];
static uint32_t focused_handle;

static void reset_route(struct surface *s, unsigned slot) {
    if (focused_handle == ((s->generation << 2) | slot)) focused_handle = 0;
    s->first = s->used = 0;
}

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
        if (map_page(t->cr3, address(slot) + i * PAGE_SIZE, writable ? s->physical[i] : s->published[i],
                     PTE_USER | PTE_PRESENT | (writable ? PTE_WRITABLE : 0))) break;
    }
    if (i == s->pages) return 0;
    while (i) { --i; unmap_page(t->cr3, address(slot) + i * PAGE_SIZE); }
    reclaim_empty_pt(t->cr3, address(slot));
    return -1;
}
static void collect(struct surface *s) {
    if (s->owner || s->reader) return;
    for (unsigned i = 0; i < s->pages; i++) {
        free_page(s->physical[i]); free_page(s->published[i]);
    }
    s->pages = 0;
}
void surface_cleanup(int pid) {
    for (unsigned i = 0; i < LUMAOS_SURFACE_SLOTS; i++) {
        struct surface *s = &surfaces[i];
        if (s->owner == pid || s->reader == pid) {
            reset_route(s, i);
            unmap_view(s, i, pid);
            if (s->owner == pid) s->owner = 0;
            if (s->reader == pid) { s->reader = 0; s->locked = 0; }
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
            s->serial = s->pending = s->locked = 0;
            s->first = s->used = 0;
            unsigned count = (r->width * r->height * 4 + PAGE_SIZE - 1) / PAGE_SIZE;
            for (s->pages = 0; s->pages < count; s->pages++) {
                uint64_t page = alloc_page();
                if (!page) { collect(s); return -1; }
                uint64_t published = alloc_page();
                if (!published) { free_page(page); collect(s); return -1; }
                s->physical[s->pages] = page;
                s->published[s->pages] = published;
                for (unsigned n = 0; n < PAGE_SIZE / 8; n++) {
                    ((uint64_t *)(uintptr_t)page)[n] = 0;
                    ((uint64_t *)(uintptr_t)published)[n] = 0;
                }
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
        for (unsigned i = 0; i < s->pages; i++)
            for (unsigned n = 0; n < PAGE_SIZE / 8; n++)
                ((uint64_t *)(uintptr_t)s->published[i])[n] = ((uint64_t *)(uintptr_t)s->physical[i])[n];
        s->serial = 1; s->pending = 1; s->locked = 0;
        s->x = s->y = 0; s->right = s->width; s->bottom = s->height;
    } else if (r->op == LUMAOS_SURFACE_CLOSE) {
        reset_route(s, slot);
        unmap_view(s, slot, pid);
        if (s->owner == pid) s->owner = 0;
        if (s->reader == pid) { s->reader = 0; s->locked = 0; }
        collect(s); return 0;
    } else if (r->op != LUMAOS_SURFACE_INFO && r->op != LUMAOS_SURFACE_ENUM) return -1;
    describe(r, s, slot); return 0;
}

static struct surface *from_handle(unsigned handle) {
    struct surface *s = &surfaces[handle & 3];
    return s->pages && (handle >> 2) == s->generation ? s : 0;
}
static int empty_event(const struct lumaos_input_event *e) {
    return !(e->type || e->code || e->x || e->y || e->value || e->flags);
}
static void enqueue(struct surface *s, struct lumaos_input_event e) {
    s->events[(s->first+s->used++) % LUMAOS_CLIENT_QUEUE] = e;
}
int surface_route(struct lumaos_route *r, int pid) {
    if (!proc_find_user(pid)) return -1;
    struct surface *s = from_handle(r->handle);
    if (r->op == LUMAOS_ROUTE_FOCUS) {
        if (!graphics_is_owner(pid) || !empty_event(&r->event)) return -1;
        if (r->handle && (!s || s->reader != pid || !s->owner)) return -1;
        struct surface *old = from_handle(focused_handle);
        if (!old || old->reader != pid || !old->owner) { old = 0; focused_handle = 0; }
        if (focused_handle == r->handle) return 0;
        if ((old && old->used == LUMAOS_CLIENT_QUEUE) || (s && s->used == LUMAOS_CLIENT_QUEUE)) return -2;
        if (old) enqueue(old, (struct lumaos_input_event){.type=LUMAOS_INPUT_FOCUS,.value=0});
        if (s) enqueue(s, (struct lumaos_input_event){.type=LUMAOS_INPUT_FOCUS,.value=1});
        focused_handle = r->handle; return 0;
    }
    if (!s) return -1;
    if (r->op == LUMAOS_ROUTE_READ) {
        if (s->owner != pid || !empty_event(&r->event) || !s->reader || !graphics_is_owner(s->reader)) return -1;
        if (!s->used) return 0;
        r->event = s->events[s->first];
        s->first = (s->first+1) % LUMAOS_CLIENT_QUEUE; --s->used; return 1;
    }
    if (r->op != LUMAOS_ROUTE_SEND || s->reader != pid || !s->owner || !graphics_is_owner(pid)) return -1;
    struct lumaos_input_event *e = &r->event;
    if (e->type == LUMAOS_INPUT_KEY) {
        if (focused_handle != r->handle || e->x || e->y || e->code > 127 || e->value > 127 ||
            (e->flags & ~(LUMAOS_INPUT_DOWN|LUMAOS_INPUT_EXTENDED))) return -1;
    } else if (e->type == LUMAOS_INPUT_POINTER) {
        if (e->code > 7 || e->flags || e->value || e->x < 0 || e->y < 0 ||
            (unsigned)e->x >= s->width || (unsigned)e->y >= s->height) return -1;
    } else return -1;
    if (s->used == LUMAOS_CLIENT_QUEUE) return -2;
    enqueue(s, *e); return 0;
}

int surface_update(struct lumaos_surface_update *r, int pid) {
    struct surface *s = &surfaces[r->handle & 3];
    if (!proc_find_user(pid) || !s->pages || (r->handle >> 2) != s->generation || r->reserved) return -1;
    if (r->op == LUMAOS_UPDATE_COMMIT) {
        if (s->owner != pid || r->serial || !r->width || !r->height ||
            (uint64_t)r->x + r->width > s->width || (uint64_t)r->y + r->height > s->height) return -1;
        if (s->locked) return -2;
        if (s->serial == UINT32_MAX) return -1;
        for (unsigned y = r->y; y < r->y + r->height; y++)
            for (unsigned x = r->x; x < r->x + r->width; x++) {
                unsigned offset = (y * s->width + x) * 4, page = offset / PAGE_SIZE;
                unsigned word = (offset % PAGE_SIZE) / 4;
                ((uint32_t *)(uintptr_t)s->published[page])[word] = ((uint32_t *)(uintptr_t)s->physical[page])[word];
            }
        if (!s->pending) { s->x = r->x; s->y = r->y; s->right = r->x+r->width; s->bottom = r->y+r->height; }
        else {
            if (r->x < s->x) s->x = r->x;
            if (r->y < s->y) s->y = r->y;
            if (r->x+r->width > s->right) s->right = r->x+r->width;
            if (r->y+r->height > s->bottom) s->bottom = r->y+r->height;
        }
        s->pending = 1; r->serial = ++s->serial; return 0;
    }
    if (s->reader != pid || !graphics_is_owner(pid) || r->x || r->y || r->width || r->height) return -1;
    if (r->op == LUMAOS_UPDATE_DAMAGE) {
        if (r->serial) return -1;
        if (!s->pending) return 0;
        s->locked = 1;
        r->x = s->x; r->y = s->y; r->width = s->right-s->x; r->height = s->bottom-s->y;
        r->serial = s->serial; return 1;
    }
    if (r->op == LUMAOS_UPDATE_ACK && s->locked && r->serial == s->serial) {
        s->locked = s->pending = 0; return 0;
    }
    return -1;
}
