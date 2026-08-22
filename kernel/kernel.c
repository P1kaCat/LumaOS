#include <stdint.h>
#include "handoff.h"
#include "cpu.h"
#include "mem.h"
#include "sched.h"
#include "user.h"

#define COM1 0x3F8
static void serial_putc(char c) { __asm__ volatile ("outb %0, %1" : : "a"(c), "dN"((uint16_t)COM1)); }
static char *uitoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0; while (n) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}
static char *uxtoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0; const char *h="0123456789ABCDEF";
    while (n) { tmp[i++]=h[n&0xF]; n>>=4; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

static uint32_t make_color(uint8_t r, uint8_t g, uint8_t b, uint32_t fmt) {
    if (fmt == LUMAOS_PIXEL_BGR) return (uint32_t)b | ((uint32_t)g<<8) | ((uint32_t)r<<16);
    return (uint32_t)r | ((uint32_t)g<<8) | ((uint32_t)b<<16);
}
static void fb_fill(struct lumaos_handoff *ho, uint32_t c) {
    uint32_t *fb = (uint32_t *)(unsigned long)ho->framebuffer;
    uint32_t p = ho->fb_pitch / 4;
    for (uint32_t y=0;y<ho->fb_height;y++) for (uint32_t x=0;x<ho->fb_width;x++) fb[y*p+x]=c;
}
static void fb_rect(struct lumaos_handoff *ho, uint32_t c, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t *fb = (uint32_t *)(unsigned long)ho->framebuffer;
    uint32_t p = ho->fb_pitch / 4;
    for (uint32_t dy=0;dy<h;dy++) for (uint32_t dx=0;dx<w;dx++) { uint32_t px=x+dx,py=y+dy; if(px<ho->fb_width&&py<ho->fb_height) fb[py*p+px]=c; }
}

static void task1_main(void) {
    uint64_t count = 0; char buf[32];
    for (;;) {
        serial_puts("  [Task 1] tick "); serial_puts(uitoa(count++, buf)); serial_puts("\n");
        __asm__ volatile ("hlt");
    }
}
static void task2_main(void) {
    uint64_t count = 0; char buf[32];
    for (;;) {
        serial_puts("  [Task 2] tick "); serial_puts(uitoa(count++, buf)); serial_puts("\n");
        __asm__ volatile ("hlt");
    }
}

void kernel_main(struct lumaos_handoff *ho) {
    if (ho->magic != LUMAOS_HANDOFF_MAGIC) {
        serial_puts("LumaOS: INVALID HANDOFF MAGIC\n");
        for (;;) __asm__ volatile ("hlt");
    }

    serial_puts("\n================================\n");
    serial_puts("  LumaOS Kernel — Phase 1\n");
    serial_puts("================================\n");
    serial_puts("Kernel is alive!\n\n");

    uint32_t bg = make_color(15,15,45,ho->fb_format);
    fb_fill(ho, bg);
    uint32_t green = make_color(40,200,100,ho->fb_format);
    uint32_t cx=ho->fb_width/2, cy=ho->fb_height/2, bw=ho->fb_width/4, bh=ho->fb_height/4;
    fb_rect(ho, green, cx-bw/2, cy-bh/2, bw, bh);

    serial_puts("[*] Setting up CPU tables...\n");
    gdt_init();
    pic_init();
    idt_init();
    tss_init();

    serial_puts("\n[*] Setting up paging...\n");
    paging_init();

    serial_puts("\n[*] Initializing heap...\n");
    heap_init(ho);

    char buf[32];
    void *a = kmalloc(256);
    void *b = kmalloc(1024);
    serial_puts("  kmalloc(256)  = 0x"); serial_puts(uxtoa((uint64_t)(unsigned long)a, buf)); serial_puts("\n");
    serial_puts("  kmalloc(1024) = 0x"); serial_puts(uxtoa((uint64_t)(unsigned long)b, buf)); serial_puts("\n");

    serial_puts("\n[*] Starting scheduler...\n");
    sched_init();
    task_create(task1_main, 1);
    task_create(task2_main, 2);
    pit_init(50);

    serial_puts("[+] Interrupts enabled (timer 50Hz + keyboard)\n");
    __asm__ volatile ("sti");

    serial_puts("\n[*] Setting up user mode (Ring 3)...\n");
    serial_puts("[+] Paging: user-accessible (U/S bit set)\n");
    user_init(); /* never returns — enters Ring 3 */

    for (;;) __asm__ volatile ("hlt");
}
