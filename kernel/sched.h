/* sched.h — Scheduler + process abstraction (Phase 3) */
#ifndef LUMAOS_SCHED_H
#define LUMAOS_SCHED_H

#include <stdint.h>

#define MAX_TASKS 8
#define TASK_STACK_QWORDS 2048  /* 16KB per task */

#define PROC_READY      0
#define PROC_TERMINATED 2

struct task {
    uint64_t rsp;
    int pid;
    int state;
    int is_user;
    int id;
    uint64_t stack[TASK_STACK_QWORDS];
};

/* Globals accessed by isr.S context switch */
extern volatile uint8_t sched_switch_pending;
extern struct task *volatile sched_current;
extern struct task *volatile sched_next;

void pit_init(uint32_t freq);
void sched_init(void);
void task_create(void (*entry)(void), int id);
int proc_create_user(uint64_t code_addr, uint64_t stack_top);
void proc_terminate(int pid);
int proc_current_pid(void);
void sched_tick(void);

#endif
