#ifndef LUMAOS_INPUT_H
#define LUMAOS_INPUT_H
#include "../include/input_abi.h"
#define INPUT_CAPACITY 64u
/* Single CPU: callers serialize through interrupt gates. */
void input_reset(void);
void input_push(const struct lumaos_input_event *event);
int input_read(struct lumaos_input_event *events, uint32_t count);
#endif
