#ifndef LUMAOS_SURFACE_H
#define LUMAOS_SURFACE_H
#include "../include/surface_abi.h"
#define USER_SHARED_BASE 0x3fe00000ULL
#define USER_SHARED_END  0x40000000ULL
int surface_request(struct lumaos_surface_request *r, int pid);
int surface_update(struct lumaos_surface_update *r, int pid);
void surface_cleanup(int pid);
#endif
