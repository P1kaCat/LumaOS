/* sched.h — Minimal preemptive scheduler + PIT (Phase 0C++) */
#ifndef LUMAOS_SCHED_H
#define LUMAOS_SCHED_H

#include <stdint.h>

#define MAX_TASKS 4
#define TASK_STACK_QWORDS 2048  /* 16KB per task */

struct task {
    uint64_t rsp;
    int id;
    int active;
    uint64_t stack[TASK_STACK_QWORDS];
};

/* Globals accessed by isr.S context switch */
extern volatile uint8_t sched_switch_pending;
extern struct task *volatile sched_current;
extern struct task *volatile sched_next;

void pit_init(uint32_t freq);
void sched_init(void);
void task_create(void (*entry)(void), int id);
void sched_tick(void);

#endif
