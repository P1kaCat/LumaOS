#include "graphics.h"
#include "framebuffer.h"
#include "console.h"

static struct lumaos_handoff *screen;

void graphics_init(struct lumaos_handoff *ho) {
    screen = 0;
    if (fb_valid(ho) && ho->fb_width <= INT32_MAX &&
        ho->fb_height > CONSOLE_TOP && ho->fb_height <= INT32_MAX &&
        ho->fb_format <= LUMAOS_PIXEL_BGR) screen = ho;
}

int graphics_get_info(struct lumaos_graphics_info *info) {
    if (!screen || !info) return -1;
    info->version = LUMAOS_GRAPHICS_ABI_VERSION;
    info->width = screen->fb_width;
    info->height = screen->fb_height - CONSOLE_TOP;
    info->pixel_format = screen->fb_format;
    return 0;
}
