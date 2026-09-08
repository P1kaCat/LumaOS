#ifndef LUMAOS_GRAPHICS_H
#define LUMAOS_GRAPHICS_H
#include "../include/graphics_abi.h"
#include "../include/handoff.h"
void graphics_init(struct lumaos_handoff *ho);
/* Kernel callers supply valid storage; syscall boundary validates user memory. */
int graphics_get_info(struct lumaos_graphics_info *info);
#endif
