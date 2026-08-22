/*
 * kernel.c — Kernel LumaOS (Phase 0B)
 *
 * Point d'entrée du kernel après handoff du bootloader.
 * Reçoit un pointeur vers lumaos_handoff avec :
 *   - Le framebuffer (GOP)
 *   - Le memory map UEFI
 *
 * Phase 0B : prouver que le kernel est en vie.
 *   1. Écrire sur le port série (COM1) → visible dans QEMU -serial stdio
 *   2. Remplir l'écran avec une couleur → prouve que le framebuffer marche
 *   3. Halt
 */

#include <stdint.h>
#include "handoff.h"   /* include/ au root du repo (via -I../include) */

/* ===== Port série COM1 (0x3F8) ===== */

#define COM1_DATA 0x3F8

static void serial_putc(char c) {
    __asm__ volatile ("outb %0, %1" : : "a"(c), "dN"((unsigned short)COM1_DATA));
}

static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s);
        s++;
    }
}

/* ===== Framebuffer ===== */

/* Construire une couleur 32bpp selon le format UEFI */
static uint32_t make_color(uint8_t r, uint8_t g, uint8_t b, uint32_t format) {
    if (format == LUMAOS_PIXEL_BGR)
        return ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16);
    /* RGB par défaut */
    return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

/* Remplir tout l'écran avec une couleur */
static void fb_fill(struct lumaos_handoff *ho, uint32_t color) {
    uint32_t *fb = (uint32_t *)(unsigned long)ho->framebuffer;
    uint32_t pitch_pixels = ho->fb_pitch / 4;  /* pitch en pixels */

    for (uint32_t y = 0; y < ho->fb_height; y++) {
        for (uint32_t x = 0; x < ho->fb_width; x++) {
            fb[y * pitch_pixels + x] = color;
        }
    }
}

/* Dessiner un rectangle (pour un effet visuel simple) */
static void fb_rect(struct lumaos_handoff *ho, uint32_t color,
                    uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t *fb = (uint32_t *)(unsigned long)ho->framebuffer;
    uint32_t pitch_pixels = ho->fb_pitch / 4;

    for (uint32_t dy = 0; dy < h; dy++) {
        for (uint32_t dx = 0; dx < w; dx++) {
            uint32_t px = x + dx;
            uint32_t py = y + dy;
            if (px < ho->fb_width && py < ho->fb_height)
                fb[py * pitch_pixels + px] = color;
        }
    }
}

/* ===== Entrée du kernel ===== */

void kernel_main(struct lumaos_handoff *ho) {
    /* 1. Valider le handoff */
    if (ho->magic != LUMAOS_HANDOFF_MAGIC) {
        /* Pas de handoff valide — on ne peut pas faire grand-chose */
        serial_puts("LumaOS: INVALID HANDOFF MAGIC\n");
        for (;;) __asm__ volatile ("hlt");
    }

    /* 2. Proof of life sur le port série */
    serial_puts("\n");
    serial_puts("================================\n");
    serial_puts("  LumaOS Kernel — Phase 0B\n");
    serial_puts("================================\n");
    serial_puts("Kernel is alive!\n\n");

    /* 3. Proof of life sur le framebuffer */
    /* Fond bleu foncé */
    uint32_t bg = make_color(15, 15, 45, ho->fb_format);
    fb_fill(ho, bg);

    /* Rectangle central vert (effet "logo" minimaliste) */
    uint32_t green = make_color(40, 200, 100, ho->fb_format);
    uint32_t cx = ho->fb_width / 2;
    uint32_t cy = ho->fb_height / 2;
    uint32_t bw = ho->fb_width / 4;
    uint32_t bh = ho->fb_height / 4;
    fb_rect(ho, green, cx - bw/2, cy - bh/2, bw, bh);

    /* Infos sur le port série */
    serial_puts("Framebuffer info:\n");
    serial_puts("  Resolution: ");

    /* Afficher les dimensions (serial only, pas de printf en freestanding) */
    /* On affiche en décimal manuellement */
    char buf[32];
    uint32_t w = ho->fb_width;
    int i = 0;
    if (w == 0) buf[i++] = '0';
    while (w > 0) { buf[i++] = '0' + (w % 10); w /= 10; }
    /* reverse */
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
    serial_puts("\n  Format: ");
    serial_puts(ho->fb_format == LUMAOS_PIXEL_BGR ? "BGR\n" : "RGB\n");

    serial_puts("\nLumaOS Phase 0B complete. Halting.\n");

    /* 4. Halt */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
