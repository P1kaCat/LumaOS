#ifndef LUMAOS_GRAPHICS_ABI_H
#define LUMAOS_GRAPHICS_ABI_H
#include <stdint.h>
#define LUMAOS_SYS_GRAPHICS_INFO 13
#define LUMAOS_GRAPHICS_ABI_VERSION 1
/* The user graphics area lies below the kernel's existing title.
 * No kernel address or framebuffer mapping is exposed. Format uses the GOP
 * RGB/BGR byte order constants in handoff.h (0 = RGB, 1 = BGR). */
struct lumaos_graphics_info {
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
};
#endif
