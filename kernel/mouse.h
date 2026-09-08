#ifndef LUMAOS_MOUSE_H
#define LUMAOS_MOUSE_H
#include <stdint.h>
struct mouse_decoder { uint8_t packet[3], count; };
struct mouse_event { int32_t dx, dy; uint8_t buttons; };
/* Standard three-byte PS/2 packets; screen Y grows downward. */
int mouse_decode(struct mouse_decoder *state, uint8_t byte, struct mouse_event *event);
/* Called before STI. Failure leaves keyboard operation available. */
int ps2_mouse_init(void);
void ps2_mouse_byte(uint8_t byte);
void ps2_mouse_reset_packet(void);
#endif
