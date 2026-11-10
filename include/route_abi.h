#ifndef LUMAOS_ROUTE_ABI_H
#define LUMAOS_ROUTE_ABI_H
#include "input_abi.h"
#define LUMAOS_SYS_ROUTE 20
#define LUMAOS_ROUTE_READ 1
#define LUMAOS_ROUTE_SEND 2
#define LUMAOS_ROUTE_FOCUS 3
#define LUMAOS_ROUTE_CLOSE_REQUEST 4
#define LUMAOS_ROUTE_TERMINATE 5
#define LUMAOS_INPUT_CLOSE_REQUEST 5
#define LUMAOS_INPUT_FOCUS 4
#define LUMAOS_CLIENT_QUEUE 32
/* 32 bytes. READ: producer's own surface, event zero on input; returns 1/0,
 * or -1 when its presenter is gone. SEND: granted presenter only, validated
 * pointer coordinates local to content, KEY only to focused surface.
 * FOCUS: presenter selects live granted handle, or 0 to clear. Focus changes
 * enqueue FOCUS(value=0/1), atomically, or return -2 on queue pressure.
 * CLOSE_REQUEST: granted presenter enqueues a zero-payload close notification;
 * the producer decides when to close its view. Queue pressure returns -2.
 * TERMINATE: granted presenter may terminate its producer after an ignored
 * request. Payload must be zero. The reader snapshot survives until CLOSE.
 * SEND also returns -2 on pressure (no silent lost key/button transitions).
 * Invalid/unauthorized operations return -1 without consuming an event.
 * Window hit-testing/activation policy remains exclusively in Ring 3. */
struct lumaos_route {
    uint32_t op, handle;
    struct lumaos_input_event event;
};
#endif
