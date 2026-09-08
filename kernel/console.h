#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdint.h>
#include "../include/handoff.h"
#define CONSOLE_CELL_WIDTH 8u
#define CONSOLE_CELL_HEIGHT 8u
#define CONSOLE_TOP 40u
/* Single text console below the existing boot title. No allocation.
 * Writes must be serialized by the caller (syscall interrupt gate in kernel). */
void console_init(struct lumaos_handoff *ho);
void console_clear(void);
void console_graphics_mode(int enabled);
void console_write(const char *s);
/* 8x8 ASCII cells, transparent background and clipped framebuffer writes.
 * Unsupported bytes render as '?'. Color is already in the native GOP format. */
void draw_char(struct lumaos_handoff *ho, char c, int x, int y, uint32_t color);
void draw_string(struct lumaos_handoff *ho, const char *s, int x, int y, uint32_t color);
#endif
