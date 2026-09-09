#include "api.h"
#define check(ok) do { if (!(ok)) { say("[SURFACE9] FAILED: " #ok "\n"); done(1); } } while (0)
void _start(void) {
    struct lumaos_graphics_info info;
    check(call3(13, (uintptr_t)&info, sizeof(info), 0) == 0);
    struct lumaos_surface_request r = {.op = LUMAOS_SURFACE_PRESENTER};
    check(call3(18, (uintptr_t)&r, 0, 0) == 0 && r.peer);
    unsigned peer = r.peer;
    r = (struct lumaos_surface_request){.op = LUMAOS_SURFACE_CREATE, .width = 128, .height = 80};
    check(call3(18, (uintptr_t)&r, 0, 0) == 0);
    uint32_t *pixels = (uint32_t *)(uintptr_t)r.address;
    for (unsigned y = 0; y < r.height; y++) for (unsigned x = 0; x < r.width; x++) {
        unsigned red = 40 + x, green = 80 + y, blue = 180;
        pixels[y * r.width + x] = info.pixel_format ? (red << 16) | (green << 8) | blue
                                                                : red | (green << 8) | (blue << 16);
    }
    unsigned handle = r.handle;
    r = (struct lumaos_surface_request){.op = LUMAOS_SURFACE_GRANT, .handle = handle, .peer = peer};
    check(call3(18, (uintptr_t)&r, 0, 0) == 0);
    struct lumaos_surface_update u = {.op = LUMAOS_UPDATE_COMMIT, .handle = handle, .width = 129, .height = 1};
    check(call3(19, 0x100000, sizeof(u), 0) == -1);
    check(call3(19, (uintptr_t)_start, sizeof(u), 0) == -1);
    check(call3(19, (uintptr_t)&u, 0, 0) == -1);
    check(call3(19, (uintptr_t)&u, sizeof(u), 0) == -1);
    u.width = u.height = 1;
    check(call3(19, (uintptr_t)&u, sizeof(u), 0) == 0);
    say("[UPDATE9] producer commit and invalid requests passed\n");
    say("[SURFACE9] producer granted read-only pixels and exits\n");
    done(0); /* Recipient keeps its view. */
}

