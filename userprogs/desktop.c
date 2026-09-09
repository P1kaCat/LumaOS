#include "api.h"
#include "scene.h"
#include "../include/route_abi.h"
#ifndef MULTI
#define MULTI 0
#endif
/* All window policy and composition live in this isolated process. The kernel
 * knows only the granted pixel views and validated presentation rectangles. */
struct window {
    int x, y, visible;
    unsigned width, height, handle;
    uint32_t *pixels;
    const uint32_t *view;
};
static struct window windows[2]; /* back to front; swap on focus */
static struct lumaos_graphics_info info;
static struct lumaos_input_event events[LUMAOS_INPUT_BATCH];
static uint32_t tile[64 * 64], cache[2][128 * 80];
static struct damage damage;
static uint64_t rendered_pixels, present_calls;
static unsigned focus_handle;
#define check(ok) do { if (!(ok)) { say("[DESKTOP9] FAILED: " #ok "\n"); done(1); } } while (0)
static uint32_t color(unsigned r, unsigned g, unsigned b) {
    return info.pixel_format ? (r << 16) | (g << 8) | b : r | (g << 8) | (b << 16);
}
static int inside(struct window *w, int x, int y) {
    return w->visible && x >= w->x && y >= w->y &&
           x < w->x + (int)w->width && y < w->y + (int)w->height + 16;
}
static void render_rect(struct rect area) {
    uint32_t background = color(18, 26, 42), title = color(62, 94, 146);
    for (unsigned y = area.y; y < (unsigned)(area.y+area.h); y += 64) {
        for (unsigned x = area.x; x < (unsigned)(area.x+area.w); x += 64) {
            unsigned width = (unsigned)(area.x+area.w) - x < 64 ? (unsigned)(area.x+area.w) - x : 64;
            unsigned height = (unsigned)(area.y+area.h) - y < 64 ? (unsigned)(area.y+area.h) - y : 64;
            for (unsigned py = 0; py < height; py++) for (unsigned px = 0; px < width; px++) {
                uint32_t pixel = background;
                for (unsigned i = 0; i < 2; i++) {
                    struct window *w = &windows[i];
                    if (!inside(w, x + px, y + py)) continue;
                    unsigned wx = x + px - w->x, wy = y + py - w->y;
                    pixel = wy < 16 ? title : w->pixels[(wy - 16) * w->width + wx];
                }
                tile[py * 64 + px] = pixel;
            }
            struct lumaos_surface_present p = {
                .pixels = (uintptr_t)tile, .surface_width = 64, .surface_height = 64,
                .stride_bytes = 256, .dst_x = x, .dst_y = y, .width = width, .height = height
            };
            check(call3(15, (uintptr_t)&p, sizeof(p), 0) == 0);
            rendered_pixels += width * height; ++present_calls;
        }
    }
}

