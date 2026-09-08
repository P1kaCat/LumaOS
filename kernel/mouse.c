#include "mouse.h"

int mouse_decode(struct mouse_decoder *state, uint8_t byte, struct mouse_event *event) {
    if (!state || !event) return 0;
    if (state->count > 2) state->count = 0;
    if (!state->count && !(byte & 8)) return 0;
    state->packet[state->count++] = byte;
    if (state->count != 3) return 0;
    state->count = 0;
    uint8_t flags = state->packet[0];
    event->buttons = flags & 7;
    /* Overflow makes the displacement unreliable, but button state is useful. */
    event->dx = flags & 0x40 ? 0 : (int32_t)state->packet[1] - ((flags & 0x10) ? 256 : 0);
    event->dy = flags & 0x80 ? 0 : -((int32_t)state->packet[2] - ((flags & 0x20) ? 256 : 0));
    return 1;
}
