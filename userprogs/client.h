#include "api.h"
#include "../include/route_abi.h"
#if APP_ID == 1
#define TAG "[CLIENT9 A] "
#else
#define TAG "[CLIENT9 B] "
#endif
#define check(ok) do { if (!(ok)) { say(TAG "FAILED: " #ok "\n"); done(1); } } while (0)
static uint32_t native(unsigned r, unsigned g, unsigned b, unsigned format) {
    return format ? (r<<16)|(g<<8)|b : r|(g<<8)|(b<<16);
}
void _start(void) {
    struct lumaos_graphics_info info;
    check(call3(13, (uintptr_t)&info, sizeof(info), 0) == 0);
    struct lumaos_surface_request r = {.op = LUMAOS_SURFACE_PRESENTER};
    check(call3(18, (uintptr_t)&r, 0, 0) == 0 && r.peer);
    unsigned presenter = r.peer;
    r = (struct lumaos_surface_request){.op=1,.width=128,.height=80};
    check(call3(18,(uintptr_t)&r,0,0) == 0);
    unsigned handle=r.handle;
    uint32_t *pixels=(uint32_t *)(uintptr_t)r.address;
    for (unsigned i=0;i<128*80;i++) pixels[i]=native(APP_ID==1?170:40,80,APP_ID==1?40:180,info.pixel_format);
    r=(struct lumaos_surface_request){.op=2,.handle=handle,.peer=presenter};
    check(call3(18,(uintptr_t)&r,0,0)==0);
    struct lumaos_route q={.op=LUMAOS_ROUTE_FOCUS,.handle=handle};
    check(call3(20,(uintptr_t)&q,sizeof(q),0)==-1); /* client cannot focus or inject */
    q=(struct lumaos_route){.op=LUMAOS_ROUTE_SEND,.handle=handle,
        .event={.type=LUMAOS_INPUT_KEY,.value='h',.flags=LUMAOS_INPUT_DOWN}};
    check(call3(20,(uintptr_t)&q,sizeof(q),0)==-1);
    check(call3(20,0x100000,sizeof(q),0)==-1);
    check(call3(20,(uintptr_t)_start,sizeof(q),0)==-1);
    check(call3(20,0xbffff8,sizeof(q),0)==-1);
    say(TAG "ready; isolation checks passed\n");
    int focused=0, pending=0;
    unsigned changes=0;
    for (;;) {
        if (pending) {
            struct lumaos_surface_update u={.op=1,.handle=handle,.width=8,.height=8};
            long result=call3(19,(uintptr_t)&u,sizeof(u),0);
            check(result==0 || result==-2);
            if (!result) { pending=0; say(TAG "published 8x8\n"); }
        }
        q=(struct lumaos_route){.op=LUMAOS_ROUTE_READ,.handle=handle};
        long result=call3(20,(uintptr_t)&q,sizeof(q),0);
        if (result==-1) { say(TAG "presenter gone; exit\n"); done(0); }
        check(result==0 || result==1);
        if (result==1) {
            if (q.event.type==LUMAOS_INPUT_FOCUS) {
                focused=q.event.value!=0;
                say(focused ? TAG "focus gained\n" : TAG "focus lost\n");
            }
            if (q.event.type==LUMAOS_INPUT_KEY && (q.event.flags&LUMAOS_INPUT_DOWN)) {
                check(focused);
                for (unsigned slot=0;slot<4;slot++) if (slot!=(handle&3)) {
                    r=(struct lumaos_surface_request){.op=LUMAOS_SURFACE_ENUM,.index=slot};
                    check(call3(18,(uintptr_t)&r,0,0)==-1);
                }
                if (q.event.value=='h') say(TAG "key h\n");
                if (q.event.value=='j') say(TAG "key j\n");
                ++changes;
                for (unsigned y=0;y<8;y++) for (unsigned x=0;x<8;x++)
                    pixels[y*128+x]=native(220,changes&1?220:120,40,info.pixel_format);
                pending=1;
            }
            if (q.event.type==LUMAOS_INPUT_POINTER && (q.event.code&1)) {
                check(q.event.x>=0 && q.event.x<128 && q.event.y>=0 && q.event.y<80);
                say(TAG "pointer down\n");
            }
        }
        sleep_ticks(1);
    }
}
