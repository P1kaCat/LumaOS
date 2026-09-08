#ifndef LUMAOS_INPUT_ABI_H
#define LUMAOS_INPUT_ABI_H
#include <stdint.h>
#define LUMAOS_SYS_INPUT_READ 17
#define LUMAOS_INPUT_KEY 1u
#define LUMAOS_INPUT_POINTER 2u
#define LUMAOS_INPUT_OVERFLOW 3u
#define LUMAOS_INPUT_DOWN 1u
#define LUMAOS_INPUT_EXTENDED 2u
#define LUMAOS_INPUT_BATCH 16u
/* Fixed 24-byte record. KEY: code=set-1 scan code, value=ASCII or zero,
 * flags=DOWN/EXTENDED. POINTER: x/y absolute in the graphics viewport,
 * code=button bits (left/right/middle). Negative y is above the viewport.
 * OVERFLOW: value=discarded records, x/y/code=latest pointer state.
 * Read returns 0 when empty, -1 on invalid buffer/count or non-owner.
 * Only the graphics owner reads input; its first valid read takes keyboard
 * focus from the shell until release/exit. No kernel pointers cross this ABI. */
struct lumaos_input_event {
    uint32_t type, code;
    int32_t x, y;
    uint32_t value, flags;
};
#endif
