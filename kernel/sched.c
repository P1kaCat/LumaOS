/* sched.c — Round-robin scheduler + process abstraction + PIT */
#include "sched.h"
#include "cpu.h"

static struct task tasks[MAX_TASKS];
static int num_tasks = 0;
static int next_pid = 1;

volatile uint8_t sched_switch_pending = 0;
struct task *volatile sched_current;
struct task *volatile sched_next;

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}

void pit_init(uint32_t freq) {
    uint16_t div = 1193182 / freq;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}

void sched_init(void) {
    tasks[0].pid = 0;
    tasks[0].id = 0;
    tasks[0].state = PROC_READY;
    tasks[0].is_user = 0;
    tasks[0].rsp = 0;
    num_tasks = 1;
    sched_current = &tasks[0];
    sched_next = &tasks[0];
}

void task_create(void (*entry)(void), int id) {
    if (num_tasks >= MAX_TASKS) return;
    struct task *t = &tasks[num_tasks++];
    t->pid = id;
    t->id = id;
    t->state = PROC_READY;
    t->is_user = 0;

    uint64_t *sp = &t->stack[TASK_STACK_QWORDS];
    sp -= 22;
    for (int i = 0; i < 15; i++) sp[i] = 0;
    sp[15] = 32;
    sp[16] = 0;
    sp[17] = (uint64_t)entry;
    sp[18] = 0x08;       /* KERNEL_CS */
    sp[19] = 0x202;
    sp[20] = (uint64_t)&t->stack[TASK_STACK_QWORDS];
    sp[21] = 0x10;       /* KERNEL_DS */
    t->rsp = (uint64_t)sp;
}

int proc_create_user(uint64_t code_addr, uint64_t stack_top) {
    if (num_tasks >= MAX_TASKS) return -1;
    struct task *t = &tasks[num_tasks++];
    t->pid = next_pid++;
    t->id = t->pid;
    t->state = PROC_READY;
    t->is_user = 1;

    uint64_t *sp = &t->stack[TASK_STACK_QWORDS];
    sp -= 22;
    for (int i = 0; i < 15; i++) sp[i] = 0;
    sp[15] = 32;
    sp[16] = 0;
    sp[17] = code_addr;  /* RIP = user code */
    sp[18] = 0x1B;        /* USER_CS (Ring 3) */
    sp[19] = 0x202;
    sp[20] = stack_top;    /* RSP = user stack */
    sp[21] = 0x23;         /* USER_DS (Ring 3) */
    t->rsp = (uint64_t)sp;
    return t->pid;
}

void proc_terminate(int pid) {
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].pid == pid) {
            tasks[i].state = PROC_TERMINATED;
            return;
        }
    }
}

int proc_current_pid(void) {
    return sched_current->pid;
}

void sched_tick(void) {
    if (num_tasks < 2) return;

    int cur = 0;
    for (int i = 0; i < num_tasks; i++)
        if (&tasks[i] == sched_current) { cur = i; break; }

    /* Find next non-terminated task */
    int nxt = (cur + 1) % num_tasks;
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[nxt].state != PROC_TERMINATED) break;
        nxt = (nxt + 1) % num_tasks;
    }
    if (nxt == cur) return;  /* no other ready task */

    sched_next = &tasks[nxt];
    sched_switch_pending = 1;
}
