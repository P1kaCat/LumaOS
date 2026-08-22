/* sched.c — Round-robin scheduler + process abstraction + PIT
 * Phase 3: basic process support
 * Phase 4: user memory regions, page cleanup on termination
 * Phase 5: sleep support, yield, system_ticks
 */
#include "sched.h"
#include "cpu.h"
#include "mem.h"

static struct task tasks[MAX_TASKS];
static int num_tasks = 0;
static int next_pid = 1;

volatile uint8_t sched_switch_pending = 0;
struct task *volatile sched_current;
struct task *volatile sched_next;
volatile uint64_t system_ticks = 0;

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
    tasks[0].kernel_rsp = 0;
    tasks[0].cr3 = 0;
    tasks[0].user_stack_top = 0;
    tasks[0].user_stack_limit = 0;
    tasks[0].user_heap_base = 0;
    tasks[0].user_heap_limit = 0;
    tasks[0].sleep_until = 0;
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
    t->kernel_rsp = 0;
    t->cr3 = 0;
    t->user_stack_top = 0;
    t->user_stack_limit = 0;
    t->user_heap_base = 0;
    t->user_heap_limit = 0;
    t->sleep_until = 0;

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

int proc_create_user(uint64_t code_addr, uint64_t stack_top, uint64_t cr3, uint64_t heap_base) {
    if (num_tasks >= MAX_TASKS) return -1;
    struct task *t = &tasks[num_tasks++];
    t->pid = next_pid++;
    t->id = t->pid;
    t->state = PROC_READY;
    t->is_user = 1;
    t->kernel_rsp = (uint64_t)&t->stack[TASK_STACK_QWORDS];
    t->cr3 = cr3;
    t->user_stack_top = stack_top;
    t->user_stack_limit = stack_top - PAGE_SIZE;
    t->user_heap_base = heap_base;
    t->user_heap_limit = heap_base;
    t->sleep_until = 0;

    uint64_t *sp = &t->stack[TASK_STACK_QWORDS];
    sp -= 22;
    for (int i = 0; i < 15; i++) sp[i] = 0;
    sp[15] = 32;
    sp[16] = 0;
    sp[17] = code_addr;
    sp[18] = 0x1B;        /* USER_CS (Ring 3) */
    sp[19] = 0x202;
    sp[20] = stack_top;
    sp[21] = 0x23;         /* USER_DS (Ring 3) */
    t->rsp = (uint64_t)sp;
    return t->pid;
}

void proc_terminate(int pid) {
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].pid == pid && tasks[i].is_user) {
            tasks[i].state = PROC_TERMINATED;
            if (tasks[i].cr3) {
                free_user_pages(tasks[i].cr3,
                    USER_STACK_BASE, tasks[i].user_stack_top);
                free_user_pages(tasks[i].cr3,
                    USER_HEAP_BASE, USER_HEAP_MAX);
            }
            return;
        }
    }
}

int proc_current_pid(void) {
    return sched_current->pid;
}

int count_active_user_procs(void) {
    int count = 0;
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == PROC_READY && tasks[i].is_user)
            count++;
    }
    return count;
}

/* Find next READY task, skip TERMINATED and SLEEPING.
   Shared logic for sched_tick and sched_yield. */
static int find_next_ready(int cur) {
    int nxt = (cur + 1) % num_tasks;
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[nxt].state == PROC_READY) break;
        nxt = (nxt + 1) % num_tasks;
    }
    return nxt;
}

void sched_tick(void) {
    system_ticks++;

    /* Wake up sleeping tasks whose timer has expired */
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == SLEEPING && system_ticks >= tasks[i].sleep_until) {
            tasks[i].state = PROC_READY;
        }
    }

    if (num_tasks < 2) return;

    int cur = 0;
    for (int i = 0; i < num_tasks; i++)
        if (&tasks[i] == sched_current) { cur = i; break; }

    int nxt = find_next_ready(cur);
    if (nxt == cur) return;

    sched_next = &tasks[nxt];
    sched_switch_pending = 1;
}

void sched_yield(void) {
    if (num_tasks < 2) return;

    int cur = 0;
    for (int i = 0; i < num_tasks; i++)
        if (&tasks[i] == sched_current) { cur = i; break; }

    int nxt = find_next_ready(cur);
    if (nxt == cur) return;

    sched_next = &tasks[nxt];
    sched_switch_pending = 1;
}
