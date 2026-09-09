#ifndef LUMAOS_SCENE_H
#define LUMAOS_SCENE_H
#include <stdint.h>
#define DAMAGE_LIMIT 16
struct rect { int x, y, w, h; };
struct damage { struct rect rects[DAMAGE_LIMIT]; unsigned count; int width, height; };
static inline int contains(struct rect r, int x, int y) {
    return r.w > 0 && r.h > 0 && x >= r.x && y >= r.y &&
           (int64_t)x < (int64_t)r.x+r.w && (int64_t)y < (int64_t)r.y+r.h;
}
static inline struct rect clipped(struct rect r, int width, int height) {
    int64_t right = (int64_t)r.x+r.w, bottom = (int64_t)r.y+r.h;
    if (r.w <= 0 || r.h <= 0 || right <= 0 || bottom <= 0 || r.x >= width || r.y >= height)
        return (struct rect){0};
    int x = r.x < 0 ? 0 : r.x, y = r.y < 0 ? 0 : r.y;
    if (right > width) right = width;
    if (bottom > height) bottom = height;
    return (struct rect){x,y,(int)right-x,(int)bottom-y};
}
static inline void damage_add(struct damage *d, struct rect r) {
    r = clipped(r, d->width, d->height);
    if (!r.w || !r.h) return;
    for (unsigned i = 0; i < d->count;) {
        struct rect p = d->rects[i];
        if (r.x <= p.x+p.w && p.x <= r.x+r.w && r.y <= p.y+p.h && p.y <= r.y+r.h) {
            int x = r.x < p.x ? r.x : p.x, y = r.y < p.y ? r.y : p.y;
            int right = r.x+r.w > p.x+p.w ? r.x+r.w : p.x+p.w;
            int bottom = r.y+r.h > p.y+p.h ? r.y+r.h : p.y+p.h;
            r = (struct rect){x,y,right-x,bottom-y};
            d->rects[i] = d->rects[--d->count]; i = 0;
        } else i++;
    }
    if (d->count == DAMAGE_LIMIT) {
        d->count = 1; d->rects[0] = (struct rect){0,0,d->width,d->height};
    } else d->rects[d->count++] = r;
}
#endif