static struct rect bounds(struct window *w) { return (struct rect){w->x,w->y,w->width,w->height+16}; }
static void invalidate(struct window *w) { if (w->visible) damage_add(&damage,bounds(w)); }
static void render(void) {
    for (unsigned i=0;i<damage.count;i++) render_rect(damage.rects[i]);
    damage.count=0;
}
static void updates(void) {
    for (unsigned n=0;n<2;n++) {
        struct window *w=&windows[n];
        if (n && w->handle==windows[0].handle) continue;
        struct lumaos_surface_update u={.op=2,.handle=w->handle};
        long result=call3(19,(uintptr_t)&u,sizeof(u),0);
        check(result==0 || result==1);
        if (!result) continue;
        check(u.x+u.width<=w->width && u.y+u.height<=w->height);
        for (unsigned y=u.y;y<u.y+u.height;y++) for (unsigned x=u.x;x<u.x+u.width;x++)
            w->pixels[y*w->width+x]=w->view[y*w->width+x];
        for (unsigned i=0;i<2;i++) if (windows[i].visible && windows[i].handle==w->handle)
            damage_add(&damage,(struct rect){windows[i].x+u.x,windows[i].y+16+u.y,u.width,u.height});
        u=(struct lumaos_surface_update){.op=3,.handle=w->handle,.serial=u.serial};
        check(call3(19,(uintptr_t)&u,sizeof(u),0)==0);
    }
}
static long route(unsigned op, unsigned handle, struct lumaos_input_event e) {
    if (!MULTI) return 0;
    long result;
    do {
        struct lumaos_route r={.op=op,.handle=handle,.event=e};
        result=call3(20,(uintptr_t)&r,sizeof(r),0);
        if (result==-2) sleep_ticks(1); /* explicit backpressure, no dropped transitions */
    } while (result==-2);
    return result;
}
static void focus(unsigned handle) {
    check(route(LUMAOS_ROUTE_FOCUS,handle,(struct lumaos_input_event){0})==0);
    focus_handle=handle;
}
static void send(unsigned handle, struct lumaos_input_event e) {
    check(route(LUMAOS_ROUTE_SEND,handle,e)==0);
}
static char *text(char *p, const char *s) { while (*s) *p++=*s++; return p; }
static char *number(char *p, uint64_t n) {
    char digits[24]; unsigned count=0;
    do { digits[count++]='0'+n%10; n/=10; } while (n);
    while (count) *p++=digits[--count]; return p;
}
static void benchmark(void) {
    uint64_t p0=rendered_pixels,c0=present_calls;
    for (unsigned i=0;i<8;i++) render_rect((struct rect){0,0,info.width,info.height});
    uint64_t full_pixels=rendered_pixels-p0,full_calls=present_calls-c0;
    p0=rendered_pixels;c0=present_calls;
    for (unsigned i=0;i<8;i++) render_rect((struct rect){windows[1].x,windows[1].y+16,8,8});
    char line[180],*p=line;
    p=text(p,"[REDRAW9] full pixels=");p=number(p,full_pixels);
    p=text(p," calls=");p=number(p,full_calls);
    p=text(p," partial pixels=");p=number(p,rendered_pixels-p0);
    p=text(p," calls=");p=number(p,present_calls-c0);
    p=text(p," frames=8\n");*p=0;say(line);
}
void _start(void) {
    check(call3(13,(uintptr_t)&info,sizeof(info),0)==0);
    check(info.width>=400 && info.height>=300);
    damage.width=info.width; damage.height=info.height;
    check(call3(14,0,0,0)==0);
    check(call3(17,(uintptr_t)events,LUMAOS_INPUT_BATCH,0)>=0);
    long producers[2]={0,0};
    producers[0]=call3(12,(uintptr_t)(MULTI?"appa.elf":"paint.elf"),0,0);
    check(producers[0]>0);
    if (MULTI) { producers[1]=call3(12,(uintptr_t)"appb.elf",0,0);check(producers[1]>0); }
    for (unsigned n=0;n<(MULTI?2u:1u);n++) {
        struct lumaos_surface_request r;
        int found=0;
        while (!found) {
            for (unsigned slot=0;slot<4;slot++) {
                r=(struct lumaos_surface_request){.op=5,.index=slot};
                if (call3(18,(uintptr_t)&r,0,0)==0 && (MULTI?r.peer==(unsigned)producers[n]:r.peer==0)) {
                    found=1;break;
                }
            }
            if (!found) sleep_ticks(1);
        }
        check(r.width==128 && r.height==80);
        check(call3(17,r.address,1,0)==-1);
        windows[n]=(struct window){n?260:80,n?150:70,1,r.width,r.height,r.handle,cache[n],(const uint32_t *)(uintptr_t)r.address};
    }
    if (!MULTI) { windows[1]=windows[0];windows[1].x=260;windows[1].y=150; }
    updates();
    focus(windows[1].handle);
    damage_add(&damage,(struct rect){0,0,info.width,info.height});render();
    say(MULTI?"[MULTI9] ready: two live applications\n":"[DESKTOP9] ready: two windows, drag title, V visibility, X exit\n");
    int dragging=0,offset_x=0,offset_y=0,was_down=0;
    unsigned capture=0;
    for (;;) {
        long count=call3(17,(uintptr_t)events,LUMAOS_INPUT_BATCH,0);check(count>=0);
        for (long i=0;i<count;i++) {
            struct lumaos_input_event e=events[i];
            if (e.type==LUMAOS_INPUT_OVERFLOW) { dragging=0;capture=0;was_down=(e.code&1)!=0;focus(0);continue; }
            if (e.type==LUMAOS_INPUT_KEY) {
                if (e.value=='v' && (e.flags&LUMAOS_INPUT_DOWN)) {
                    invalidate(&windows[1]);windows[1].visible=!windows[1].visible;invalidate(&windows[1]);
                    dragging=0;capture=0;focus(windows[1].visible?windows[1].handle:windows[0].visible?windows[0].handle:0);
                    say(windows[1].visible?"[DESKTOP9] shown\n":"[DESKTOP9] hidden\n");
                } else if (e.value=='x' && (e.flags&LUMAOS_INPUT_DOWN)) {
                    focus(0);
                    for (unsigned n=0;n<2;n++) {
                        if (n && windows[n].handle==windows[0].handle) continue;
                        struct lumaos_surface_request r={.op=4,.handle=windows[n].handle};
                        check(call3(18,(uintptr_t)&r,0,0)==0);
                    }
                    check(call3(16,0,0,0)==0);
                    say(MULTI?"[MULTI9] closed\n":"[DESKTOP9] closed; shell restored\n");done(0);
                } else if (e.value=='f' && (e.flags&LUMAOS_INPUT_DOWN)) {
                    render_rect((struct rect){0,0,info.width,info.height});say("[REDRAW9] reference full frame\n");
                } else if (e.value=='b' && (e.flags&LUMAOS_INPUT_DOWN)) benchmark();
                else if (MULTI && focus_handle && e.value!='v' && e.value!='x' && e.value!='f' && e.value!='b') send(focus_handle,e);
            }
            if (e.type!=LUMAOS_INPUT_POINTER) continue;
            int down=(e.code&1)!=0;
            if (down && !was_down) {
                int hit=-1;
                for (int n=1;n>=0;n--) if (inside(&windows[n],e.x,e.y)) {hit=n;break;}
                if (hit<0) focus(0);
                else {
                    invalidate(&windows[hit]);
                    struct window w=windows[hit];windows[hit]=windows[1];windows[1]=w;
                    invalidate(&windows[1]);focus(w.handle);
                    dragging=e.y<w.y+16;capture=dragging?0:w.handle;
                    offset_x=e.x-w.x;offset_y=e.y-w.y;
                }
            }
            if (down && dragging) {
                invalidate(&windows[1]);
                int x=e.x-offset_x,y=e.y-offset_y,max_x=info.width-windows[1].width,max_y=info.height-windows[1].height-16;
                windows[1].x=x<0?0:x>max_x?max_x:x;windows[1].y=y<0?0:y>max_y?max_y:y;
                invalidate(&windows[1]);
            }
            if (MULTI && !dragging) {
                for (int n=1;n>=0;n--) {
                    struct window *w=&windows[n];
                    /* A front title bar occludes client content behind it. */
                    if (!capture && inside(w,e.x,e.y) && e.y<w->y+16) break;
                    int eligible=capture?w->handle==capture:(inside(w,e.x,e.y) && e.y>=w->y+16);
                    if (!eligible) continue;
                    struct lumaos_input_event local=e;local.x=e.x-w->x;local.y=e.y-w->y-16;
                    if (local.x<0) local.x=0;if (local.y<0) local.y=0;
                    if ((unsigned)local.x>=w->width) local.x=w->width-1;
                    if ((unsigned)local.y>=w->height) local.y=w->height-1;
                    send(w->handle,local);break;
                }
            }
            if (!down && dragging) {dragging=0;say("[DESKTOP9] moved\n");}
            if (!down) capture=0;
            was_down=down;
        }
        updates();render();sleep_ticks(1);
    }
}
