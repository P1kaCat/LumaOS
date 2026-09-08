#ifndef LUMAOS_POINTER_H
#define LUMAOS_POINTER_H
#include "../include/handoff.h"
/* Serialized with console writes by interrupt gates; no allocation or syscall. */
void pointer_init(struct lumaos_handoff *ho);
void pointer_hide(void);
void pointer_show(void);
void pointer_move(int32_t dx, int32_t dy, uint8_t buttons);
void pointer_position(int32_t *px, int32_t *py);
#endif
