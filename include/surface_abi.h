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
#define LUMAOS_SYS_SURFACE_UPDATE 19
#define LUMAOS_UPDATE_COMMIT 1
#define LUMAOS_UPDATE_DAMAGE 2
#define LUMAOS_UPDATE_ACK 3
/* 32 bytes. COMMIT copies the bounded draft rectangle to published pixels.
 * DAMAGE returns 1 + union of pending rectangles, or 0 if none, and locks the
 * published view until ACK(serial). COMMIT while locked returns -2 (retry).
 * Invalid/unauthorized requests return -1 without changing state. DAMAGE and
 * ACK belong to the granted presenter; COMMIT belongs to the producer.
 * Non-input fields must be zero. Grant publishes an initial full snapshot. */
struct lumaos_surface_update {
    uint32_t op, handle, x, y, width, height, serial, reserved;
};
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
