#ifndef LUMAOS_GRAPHICS_ABI_H
#define LUMAOS_GRAPHICS_ABI_H
#include <stdint.h>

#define LUMAOS_SYS_GRAPHICS_INFO     13
#define LUMAOS_SYS_GRAPHICS_ACQUIRE  14
#define LUMAOS_SYS_GRAPHICS_PRESENT  15
#define LUMAOS_SYS_GRAPHICS_RELEASE  16

#define LUMAOS_GRAPHICS_ABI_VERSION 2

/* The user graphics area lies below the kernel's existing title.
 * No kernel address or framebuffer mapping is exposed. Format uses the GOP
 * RGB/BGR byte order constants in handoff.h (0 = RGB, 1 = BGR).
 *
 * User surfaces stay in the owning Ring 3 process. The compositor submits a
 * bounded rectangle and the kernel validates/copies it into the GOP surface.
 * Pixel values use the native 32-bit format returned by GRAPHICS_INFO. */
struct lumaos_graphics_info {
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
};

/* 48-byte fixed ABI. All dimensions are pixels; stride is bytes per row.
 * reserved must be zero. The complete source and destination rectangles must
 * fit their respective surfaces: presentation never clips or wraps. */
struct lumaos_surface_present {
    uint64_t pixels;
    uint32_t surface_width;
    uint32_t surface_height;
    uint32_t stride_bytes;
    uint32_t src_x;
    uint32_t src_y;
    int32_t dst_x;
    int32_t dst_y;
    uint32_t width;
    uint32_t height;
    uint32_t reserved;
};

#endif
