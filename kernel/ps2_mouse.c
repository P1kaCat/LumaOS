#include "mouse.h"
#include "graphics.h"
#include "pointer.h"
#include "cpu.h"
#include "apic.h"

static struct mouse_decoder decoder;
static int ready;

/* Bounded boot-time polling. IRQ handlers never wait for controller replies. */
static int write_port(uint16_t port, uint8_t value) {
    for (unsigned n = 0; n < 100000; n++) {
        if (!(inb(0x64) & 2)) { outb(port, value); return 1; }
    }
    return 0;
}

static int read_reply(int auxiliary, uint8_t *value) {
    for (unsigned n = 0; n < 100000; n++) {
        uint8_t status = inb(0x64);
        if (status & 1) {
            uint8_t byte = inb(0x60);
            if ((status & 0xC0) || !!(status & 0x20) != auxiliary) return 0;
            *value = byte;
            return 1;
        }
    }
    return 0;
}

static int mouse_command(uint8_t command) {
    uint8_t reply;
    return write_port(0x64, 0xD4) && write_port(0x60, command) &&
           read_reply(1, &reply) && reply == 0xFA;
}

int ps2_mouse_init(void) {
    uint8_t mode = 0x43, reply;
    ready = 0;
    decoder.count = 0;
    /* Quiesce both ports while collecting command replies; retain translation. */
    if (!write_port(0x64, 0xAD) || !write_port(0x64, 0xA7)) goto fail;
    for (unsigned n = 0; n < 32 && (inb(0x64) & 1); n++) (void)inb(0x60);
    if (!write_port(0x64, 0x20) || !read_reply(0, &mode)) goto fail;
    if (!write_port(0x64, 0xA8) || !mouse_command(0xFF) ||
        !read_reply(1, &reply) || reply != 0xAA ||
        !read_reply(1, &reply) || reply != 0 || !mouse_command(0xF4)) goto fail;
    /* Enable IRQ1/IRQ12 and both clocks, leaving scan-code translation intact. */
    if (!write_port(0x64, 0x60) || !write_port(0x60, (mode | 3) & ~0x30)) goto fail;
    if (!apic_enable_isa_irq(12)) goto fail;
    ready = 1;
    serial_puts("[MOUSE9] PS/2 three-byte input enabled\n");
    return 1;
fail:
    /* Keep the keyboard available when a mouse/controller is absent. */
    write_port(0x64, 0xA7);
    if (write_port(0x64, 0x60)) write_port(0x60, (mode | 0x21) & ~0x12);
    write_port(0x64, 0xAE);
    serial_puts("[MOUSE9] unavailable; keyboard retained\n");
    return 0;
}

void ps2_mouse_reset_packet(void) { decoder.count = 0; }

void ps2_mouse_byte(uint8_t byte) {
    struct mouse_event event;
    if (ready && mouse_decode(&decoder, byte, &event)) {
        pointer_move(event.dx, event.dy, event.buttons);
        int32_t x, y;
        pointer_position(&x, &y);
        graphics_pointer_event(x, y, event.buttons);
    }
}
