#ifndef LUMAOS_FRAMEBUFFER_H
#define LUMAOS_FRAMEBUFFER_H
#include "../include/handoff.h"

/* CPU rendering into the existing 32-bit GOP surface. No allocation.
 * Colors are native GOP values; rectangles use signed origins and unsigned
 * sizes. Writes are clipped to visible pixels and preserve pitch padding.
 * The caller owns the surface lifetime and serializes access. */
int fb_valid(const struct lumaos_handoff *ho);
uint32_t fb_color(uint8_t r, uint8_t g, uint8_t b, uint32_t format);
void fb_fill(struct lumaos_handoff *ho, uint32_t color);
void fb_fill_rect(struct lumaos_handoff *ho, int32_t x, int32_t y,
                  uint32_t width, uint32_t height, uint32_t color);
void fb_outline(struct lumaos_handoff *ho, int32_t x, int32_t y,
                uint32_t width, uint32_t height, uint32_t color);
/* Source and destination are clipped together. Overlap has memmove semantics. */
void fb_copy_rect(struct lumaos_handoff *ho, int32_t sx, int32_t sy,
                  int32_t dx, int32_t dy, uint32_t width, uint32_t height);
#endif
