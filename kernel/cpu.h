/*
 * cpu.h — GDT, IDT, ISR, PIC pour LumaOS (Phase 0C)
 */
#ifndef LUMAOS_CPU_H
#define LUMAOS_CPU_H

#include <stdint.h>

/* ===== GDT ===== */

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Selectors */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10

/* ===== User mode (Ring 3) ===== */
#define USER_CS 0x1B  /* GDT entry 3 | RPL 3 */
#define USER_DS 0x23  /* GDT entry 4 | RPL 3 */

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

void tss_init(void);
struct registers;
void syscall_handler(struct registers *regs);

void gdt_init(void);

/* ===== IDT ===== */

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256

void idt_init(void);

/* ===== PIC 8259A ===== */

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define PIC_EOI    0x20

void pic_init(void);
void pic_eoi(uint8_t irq);

/* ===== ISR ===== */

struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

void isr_handler(struct registers *regs);
void irq_handler(struct registers *regs);

/* Handlers C */
void exception_handler(uint64_t int_no, uint64_t err_code);
void irq_default_handler(uint8_t irq);

#endif /* LUMAOS_CPU_H */
