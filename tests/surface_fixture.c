/* Host substitutes for page allocation and per-process mappings. The registry
 * and all access/lifetime decisions under test are the real kernel/surface.c. */
#include "../kernel/mem.h"
#include "../kernel/sched.h"
#include "../kernel/surface.h"
static unsigned char arena[128][4096] __attribute__((aligned(4096)));
static unsigned used[128], mappings[4][64], flags_at[4][64];
static uint64_t mapped_physical[4][64];
uint32_t read_pixel(int pid, unsigned offset) {
    return ((uint32_t *)(uintptr_t)mapped_physical[pid-1][offset/1024])[offset%1024];
}
void write_pixel(int pid, unsigned offset, uint32_t value) {
    ((uint32_t *)(uintptr_t)mapped_physical[pid-1][offset/1024])[offset%1024] = value;
}
static struct task tasks[4];
static int allocation_budget = -1, mapping_budget = -1, errors;
void budgets(int allocation, int mapping) { allocation_budget = allocation; mapping_budget = mapping; }
int outstanding(void) {
    int n = 0;
    for (int i = 0; i < 128; i++) n += used[i];
    return n;
}
int mapping_count(int pid) {
    int n = 0;
    for (int i = 0; i < 64; i++) n += mappings[pid-1][i];
    return n;
}
int mapping_flags(int pid, unsigned slot) { return flags_at[pid-1][slot]; }
int fixture_errors(void) { return errors; }
struct task *proc_find_user(int pid) {
    if (pid < 1 || pid > 4) return 0;
    tasks[pid-1].cr3 = pid;
    return &tasks[pid-1];
}
int graphics_owner_pid(void) { return 2; }
int graphics_is_owner(int pid) { return pid == 2; }
uint64_t alloc_page(void) {
    if (!allocation_budget) return 0;
    if (allocation_budget > 0) --allocation_budget;
    for (int i = 0; i < 128; i++) if (!used[i]) {
        used[i] = 1; return (uintptr_t)arena[i];
    }
    return 0;
}
void free_page(uint64_t page) {
    for (int i = 0; i < 128; i++) if (page == (uintptr_t)arena[i]) {
        if (!used[i]) errors++;
        used[i] = 0; return;
    }
    errors++;
}
int map_page(uint64_t cr3, uint64_t va, uint64_t pa, uint64_t flags) {
    unsigned slot = (va - USER_SHARED_BASE) / PAGE_SIZE;
    if (cr3 < 1 || cr3 > 4 || slot >= 64) { errors++; return -1; }
    if (!mapping_budget) return -1;
    if (mapping_budget > 0) --mapping_budget;
    if (mappings[cr3-1][slot]) return -2;
    mappings[cr3-1][slot] = 1; flags_at[cr3-1][slot] = flags;
    mapped_physical[cr3-1][slot] = pa;
    return 0;
}
int unmap_page(uint64_t cr3, uint64_t va) {
    unsigned slot = (va - USER_SHARED_BASE) / PAGE_SIZE;
    if (cr3 < 1 || cr3 > 4 || slot >= 64) { errors++; return -1; }
    if (!mappings[cr3-1][slot]) return -1;
    mappings[cr3-1][slot] = 0; flags_at[cr3-1][slot] = 0;
    return 0;
}
void reclaim_empty_pt(uint64_t cr3, uint64_t va) { (void)cr3; (void)va; }
