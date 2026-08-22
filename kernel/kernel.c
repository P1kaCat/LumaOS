/*
 * kernel.c — Kernel LumaOS (Phase 0B + 0C)
 *
 * Point d'entrée du kernel après handoff du bootloader.
 */

#include <stdint.h>
#include "handoff.h"
#include "cpu.h"

/* ===== Port série COM1 ===== */

#define COM1 0x3F8

static void serial_putc(char c) {
    __asm__ volatile ("outb %0, %1" : : "a"(c), "dN"((uint16_t)COM1));
}

static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

/* ===== Framebuffer ===== */

static uint32_t make_color(uint8_t r, uint8_t g, uint8_t b, uint32_t format) {
    if (format == LUMAOS_PIXEL_BGR)
        return ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16);
    return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

static void fb_fill(struct lumaos_handoff *ho, uint32_t color) {
    uint32_t *fb = (uint32_t *)(unsigned long)ho->framebuffer;
    uint32_t pitch_pixels = ho->fb_pitch / 4;
    for (uint32_t y = 0; y < ho->fb_height; y++)
        for (uint32_t x = 0; x < ho->fb_width; x++)
            fb[y * pitch_pixels + x] = color;
}

static void fb_rect(struct lumaos_handoff *ho, uint32_t color,
                    uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t *fb = (uint32_t *)(unsigned long)ho->framebuffer;
    uint32_t pitch_pixels = ho->fb_pitch / 4;
    for (uint32_t dy = 0; dy < h; dy++) {
        for (uint32_t dx = 0; dx < w; dx++) {
            uint32_t px = x + dx, py = y + dy;
            if (px < ho->fb_width && py < ho->fb_height)
                fb[py * pitch_pixels + px] = color;
        }
    }
}

/* ===== Entrée du kernel ===== */

void kernel_main(struct lumaos_handoff *ho) {
    if (ho->magic != LUMAOS_HANDOFF_MAGIC) {
        serial_puts("LumaOS: INVALID HANDOFF MAGIC\n");
        for (;;) __asm__ volatile ("hlt");
    }

    serial_puts("\n");
    serial_puts("================================\n");
    serial_puts("  LumaOS Kernel — Phase 0C\n");
    serial_puts("================================\n");
    serial_puts("Kernel is alive!\n\n");

    /* Framebuffer proof of life */
    uint32_t bg = make_color(15, 15, 45, ho->fb_format);
    fb_fill(ho, bg);

    uint32_t green = make_color(40, 200, 100, ho->fb_format);
    uint32_t cx = ho->fb_width / 2, cy = ho->fb_height / 2;
    uint32_t bw = ho->fb_width / 4, bh = ho->fb_height / 4;
    fb_rect(ho, green, cx - bw/2, cy - bh/2, bw, bh);

    serial_puts("Framebuffer: ");
    char buf[32];
    uint32_t w = ho->fb_width;
    int i = 0;
    if (w == 0) buf[i++] = '0';
    while (w > 0) { buf[i++] = '0' + (w % 10); w /= 10; }
    for (int j = 0; j < i/2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
    buf[i] = 0;
    serial_puts(buf);
    serial_puts("x");
    uint32_t h = ho->fb_height;
    i = 0;
    if (h == 0) buf[i++] = '0';
    while (h > 0) { buf[i++] = '0' + (h % 10); h /= 10; }
    for (int j = 0; j < i/2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
    buf[i] = 0;
    serial_puts(buf);
    serial_puts(ho->fb_format == LUMAOS_PIXEL_BGR ? " BGR" : " RGB");
    serial_puts("\n\n");

    /* Phase 0C : GDT, PIC, IDT */
    serial_puts("[*] Setting up CPU tables...\n");
    gdt_init();
    pic_init();
    idt_init();

    serial_puts("\n[+] Interrupts enabled\n");
    __asm__ volatile ("sti");

    serial_puts("\nLumaOS Phase 0C complete. System idle.\n");

    for (;;) __asm__ volatile ("hlt");
}
