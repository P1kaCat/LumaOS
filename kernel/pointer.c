#include "pointer.h"
#include "framebuffer.h"

static struct lumaos_handoff *surface;
static uint32_t saved[12 * 16];
static int32_t x, y;
static int visible;
static uint8_t pressed;

void pointer_hide(void) {
    if (!surface || !visible) return;
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)surface->framebuffer;
    uint32_t stride = surface->fb_pitch / 4;
    for (uint32_t py = 0; py < 16 && (uint32_t)y + py < surface->fb_height; py++)
        for (uint32_t px = 0; px < 12 && (uint32_t)x + px < surface->fb_width; px++)
            fb[((uint64_t)y + py) * stride + x + px] = saved[py * 12 + px];
    visible = 0;
}

void pointer_show(void) {
    if (!surface || visible) return;
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)surface->framebuffer;
    uint32_t stride = surface->fb_pitch / 4;
    for (uint32_t py = 0; py < 16 && (uint32_t)y + py < surface->fb_height; py++) {
        for (uint32_t px = 0; px < 12 && (uint32_t)x + px < surface->fb_width; px++) {
            uint64_t offset = ((uint64_t)y + py) * stride + x + px;
            saved[py * 12 + px] = fb[offset];
            /* Small arrow with a dark outline; inverted while a button is held. */
            int body = py < 12 ? px <= py : (px >= 3 && px <= 5);
            if (body) {
                int edge = !px || px == py || py == 11 || py == 15 ||
                           (py >= 12 && (px == 3 || px == 5));
                fb[offset] = (edge != !!pressed) ? 0 : 0xFFFFFF;
            }
        }
    }
    visible = 1;
}

void pointer_init(struct lumaos_handoff *ho) {
    pointer_hide();
    surface = 0;
    if (!fb_valid(ho) || ho->fb_width > INT32_MAX || ho->fb_height > INT32_MAX) return;
    surface = ho;
    x = (int32_t)(ho->fb_width / 2);
    y = (int32_t)(ho->fb_height / 2);
    pressed = 0;
    pointer_show();
}

void pointer_move(int32_t dx, int32_t dy, uint8_t buttons) {
    if (!surface) return;
    pointer_hide();
    int64_t nx = (int64_t)x + dx, ny = (int64_t)y + dy;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx >= surface->fb_width) nx = surface->fb_width - 1;
    if (ny >= surface->fb_height) ny = surface->fb_height - 1;
    x = (int32_t)nx;
    y = (int32_t)ny;
    pressed = buttons & 7;
    pointer_show();
}

void pointer_position(int32_t *px, int32_t *py) { *px = x; *py = y; }
