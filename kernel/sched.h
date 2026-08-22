/* sched.h — Scheduler + process abstraction (Phase 3 + 4 + 5) */
#ifndef LUMAOS_SCHED_H
#define LUMAOS_SCHED_H

#include <stdint.h>

#define MAX_TASKS 8
#define TASK_STACK_QWORDS 2048  /* 16KB per task */

#define PROC_READY      0
#define PROC_TERMINATED  2
#define SLEEPING         3      /* Phase 5: sleeping state */

/* Phase 4: User memory regions (virtual addresses) */
#define USER_STACK_BASE   0xA00000ULL
#define USER_STACK_TOP    0xC00000ULL
#define USER_HEAP_BASE    0x1000000ULL
#define USER_HEAP_MAX     0x1400000ULL

struct task {
    uint64_t rsp;         /* offset 0  — saved RSP for context switch */
    int pid;              /* offset 8  */
    int state;            /* offset 12 */
    int is_user;          /* offset 16 — Ring 3 task? */
    int id;               /* offset 20 */
    uint64_t kernel_rsp;  /* offset 24 — per-process kernel stack top (TSS RSP0) */
    uint64_t cr3;         /* offset 32 — per-process page tables (0 = kernel) */
    /* Phase 4: user memory management */
    uint64_t user_stack_top;   /* offset 40 — top of stack (initial RSP) */
    uint64_t user_stack_limit; /* offset 48 — lowest mapped stack page */
    uint64_t user_heap_base;   /* offset 56 — heap region start */
    uint64_t user_heap_limit;  /* offset 64 — current brk (sbrk limit) */
    /* Phase 5: sleep support */
    uint64_t sleep_until;      /* offset 72 — system_ticks when task wakes */
    uint64_t stack[TASK_STACK_QWORDS]; /* offset 80 — kernel stack */
};

/* Globals accessed by isr.S context switch */
extern volatile uint8_t sched_switch_pending;
extern struct task *volatile sched_current;
extern struct task *volatile sched_next;
extern volatile uint64_t system_ticks;  /* Phase 5: global tick counter */

void pit_init(uint32_t freq);
void sched_init(void);
void task_create(void (*entry)(void), int id);
int proc_create_user(uint64_t code_addr, uint64_t stack_top, uint64_t cr3, uint64_t heap_base);
void proc_terminate(int pid);
int proc_current_pid(void);
void sched_tick(void);
int count_active_user_procs(void);
void sched_yield(void);  /* Phase 5: yield CPU to next READY task */

#endif
