#include <stdint.h>
#include "console.h"
#include "../include/handoff.h"

/* Hand-drawn 5x7 ASCII glyphs in 8x8 cells. Bit 7 is the leftmost pixel.
 * One blank row and horizontal margins keep adjacent glyphs separate. */
#define G(a,b,c,d,e,f,g) { (a)<<2, (b)<<2, (c)<<2, (d)<<2, (e)<<2, (f)<<2, (g)<<2, 0 }
static const uint8_t font8x8[95][8] = {
    G(0,0,0,0,0,0,0),             /* space */
    G(4,4,4,4,4,0,4),             /* ! */
    G(10,10,10,0,0,0,0),          /* " */
    G(10,31,10,10,31,10,0),       /* # */
    G(4,15,20,14,5,30,4),         /* $ */
    G(24,25,2,4,8,19,3),          /* % */
    G(12,18,20,8,21,18,13),       /* & */
    G(4,4,8,0,0,0,0),             /* ' */
    G(2,4,8,8,8,4,2),             /* ( */
    G(8,4,2,2,2,4,8),             /* ) */
    G(0,21,14,31,14,21,0),        /* * */
    G(0,4,4,31,4,4,0),            /* + */
    G(0,0,0,0,0,4,8),             /* , */
    G(0,0,0,31,0,0,0),            /* - */
    G(0,0,0,0,0,0,4),             /* . */
    G(1,1,2,4,8,16,16),           /* / */
    G(14,17,19,21,25,17,14),      /* 0 */
    G(4,12,4,4,4,4,14),           /* 1 */
    G(14,17,1,2,4,8,31),          /* 2 */
    G(30,1,1,14,1,1,30),          /* 3 */
    G(2,6,10,18,31,2,2),          /* 4 */
    G(31,16,16,30,1,1,30),        /* 5 */
    G(6,8,16,30,17,17,14),        /* 6 */
    G(31,1,2,4,8,8,8),            /* 7 */
    G(14,17,17,14,17,17,14),      /* 8 */
    G(14,17,17,15,1,2,12),        /* 9 */
    G(0,0,4,0,0,4,0),             /* : */
    G(0,0,4,0,0,4,8),             /* ; */
    G(2,4,8,16,8,4,2),            /* < */
    G(0,0,31,0,31,0,0),           /* = */
    G(8,4,2,1,2,4,8),             /* > */
    G(14,17,1,2,4,0,4),           /* ? */
    G(14,17,23,21,23,16,14),      /* @ */
    G(14,17,17,31,17,17,17),      /* A */
    G(30,17,17,30,17,17,30),      /* B */
    G(14,17,16,16,16,17,14),      /* C */
    G(30,17,17,17,17,17,30),      /* D */
    G(31,16,16,30,16,16,31),      /* E */
    G(31,16,16,30,16,16,16),      /* F */
    G(14,17,16,23,17,17,15),      /* G */
    G(17,17,17,31,17,17,17),      /* H */
    G(14,4,4,4,4,4,14),           /* I */
    G(7,2,2,2,2,18,12),           /* J */
    G(17,18,20,24,20,18,17),      /* K */
    G(16,16,16,16,16,16,31),      /* L */
    G(17,27,21,21,17,17,17),      /* M */
    G(17,25,25,21,19,19,17),      /* N */
    G(14,17,17,17,17,17,14),      /* O */
    G(30,17,17,30,16,16,16),      /* P */
    G(14,17,17,17,21,18,13),      /* Q */
    G(30,17,17,30,20,18,17),      /* R */
    G(15,16,16,14,1,1,30),        /* S */
    G(31,4,4,4,4,4,4),           /* T */
    G(17,17,17,17,17,17,14),      /* U */
    G(17,17,17,17,17,10,4),       /* V */
    G(17,17,17,21,21,27,17),      /* W */
    G(17,17,10,4,10,17,17),       /* X */
    G(17,17,10,4,4,4,4),          /* Y */
    G(31,1,2,4,8,16,31),          /* Z */
    G(14,8,8,8,8,8,14),           /* [ */
    G(16,16,8,4,2,1,1),           /* backslash */
    G(14,2,2,2,2,2,14),           /* ] */
    G(4,10,17,0,0,0,0),           /* ^ */
    G(0,0,0,0,0,0,31),            /* _ */
    G(8,4,2,0,0,0,0),             /* ` */
    G(0,0,14,1,15,17,15),         /* a */
    G(16,16,30,17,17,17,30),      /* b */
    G(0,0,14,17,16,17,14),        /* c */
    G(1,1,15,17,17,17,15),        /* d */
    G(0,0,14,17,31,16,14),        /* e */
    G(6,9,8,28,8,8,8),            /* f */
    G(0,15,17,17,15,1,14),        /* g */
    G(16,16,30,17,17,17,17),      /* h */
    G(4,0,12,4,4,4,14),           /* i */
    G(2,0,6,2,2,18,12),           /* j */
    G(16,16,18,20,24,20,18),      /* k */
    G(12,4,4,4,4,4,14),           /* l */
    G(0,0,26,21,21,21,21),        /* m */
    G(0,0,30,17,17,17,17),        /* n */
    G(0,0,14,17,17,17,14),        /* o */
    G(0,30,17,17,30,16,16),       /* p */
    G(0,15,17,17,15,1,1),         /* q */
    G(0,0,22,25,16,16,16),        /* r */
    G(0,0,15,16,14,1,30),         /* s */
    G(8,8,28,8,8,9,6),            /* t */
    G(0,0,17,17,17,19,13),        /* u */
    G(0,0,17,17,17,10,4),         /* v */
    G(0,0,17,17,21,21,10),        /* w */
    G(0,0,17,10,4,10,17),         /* x */
    G(0,17,17,17,15,1,14),        /* y */
    G(0,0,31,2,4,8,31),           /* z */
    G(2,4,4,8,4,4,2),             /* { */
    G(4,4,4,4,4,4,4),             /* | */
    G(8,4,4,2,4,4,8),             /* } */
    G(0,0,8,21,2,0,0),            /* ~ */
};
#undef G

