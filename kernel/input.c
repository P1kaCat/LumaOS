#include "input.h"
static struct lumaos_input_event queue[INPUT_CAPACITY], pointer;
static uint32_t first, used;
void input_reset(void) {
    first = used = 0;
    pointer = (struct lumaos_input_event){0};
}
void input_push(const struct lumaos_input_event *event) {
    if (!event) return;
    if (event->type == LUMAOS_INPUT_POINTER) pointer = *event;
    if (used == INPUT_CAPACITY) {
        first = 0;
        used = 1;
        queue[0] = pointer;
        queue[0].type = LUMAOS_INPUT_OVERFLOW;
        queue[0].value = INPUT_CAPACITY;
        queue[0].flags = 0;
    }
    queue[(first + used++) % INPUT_CAPACITY] = *event;
}
int input_read(struct lumaos_input_event *events, uint32_t count) {
    if (!events || !count || count > LUMAOS_INPUT_BATCH) return -1;
    uint32_t n = used < count ? used : count;
    for (uint32_t i = 0; i < n; i++) events[i] = queue[(first + i) % INPUT_CAPACITY];
    first = (first + n) % INPUT_CAPACITY;
    used -= n;
    return (int)n;
}
