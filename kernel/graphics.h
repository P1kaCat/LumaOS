#ifndef LUMAOS_GRAPHICS_H
#define LUMAOS_GRAPHICS_H
#include "../include/graphics_abi.h"
#include "../include/handoff.h"

void graphics_init(struct lumaos_handoff *ho);
/* Kernel callers supply valid storage; syscall boundary validates user memory. */
int graphics_get_info(struct lumaos_graphics_info *info);
/* Exactly one live Ring 3 task may own presentation at a time. */
int graphics_acquire(int pid);
int graphics_release(int pid);
/* Validate geometry independently of user-page validation. */
int graphics_validate_present(const struct lumaos_surface_present *present);
/* User rows must already have been validated readable by the syscall boundary. */
int graphics_present(const struct lumaos_surface_present *present, int pid);

#endif
