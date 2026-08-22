/* sched.c — Round-robin preemptive scheduler + PIT timer */
#include "sched.h"

static struct task tasks[MAX_TASKS];
static int num_tasks = 0;

volatile uint8_t sched_switch_pending = 0;
struct task *volatile sched_current;
struct task *volatile sched_next;

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}

void pit_init(uint32_t freq) {
    uint16_t div = 1193182 / freq;
    outb(0x43, 0x36);              /* channel 0, mode 2, binary */
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}

void sched_init(void) {
    tasks[0].id = 0;
    tasks[0].active = 1;
    tasks[0].rsp = 0;             /* set on first switch */
    num_tasks = 1;
    sched_current = &tasks[0];
    sched_next = &tasks[0];
}

void task_create(void (*entry)(void), int id) {
    if (num_tasks >= MAX_TASKS) return;
    struct task *t = &tasks[num_tasks++];
    t->id = id;
    t->active = 1;

    /* Build initial stack frame as if just interrupted by timer */
    uint64_t *sp = &t->stack[TASK_STACK_QWORDS];
    sp -= 22;
    /* sp[0..14] = 15 GPRs (zeroed) */
    for (int i = 0; i < 15; i++) sp[i] = 0;
    /* sp[15] = int_no (32=timer), sp[16] = err_code (0) */
    sp[15] = 32;
    sp[16] = 0;
    /* sp[17..21] = RIP, CS, RFLAGS, RSP, SS (CPU-pushed frame) */
    sp[17] = (uint64_t)entry;
    sp[18] = 0x08;                /* KERNEL_CS */
    sp[19] = 0x202;               /* RFLAGS: IF set */
    sp[20] = (uint64_t)&t->stack[TASK_STACK_QWORDS];
    sp[21] = 0x10;                /* KERNEL_DS */

    t->rsp = (uint64_t)sp;
}

void sched_tick(void) {
    if (num_tasks < 2) return;

    int cur = 0;
    for (int i = 0; i < num_tasks; i++)
        if (&tasks[i] == sched_current) { cur = i; break; }

    int nxt = (cur + 1) % num_tasks;
    sched_next = &tasks[nxt];
    sched_switch_pending = 1;
}
