/*
 * cpu.c — GDT, IDT, PIC setup pour LumaOS (Phase 0C)
 */
#include "cpu.h"
#include <stdint.h>

/* ===== Serial (déclaré dans kernel.c, on redéfinit ici) ===== */

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

static char *uitoa(uint64_t n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return buf; }
    char tmp[32]; int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0; while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
    return buf;
}

/* ===== GDT ===== */

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdtr;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[i].limit_low      = limit & 0xFFFF;
    gdt[i].base_low       = base & 0xFFFF;
    gdt[i].base_mid       = (base >> 16) & 0xFF;
    gdt[i].access         = access;
    gdt[i].flags_limit_high = (flags << 4) | ((limit >> 16) & 0x0F);
    gdt[i].base_high      = (base >> 24) & 0xFF;
}

void gdt_init(void) {
    gdt_set_entry(0, 0, 0, 0, 0);                            /* Null */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0x0A);               /* Code: 64-bit, present, executable */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0x0C);               /* Data: 64-bit, present, writable */

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)&gdt;

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        : : "m"(gdtr) : "rax", "memory"
    );

    serial_puts("[+] GDT loaded (3 entries)\n");
}

/* ===== IDT ===== */

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtr;

/* Déclarations extern des stubs ASM */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static void idt_set_entry(int i, void *handler, uint8_t flags) {
    uint64_t addr = (uint64_t)handler;
    idt[i].offset_low   = addr & 0xFFFF;
    idt[i].selector     = KERNEL_CS;
    idt[i].ist          = 0;
    idt[i].flags        = flags;
    idt[i].offset_mid   = (addr >> 16) & 0xFFFF;
    idt[i].offset_high  = (addr >> 32) & 0xFFFFFFFF;
    idt[i].reserved     = 0;
}

void idt_init(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    /* Exceptions CPU (0-31) */
    void (*isr_stubs[32])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    for (int i = 0; i < 32; i++)
        idt_set_entry(i, isr_stubs[i], 0x8E); /* Present | interrupt gate | ring 0 */

    /* IRQ 0-15 (PIC remappé → 32-47) */
    void (*irq_stubs[16])(void) = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };
    for (int i = 0; i < 16; i++)
        idt_set_entry(32 + i, irq_stubs[i], 0x8E);

    __asm__ volatile ("lidt %0" : : "m"(idtr));
    serial_puts("[+] IDT loaded (256 entries)\n");
}

/* ===== PIC 8259A ===== */

void pic_init(void) {
    /* ICW1 : init, cascade, ICW4 needed */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x11), "dN"((uint16_t)PIC1_CMD));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x11), "dN"((uint16_t)PIC2_CMD));
    /* ICW2 : offset vectors */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "dN"((uint16_t)PIC1_DATA)); /* IRQ 0-7 → 32-39 */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x28), "dN"((uint16_t)PIC2_DATA)); /* IRQ 8-15 → 40-47 */
    /* ICW3 : cascade */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x04), "dN"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x02), "dN"((uint16_t)PIC2_DATA));
    /* ICW4 : 8086 mode */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "dN"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "dN"((uint16_t)PIC2_DATA));
    /* Masquer tout sauf IRQ1 (clavier) pour l'instant */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFD), "dN"((uint16_t)PIC1_DATA)); /* 11111101 → IRQ1 only */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "dN"((uint16_t)PIC2_DATA)); /* all masked */
    serial_puts("[+] PIC remapped (IRQ0-15 → vectors 32-47)\n");
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8)
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)PIC_EOI), "dN"((uint16_t)PIC2_CMD));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)PIC_EOI), "dN"((uint16_t)PIC1_CMD));
}

/* ===== Handlers ===== */

static const char *exception_names[] = {
    "Divide by Zero", "Debug", "NMI", "Breakpoint", "Overflow",
    "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS",
    "Segment Not Present", "Stack-Segment Fault", "General Protection Fault",
    "Page Fault", "Reserved", "x87 FPU Error", "Alignment Check",
    "Machine Check", "SIMD Floating-Point", "Virtualization", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Security Exception", "Reserved"
};

void exception_handler(uint64_t int_no, uint64_t err_code) {
    serial_puts("\n[!] EXCEPTION #");
    char buf[32];
    serial_puts(uitoa(int_no, buf));
    serial_puts(" — ");
    if (int_no < 32) serial_puts(exception_names[int_no]);
    serial_puts(" (err: ");
    serial_puts(uitoa(err_code, buf));
    serial_puts(")\n");
    serial_puts("[!] Halting.\n");
    for (;;) __asm__ volatile ("hlt");
}

static uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "dN"(port));
    return v;
}

static uint64_t kbd_ticks = 0;

void irq_default_handler(uint8_t irq) {
    if (irq == 1) {
        /* Keyboard: read scancode from port 0x60 */
        uint8_t sc = inb(0x60);
        kbd_ticks++;
        char buf[32];
        serial_puts("[kbd] scancode=0x");
        serial_puts(uxtoa((uint64_t)sc, buf));
        serial_puts(" (");
        serial_puts(uitoa(kbd_ticks, buf));
        serial_puts(")\n");
    }
    pic_eoi(irq);
}

void isr_handler(struct registers *regs) {
    if (regs->int_no < 32)
        exception_handler(regs->int_no, regs->err_code);
}

void irq_handler(struct registers *regs) {
    uint8_t irq = regs->int_no - 32;
    irq_default_handler(irq);
}
