#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdint.h>
#include "../include/handoff.h"
void console_init(struct lumaos_handoff *ho);
/* 8x8 ASCII cells, transparent background and clipped framebuffer writes.
 * Unsupported bytes render as '?'. Color is already in the native GOP format. */
void draw_char(struct lumaos_handoff *ho, char c, int x, int y, uint32_t color);
void draw_string(struct lumaos_handoff *ho, const char *s, int x, int y, uint32_t color);
#endif
