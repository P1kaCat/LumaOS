#include "api.h"
static struct lumaos_input_event events[LUMAOS_INPUT_BATCH];
static void check(int ok) { if (!ok) { say("[INPUT9] FAILED\n"); done(1); } }
void _start(void) {
    check(call3(17, (uintptr_t)events, 1, 0) == -1); /* no ownership */
    check(call3(14, 0, 0, 0) == 0);
    check(call3(17, 0x100000, 1, 0) == -1);
    check(call3(17, (uintptr_t)_start, 1, 0) == -1); /* read-only text */
    check(call3(17, 0xBFFFF8, 1, 0) == -1);
    check(call3(17, (uintptr_t)events, 0, 0) == -1);
    check(call3(17, (uintptr_t)events, 17, 0) == -1);
    while (call3(17, (uintptr_t)events, LUMAOS_INPUT_BATCH, 0) > 0) {}
    check(call3(17, (uintptr_t)events, 1, 0) == 0);
    say("[INPUT9] ready\n");
    unsigned seen = 0;
    for (;;) {
        long count = call3(17, (uintptr_t)events, LUMAOS_INPUT_BATCH, 0);
        check(count >= 0 && count <= LUMAOS_INPUT_BATCH);
        for (long i = 0; i < count; i++) {
            struct lumaos_input_event *e = &events[i];
            if (e->type == LUMAOS_INPUT_KEY && e->value == 'h')
                seen |= (e->flags & LUMAOS_INPUT_DOWN) ? 1 : 2;
            if (e->type == LUMAOS_INPUT_POINTER) {
                seen |= 4;
                if (e->code & 1) seen |= 8;
                else if (seen & 8) seen |= 16;
            }
            if (e->type == LUMAOS_INPUT_KEY && e->value == 'x' && (e->flags & LUMAOS_INPUT_DOWN)) {
                check(seen == 31);
                check(call3(16, 0, 0, 0) == 0);
                check(call3(17, (uintptr_t)events, 1, 0) == -1);
                say("[INPUT9] keys, pointer, buttons, empty read and invalid buffers passed\n");
                done(0);
            }
        }
        sleep_ticks(1);
    }
}
