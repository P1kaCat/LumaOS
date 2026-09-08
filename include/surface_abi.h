#ifndef LUMAOS_SURFACE_ABI_H
#define LUMAOS_SURFACE_ABI_H
#include <stdint.h>
#define LUMAOS_SYS_SURFACE 18
#define LUMAOS_SURFACE_CREATE 1
#define LUMAOS_SURFACE_GRANT 2
#define LUMAOS_SURFACE_INFO 3
#define LUMAOS_SURFACE_CLOSE 4
#define LUMAOS_SURFACE_ENUM 5
#define LUMAOS_SURFACE_PRESENTER 6
#define LUMAOS_SURFACE_SLOTS 4
#define LUMAOS_SURFACE_MAX_SIDE 128
/* Fixed 40-byte in/out request. CREATE uses width/height; GRANT handle/peer;
 * INFO/CLOSE handle; ENUM index. Other input fields must be zero.
 * ENUM returns only this process's views. Output address is a user VA, never
 * a physical address. Producer is RW; the one explicitly granted presenter
 * is read-only. Closing/exiting drops a view; pixels live until the last view.
 * Handles carry a generation; dimensions/stride cannot change after creation. */
struct lumaos_surface_request {
    uint32_t op, handle, width, height, stride, peer, index, reserved;
    uint64_t address;
};
#endif
