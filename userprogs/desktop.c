#include "api.h"
/* All window policy and composition live in this isolated process. The kernel
 * knows only the granted pixel views and validated presentation rectangles. */
struct window {
    int x, y, visible;
    unsigned width, height, handle;
    const uint32_t *pixels;
};
static struct window windows[2]; /* back to front; swap on focus */
static struct lumaos_graphics_info info;
static struct lumaos_input_event events[LUMAOS_INPUT_BATCH];
static uint32_t tile[64 * 64];
#define check(ok) do { if (!(ok)) { say("[DESKTOP9] FAILED: " #ok "\n"); done(1); } } while (0)
static uint32_t color(unsigned r, unsigned g, unsigned b) {
    return info.pixel_format ? (r << 16) | (g << 8) | b : r | (g << 8) | (b << 16);
}
static int inside(struct window *w, int x, int y) {
    return w->visible && x >= w->x && y >= w->y &&
           x < w->x + (int)w->width && y < w->y + (int)w->height + 16;
}
static void render(void) {
    uint32_t background = color(18, 26, 42), title = color(62, 94, 146);
    for (unsigned y = 0; y < info.height; y += 64) {
        for (unsigned x = 0; x < info.width; x += 64) {
            unsigned width = info.width - x < 64 ? info.width - x : 64;
            unsigned height = info.height - y < 64 ? info.height - y : 64;
            for (unsigned py = 0; py < height; py++) for (unsigned px = 0; px < width; px++) {
                uint32_t pixel = background;
                for (unsigned i = 0; i < 2; i++) {
                    struct window *w = &windows[i];
                    if (!inside(w, x + px, y + py)) continue;
                    unsigned wx = x + px - w->x, wy = y + py - w->y;
                    pixel = wy < 16 ? title : w->pixels[(wy - 16) * w->width + wx];
                }
                tile[py * 64 + px] = pixel;
            }
            struct lumaos_surface_present p = {
                .pixels = (uintptr_t)tile, .surface_width = 64, .surface_height = 64,
                .stride_bytes = 256, .dst_x = x, .dst_y = y, .width = width, .height = height
            };
            check(call3(15, (uintptr_t)&p, sizeof(p), 0) == 0);
        }
    }
}
void _start(void) {
    check(call3(13, (uintptr_t)&info, sizeof(info), 0) == 0);
    check(info.width >= 400 && info.height >= 300);
    check(call3(14, 0, 0, 0) == 0);
    check(call3(17, (uintptr_t)events, LUMAOS_INPUT_BATCH, 0) >= 0);
    check(call3(12, (uintptr_t)"paint.elf", 0, 0) > 0);
    struct lumaos_surface_request r;
    for (;;) {
        r = (struct lumaos_surface_request){.op = LUMAOS_SURFACE_ENUM, .index = 0};
        if (call3(18, (uintptr_t)&r, 0, 0) == 0 && !r.peer) break;
        sleep_ticks(1);
    }
    check(r.width == 128 && r.height == 80);
    check(call3(17, r.address, 1, 0) == -1);
    windows[0] = (struct window){80, 70, 1, r.width, r.height, r.handle, (const uint32_t *)(uintptr_t)r.address};
    windows[1] = windows[0]; windows[1].x = 260; windows[1].y = 150;
    render();
    say("[DESKTOP9] ready: two windows, drag title, V visibility, X exit\n");
    int dragging = 0, offset_x = 0, offset_y = 0, was_down = 0;
    for (;;) {
        long count = call3(17, (uintptr_t)events, LUMAOS_INPUT_BATCH, 0);
        check(count >= 0);
        int dirty = 0;
        for (long i = 0; i < count; i++) {
            struct lumaos_input_event *e = &events[i];
            if (e->type == LUMAOS_INPUT_OVERFLOW) {
                dragging = 0; was_down = (e->code & 1) != 0; continue;
            }
            if (e->type == LUMAOS_INPUT_KEY && (e->flags & LUMAOS_INPUT_DOWN)) {
                if (e->value == 'v') {
                    windows[1].visible = !windows[1].visible; dirty = 1; dragging = 0;
                    say(windows[1].visible ? "[DESKTOP9] shown\n" : "[DESKTOP9] hidden\n");
                }
                if (e->value == 'x') {
                    r = (struct lumaos_surface_request){.op = LUMAOS_SURFACE_CLOSE, .handle = windows[0].handle};
                    check(call3(18, (uintptr_t)&r, 0, 0) == 0);
                    check(call3(16, 0, 0, 0) == 0);
                    say("[DESKTOP9] closed; shell restored\n"); done(0);
                }
            }
            if (e->type != LUMAOS_INPUT_POINTER) continue;
            int down = (e->code & 1) != 0;
            if (down && !was_down) {
                for (int n = 1; n >= 0; n--) if (inside(&windows[n], e->x, e->y)) {
                    struct window w = windows[n]; windows[n] = windows[1]; windows[1] = w;
                    dragging = e->y < w.y + 16;
                    offset_x = e->x - w.x; offset_y = e->y - w.y;
                    dirty = 1; break;
                }
            }
            if (down && dragging) {
                int x = e->x - offset_x, y = e->y - offset_y;
                int max_x = info.width - windows[1].width;
                int max_y = info.height - windows[1].height - 16;
                windows[1].x = x < 0 ? 0 : x > max_x ? max_x : x;
                windows[1].y = y < 0 ? 0 : y > max_y ? max_y : y;
                dirty = 1;
            }
            if (!down && dragging) { dragging = 0; say("[DESKTOP9] moved\n"); }
            was_down = down;
        }
        if (dirty) render();
        sleep_ticks(1);
    }
}
