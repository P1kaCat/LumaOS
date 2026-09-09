#include "api.h"
#define check(ok) do { if (!(ok)) { say("[SURFACE9] FAILED: " #ok "\n"); done(1); } } while (0)
void _start(void) {
    struct lumaos_surface_request r;
    unsigned handles[4];
    check(call3(18, 0x100000, 0, 0) == -1);
    check(call3(18, (uintptr_t)_start, 0, 0) == -1);
    check(call3(18, 0xbffff8, 0, 0) == -1);
    r = (struct lumaos_surface_request){.op = 1, .width = 129, .height = 1};
    check(call3(18, (uintptr_t)&r, 0, 0) == -1);
    for (unsigned i = 0; i < 4; i++) {
        r = (struct lumaos_surface_request){.op = 1, .width = 128, .height = 128};
        check(call3(18, (uintptr_t)&r, 0, 0) == 0);
        handles[i] = r.handle;
        volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)r.address;
        check(p[0] == 0 && p[16383] == 0);
        p[16383] = 0x12345678; check(p[16383] == 0x12345678);
    }
    r = (struct lumaos_surface_request){.op = 1, .width = 1, .height = 1};
    check(call3(18, (uintptr_t)&r, 0, 0) == -1);
    r = (struct lumaos_surface_request){.op = 2, .handle = handles[0], .peer = 1};
    check(call3(18, (uintptr_t)&r, 0, 0) == -1); /* init is not presenter */
    for (unsigned i = 0; i < 4; i++) {
        r = (struct lumaos_surface_request){.op = 4, .handle = handles[i]};
        check(call3(18, (uintptr_t)&r, 0, 0) == 0);
        check(call3(18, (uintptr_t)&r, 0, 0) == -1);
    }
    check(call3(14, 0, 0, 0) == 0);
    check(call3(12, (uintptr_t)"paint.elf", 0, 0) > 0);
    for (;;) {
        r = (struct lumaos_surface_request){.op = 5, .index = 0};
        if (call3(18, (uintptr_t)&r, 0, 0) == 0 && r.peer == 0) break;
        sleep_ticks(1);
    }
    check(r.width == 128 && r.height == 80);
    check(*(volatile uint32_t *)(uintptr_t)r.address != 0);
    check(call3(17, r.address, 1, 0) == -1); /* kernel copy-out cannot write RO view */
    uint64_t read_only_address = r.address;
    unsigned handle = r.handle;
    r = (struct lumaos_surface_request){.op = 3, .handle = handles[0]};
    check(call3(18, (uintptr_t)&r, 0, 0) == -1); /* old generation */
    r = (struct lumaos_surface_request){.op = 2, .handle = handle, .peer = 1};
    check(call3(18, (uintptr_t)&r, 0, 0) == -1); /* reader cannot grant */
    say("[SURFACE9] bounds, ownership, stale handles, sharing and producer exit passed\n");
    say("[SURFACE9] read-only write probe\n");
    *(volatile uint32_t *)(uintptr_t)read_only_address = 0;
    check(0); /* Must fault in Ring 3; kernel reclaims reader and lease. */
}
