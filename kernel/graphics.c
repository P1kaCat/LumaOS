#include "graphics.h"
#include "framebuffer.h"
#include "console.h"
#include "pointer.h"
#include "sched.h"
#include <stdint.h>

static struct lumaos_handoff *screen;
static struct task *owner_task;
static int owner_pid;

static void drop_stale_owner(void) {
    if (!owner_task) return;
    if (!owner_task->is_user || owner_task->state == PROC_TERMINATED ||
        owner_task->pid != owner_pid) {
        owner_task = 0;
        owner_pid = 0;
    }
}

void graphics_init(struct lumaos_handoff *ho) {
    screen = 0;
    owner_task = 0;
    owner_pid = 0;
    if (fb_valid(ho) && ho->fb_width <= INT32_MAX &&
        ho->fb_height > CONSOLE_TOP && ho->fb_height <= INT32_MAX &&
        ho->fb_format <= LUMAOS_PIXEL_BGR) screen = ho;
}

int graphics_get_info(struct lumaos_graphics_info *info) {
    if (!screen || !info) return -1;
    info->version = LUMAOS_GRAPHICS_ABI_VERSION;
    info->width = screen->fb_width;
    info->height = screen->fb_height - CONSOLE_TOP;
    info->pixel_format = screen->fb_format;
    return 0;
}

int graphics_acquire(int pid) {
    if (!screen || !sched_current || !sched_current->is_user ||
        sched_current->pid != pid) return -1;
    drop_stale_owner();
    if (!owner_task) {
        owner_task = (struct task *)sched_current;
        owner_pid = pid;
        return 0;
    }
    return (owner_task == sched_current && owner_pid == pid) ? 0 : -1;
}

int graphics_release(int pid) {
    drop_stale_owner();
    if (!owner_task || owner_task != sched_current || owner_pid != pid)
        return -1;
    owner_task = 0;
    owner_pid = 0;
    return 0;
}

int graphics_validate_present(const struct lumaos_surface_present *p) {
    if (!screen || !p || !p->pixels || p->reserved != 0) return -1;
    if (!p->surface_width || !p->surface_height || !p->width || !p->height)
        return -1;
    if ((p->pixels & 3ULL) || (p->stride_bytes & 3U)) return -1;

    uint64_t min_stride = (uint64_t)p->surface_width * 4ULL;
    if (min_stride > UINT32_MAX || p->stride_bytes < min_stride) return -1;

    if ((uint64_t)p->src_x + p->width > p->surface_width ||
        (uint64_t)p->src_y + p->height > p->surface_height)
        return -1;

    if (p->dst_x < 0 || p->dst_y < 0) return -1;
    uint64_t graphics_height = (uint64_t)screen->fb_height - CONSOLE_TOP;
    if ((uint64_t)(uint32_t)p->dst_x + p->width > screen->fb_width ||
        (uint64_t)(uint32_t)p->dst_y + p->height > graphics_height)
        return -1;

    return 0;
}

int graphics_present(const struct lumaos_surface_present *p, int pid) {
    drop_stale_owner();
    if (!owner_task || owner_task != sched_current || owner_pid != pid)
        return -1;
    if (graphics_validate_present(p) != 0) return -1;

    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)screen->framebuffer;
    uint32_t fb_stride = screen->fb_pitch / 4;

    /* Keep the software pointer's saved background coherent with new pixels. */
    pointer_hide();
    for (uint32_t row = 0; row < p->height; row++) {
        uint64_t src_off = (uint64_t)(p->src_y + row) * p->stride_bytes +
                           (uint64_t)p->src_x * 4ULL;
        const uint32_t *src = (const uint32_t *)(uintptr_t)(p->pixels + src_off);
        uint64_t dst_row = (uint64_t)CONSOLE_TOP + (uint32_t)p->dst_y + row;
        volatile uint32_t *dst = fb + dst_row * fb_stride + (uint32_t)p->dst_x;
        for (uint32_t col = 0; col < p->width; col++)
            dst[col] = src[col];
    }
    pointer_show();
    return 0;
}