static int framebuffer_valid(const struct lumaos_handoff *ho) {
    return ho && ho->framebuffer && !(ho->framebuffer & 3) &&
           ho->fb_bpp == 32 && ho->fb_width && ho->fb_height &&
           !(ho->fb_pitch & 3) && ho->fb_pitch / 4 >= ho->fb_width;
}

void draw_char(struct lumaos_handoff *ho, char c, int x, int y, uint32_t color) {
    if (!framebuffer_valid(ho)) return;
    unsigned char ch = (unsigned char)c;
    if (ch < 32 || ch > 126) ch = '?';
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)ho->framebuffer;
    uint32_t p = ho->fb_pitch / 4;
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            int64_t px = (int64_t)x + dx, py = (int64_t)y + dy;
            if ((font8x8[ch - 32][dy] & (0x80u >> dx)) &&
                px >= 0 && px < ho->fb_width && py >= 0 && py < ho->fb_height)
                fb[(uint64_t)py * p + (uint64_t)px] = color;
        }
    }
}

static struct lumaos_handoff *console_fb;
static uint32_t columns, rows, column, row, background;
static const uint32_t foreground = 0x00FFFFFF;

static void fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)console_fb->framebuffer;
    uint32_t stride = console_fb->fb_pitch / 4;
    for (uint32_t dy = 0; dy < h; dy++)
        for (uint32_t dx = 0; dx < w; dx++)
            fb[(uint64_t)(y + dy) * stride + x + dx] = color;
}

/* The glyph's final row is blank, so hiding the underline loses no glyph data. */
static void cursor(int visible) {
    fill_rect(column * 8, CONSOLE_TOP + row * 8 + 7, 8, 1,
              visible ? foreground : background);
}

static void scroll_if_needed(void) {
    if (row < rows) return;
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)console_fb->framebuffer;
    uint32_t stride = console_fb->fb_pitch / 4;
    /* Forward copy is safe for this upward, overlapping move. Preserve pitch
     * padding, the title and any incomplete cells at the screen edges. */
    for (uint32_t y = CONSOLE_TOP; y < CONSOLE_TOP + (rows - 1) * 8; y++)
        for (uint32_t x = 0; x < columns * 8; x++)
            fb[(uint64_t)y * stride + x] = fb[(uint64_t)(y + 8) * stride + x];
    row = rows - 1;
    fill_rect(0, CONSOLE_TOP + row * 8, columns * 8, 8, background);
}

void console_clear(void) {
    if (!console_fb) return;
    fill_rect(0, CONSOLE_TOP, columns * 8, rows * 8, background);
    column = row = 0;
    cursor(1);
}

void console_init(struct lumaos_handoff *ho) {
    console_fb = 0;
    if (!framebuffer_valid(ho) || ho->fb_width < 8 ||
        ho->fb_height < CONSOLE_TOP + 8 || ho->fb_width > INT32_MAX ||
        ho->fb_height > INT32_MAX || ho->fb_format > LUMAOS_PIXEL_BGR) return;
    console_fb = ho;
    columns = ho->fb_width / 8;
    rows = (ho->fb_height - CONSOLE_TOP) / 8;
    background = ho->fb_format == LUMAOS_PIXEL_BGR ? 0x000F0F2D : 0x002D0F0F;
    console_clear();
}

static void console_putc(unsigned char c) {
    cursor(0);
    if (c == '\n') {
        column = 0;
        row++;
    } else if (c == '\r') {
        column = 0;
    } else if (c == '\b') {
        if (column) column--;
        else if (row) { row--; column = columns - 1; }
    } else if (c == '\t') {
        uint32_t spaces = 4 - column % 4;
        while (spaces--) console_putc(' ');
    } else if (c >= 32) {
        fill_rect(column * 8, CONSOLE_TOP + row * 8, 8, 8, background);
        draw_char(console_fb, (char)c, (int)(column * 8),
                  (int)(CONSOLE_TOP + row * 8), foreground);
        if (++column == columns) { column = 0; row++; }
    }
    scroll_if_needed();
    cursor(1);
}

void console_write(const char *s) {
    if (!console_fb || !s) return;
    while (*s) console_putc((unsigned char)*s++);
}

void draw_string(struct lumaos_handoff *ho, const char *s, int x, int y, uint32_t color) {
    if (!s || !framebuffer_valid(ho)) return;
    int64_t cx = x;
    for (; *s && cx < ho->fb_width && cx <= INT32_MAX; s++, cx += 8)
        draw_char(ho, *s, (int)cx, y, color);
}
