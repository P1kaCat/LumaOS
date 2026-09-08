#include "framebuffer.h"

int fb_valid(const struct lumaos_handoff *ho) {
    if (!ho || !ho->framebuffer || (ho->framebuffer & 3) || ho->fb_bpp != 32 ||
        !ho->fb_width || !ho->fb_height || (ho->fb_pitch & 3) ||
        ho->fb_pitch / 4 < ho->fb_width) return 0;
    uint64_t bytes = (uint64_t)(ho->fb_height - 1) * ho->fb_pitch +
                     (uint64_t)ho->fb_width * 4;
    return ho->framebuffer <= UINT64_MAX - bytes;
}

uint32_t fb_color(uint8_t r, uint8_t g, uint8_t b, uint32_t format) {
    if (format == LUMAOS_PIXEL_BGR)
        return (uint32_t)b | ((uint32_t)g << 8) | ((uint32_t)r << 16);
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

/* int64_t keeps origin + size and outline endpoints safe at integer limits. */
static void rect(struct lumaos_handoff *ho, int64_t x, int64_t y,
                 uint32_t width, uint32_t height, uint32_t color) {
    int64_t right = x + width, bottom = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > ho->fb_width) right = ho->fb_width;
    if (bottom > ho->fb_height) bottom = ho->fb_height;
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)ho->framebuffer;
    uint32_t stride = ho->fb_pitch / 4;
    for (int64_t py = y; py < bottom; py++)
        for (int64_t px = x; px < right; px++)
            fb[(uint64_t)py * stride + (uint64_t)px] = color;
}

void fb_fill_rect(struct lumaos_handoff *ho, int32_t x, int32_t y,
                  uint32_t width, uint32_t height, uint32_t color) {
    if (fb_valid(ho)) rect(ho, x, y, width, height, color);
}

void fb_fill(struct lumaos_handoff *ho, uint32_t color) {
    if (fb_valid(ho)) rect(ho, 0, 0, ho->fb_width, ho->fb_height, color);
}

void fb_outline(struct lumaos_handoff *ho, int32_t x, int32_t y,
                uint32_t width, uint32_t height, uint32_t color) {
    if (!fb_valid(ho) || !width || !height) return;
    rect(ho, x, y, width, 1, color);
    rect(ho, x, (int64_t)y + height - 1, width, 1, color);
    rect(ho, x, y, 1, height, color);
    rect(ho, (int64_t)x + width - 1, y, 1, height, color);
}

static void copy_bounds(int32_t source, int32_t dest, uint32_t size,
                        uint32_t limit, int64_t *first, int64_t *last) {
    *first = 0;
    if (-(int64_t)source > *first) *first = -(int64_t)source;
    if (-(int64_t)dest > *first) *first = -(int64_t)dest;
    *last = size;
    if ((int64_t)limit - source < *last) *last = (int64_t)limit - source;
    if ((int64_t)limit - dest < *last) *last = (int64_t)limit - dest;
}

void fb_copy_rect(struct lumaos_handoff *ho, int32_t sx, int32_t sy,
                  int32_t dx, int32_t dy, uint32_t width, uint32_t height) {
    if (!fb_valid(ho)) return;
    int64_t left, right, top, bottom;
    copy_bounds(sx, dx, width, ho->fb_width, &left, &right);
    copy_bounds(sy, dy, height, ho->fb_height, &top, &bottom);
    if (left >= right || top >= bottom) return;
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)ho->framebuffer;
    uint32_t stride = ho->fb_pitch / 4;
    for (int64_t iy = top; iy < bottom; iy++) {
        int64_t y = dy > sy ? bottom - 1 - (iy - top) : iy;
        for (int64_t ix = left; ix < right; ix++) {
            int64_t x = dx > sx ? right - 1 - (ix - left) : ix;
            fb[(uint64_t)(dy + y) * stride + (uint64_t)(dx + x)] =
                fb[(uint64_t)(sy + y) * stride + (uint64_t)(sx + x)];
        }
    }
}
