/*
 * cpu.c — GDT (7 entries), IDT (+syscall), TSS, PIC, handlers
 * Phase 4: lazy allocation, stack growth, sbrk syscall
 * Phase 5: keyboard ASCII, read/sleep/yield/getpages syscalls, error handling
 */
#include "cpu.h"
#include "sched.h"
#include "mem.h"
#include "vfs.h"
#include <stdint.h>

#define COM1 0x3F8
static void serial_putc(char c) { __asm__ volatile ("outb %0, %1" : : "a"(c), "dN"((uint16_t)COM1)); }
void serial_puts(const char *s) { while (*s) { if (*s == '\n') serial_putc('\r'); serial_putc(*s++); } }
static char *uitoa(uint64_t n, char *buf) {
    if (n == 0) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0; while (n>0) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while (i>0) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}
static char *uxtoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0; const char *h="0123456789ABCDEF";
    while (n) { tmp[i++]=h[n&0xF]; n>>=4; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

/* ===== Keyboard (Phase 5) ===== */
#define KB_BUF_SIZE 256
static char kb_buffer[KB_BUF_SIZE];
static int kb_head = 0;
static int kb_tail = 0;
static int shift_pressed = 0;

/* Set 1 scancode → ASCII — AZERTY FR unshifted (lowercase)
 *
 * Physical key positions → AZERTY unshifted characters.
 * Non-ASCII chars (é è ç à ù ²) map to 0 (skipped) — use Shift
 * for their digit equivalents.
 * Letters: a/z swapped vs QWERTY, m at position 0x27.
 */
static const char scancode_map[128] = {
    /* Number row: & é " ' ( - è _ ç à ) = */
    [0x02] = '&',
    /* 0x03: é → not ASCII, skip */
    [0x04] = '"',
    [0x05] = '\'',
    [0x06] = '(',
    [0x07] = '-',
    /* 0x08: è → not ASCII, skip */
    [0x09] = '_',
    /* 0x0A: ç → not ASCII, skip */
    /* 0x0B: à → not ASCII, skip */
    [0x0C] = ')',
    [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    /* Top letter row: azertyuiop ^ $ */
    [0x10] = 'a', [0x11] = 'z', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '^', [0x1B] = '$',
    [0x1C] = '\n',
    /* Home row: qsdfghjklm ù * */
    [0x1E] = 'q', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = 'm',
    /* 0x28: ù → not ASCII, skip */
    [0x29] = '*',
    /* Bottom row: wxcvbn , ; : ! */
    [0x2C] = 'w', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n',
    [0x32] = ',', [0x33] = ';', [0x34] = ':', [0x35] = '!',
    [0x39] = ' ',
};

/* Set 1 scancode → ASCII — AZERTY FR shifted (uppercase + digits)
 *
 * Shift + number row → digits (1-0)
 * Shift + letters → uppercase
 * Shift + special → ? . / % etc.
 */
static const char scancode_shift_map[128] = {
    /* Number row shifted: 1 2 3 4 5 6 7 8 9 0 */
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0',
    /* 0x0C: ° → not ASCII, skip */
    [0x0D] = '+',
    [0x0E] = '\b', [0x0F] = '\t',
    /* Top letter row: AZERTYUIOP */
    [0x10] = 'A', [0x11] = 'Z', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P',
    /* 0x1A: ¨ dead key → skip */
    [0x1B] = '$',  /* £ → not ASCII, keep $ */
    [0x1C] = '\n',
    /* Home row: QSDFGHJKLM% */
    [0x1E] = 'Q', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = 'M',
    [0x28] = '%',  /* ù shifted → % */
    [0x29] = '*',  /* µ → not ASCII, keep * */
    /* Bottom row: WXCVBN ? . / */
    [0x2C] = 'W', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N',
    [0x32] = '?', [0x33] = '.', [0x34] = '/',
    /* 0x35: § → not ASCII, skip */
    [0x39] = ' ',
};

/* ===== GDT ===== */

static struct gdt_entry gdt[7];
static struct gdt_ptr   gdtr;
struct tss tss;

volatile uint64_t test_fault_addr = 0;
volatile int test_fault_caught = 0;
static uint64_t tss_stack[2048];

static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[i].limit_low      = limit & 0xFFFF;
    gdt[i].base_low       = base & 0xFFFF;
    gdt[i].base_mid       = (base >> 16) & 0xFF;
    gdt[i].access         = access;
    gdt[i].flags_limit_high = (flags << 4) | ((limit >> 16) & 0x0F);
    gdt[i].base_high      = (base >> 24) & 0xFF;
}

void gdt_init(void) {
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0x0A);
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0x0C);
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0x0A);
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0x0C);
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    gdt_set_entry(5, (uint32_t)tss_base, tss_limit, 0x89, 0x00);
    *(uint64_t *)&gdt[6] = tss_base >> 32;
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
    serial_puts("[+] GDT loaded (5 segments + TSS)\n");
}

void tss_init(void) {
    for (int i = 0; i < (int)(sizeof(tss)/8); i++) ((uint64_t*)&tss)[i] = 0;
    tss.rsp0 = (uint64_t)&tss_stack[2048];
    __asm__ volatile ("ltr %0" : : "r"((uint16_t)0x28));
    char buf[32];
    serial_puts("[+] TSS loaded (RSP0=0x");
    serial_puts(uxtoa(tss.rsp0, buf));
    serial_puts(")\n");
}

/* ===== IDT ===== */

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtr;

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
extern void isr128(void);

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
    void (*isr_stubs[32])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    for (int i = 0; i < 32; i++)
        idt_set_entry(i, isr_stubs[i], 0x8E);
    void (*irq_stubs[16])(void) = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };
    for (int i = 0; i < 16; i++)
        idt_set_entry(32 + i, irq_stubs[i], 0x8E);
    idt_set_entry(128, isr128, 0xEE);
    __asm__ volatile ("lidt %0" : : "m"(idtr));
    serial_puts("[+] IDT loaded (256 entries + syscall gate @128 DPL=3)\n");
}

/* ===== PIC ===== */
void pic_init(void) {
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x11), "dN"((uint16_t)PIC1_CMD));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x11), "dN"((uint16_t)PIC2_CMD));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "dN"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x28), "dN"((uint16_t)PIC2_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x04), "dN"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x02), "dN"((uint16_t)PIC2_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "dN"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "dN"((uint16_t)PIC2_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFC), "dN"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "dN"((uint16_t)PIC2_DATA));
    serial_puts("[+] PIC remapped (IRQ0-15 -> vectors 32-47)\n");
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8)
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)PIC_EOI), "dN"((uint16_t)PIC2_CMD));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)PIC_EOI), "dN"((uint16_t)PIC1_CMD));
}

/* ===== Handlers ===== */
static const char *exception_names[] = {
    "Divide by Zero", "Debug", "NMI", "Breakpoint", "Overflow",
    "Bound Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coproc Overrun", "Invalid TSS",
    "Segment Not Present", "Stack Fault", "General Protection",
    "Page Fault", "Reserved", "x87 FPU", "Alignment Check",
    "Machine Check", "SIMD FP", "Virtualization", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Security", "Reserved"
};

void exception_handler(uint64_t int_no, uint64_t err_code) {
    serial_puts("\n[!] EXCEPTION #");
    char buf[32];
    serial_puts(uitoa(int_no, buf));
    serial_puts(" — ");
    if (int_no < 32) serial_puts(exception_names[int_no]);
    serial_puts(" (err: ");
    serial_puts(uxtoa(err_code, buf));
    serial_puts(")\n");
    if (int_no == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        serial_puts("[!] Page fault at 0x");
        serial_puts(uxtoa(cr2, buf));
        serial_puts("\n");
    }
    serial_puts("[!] Halting.\n");
    for (;;) __asm__ volatile ("hlt");
}

void irq_default_handler(uint8_t irq) {
    if (irq == 0) {
        sched_tick();
    } else if (irq == 1) {
        /* Phase 5: keyboard scancode → ASCII → ring buffer */
        uint8_t sc = inb(0x60);
        if (sc == 0x2A || sc == 0x36) {
            /* Left Shift or Right Shift pressed */
            shift_pressed = 1;
        } else if (sc == 0xAA || sc == 0xB6) {
            /* Left Shift or Right Shift released */
            shift_pressed = 0;
        } else if (!(sc & 0x80) && sc < 128) {  /* make code only */
            char c = shift_pressed ? scancode_shift_map[sc] : scancode_map[sc];
            if (c) {
                int next_tail = (kb_tail + 1) % KB_BUF_SIZE;
                if (next_tail != kb_head) {  /* buffer not full */
                    kb_buffer[kb_tail] = c;
                    kb_tail = next_tail;
                }
            }
        }
    }
    pic_eoi(irq);
}

/* ===== Syscall handler (Phase 5: stable API) ===== */
/* ===== Phase 6: User pointer validation ===== */

/* User memory range: code 0x00800000, stack 0x00A00000-0x00C00000,
 * heap 0x01000000-0x01400000. We accept any address in [0x00800000, USER_HEAP_MAX). */
#define USER_ADDR_MIN 0x00800000ULL

/* Validate that [addr, addr+len) is in user space and the pages are mapped.
 * need_write: if 1, require PTE_WRITABLE in addition to PTE_PRESENT|PTE_USER.
 * Returns 1 if valid, 0 otherwise. */
static int validate_user_ptr(uint64_t addr, uint32_t len, int need_write) {
    if (addr < USER_ADDR_MIN || addr >= USER_HEAP_MAX)
        return 0;
    if (len > 0 && (addr + len) > USER_HEAP_MAX)
        return 0;

    /* Must have a current user process with its own CR3 */
    if (!sched_current || sched_current->cr3 == 0)
        return 0;

    uint64_t cr3 = sched_current->cr3;

    /* Check every page that [addr, addr+len) spans */
    uint64_t start_page = addr & ~0xFFFULL;
    uint64_t end_addr = (len == 0) ? addr : addr + len - 1;
    for (uint64_t va = start_page; va <= end_addr; va += PAGE_SIZE) {
        uint64_t pte = get_page(cr3, va);
        if (!(pte & PTE_PRESENT))
            return 0;
        if (!(pte & PTE_USER))
            return 0;
        if (need_write && !(pte & PTE_WRITABLE))
            return 0;
        if (va == end_addr) break;  /* avoid overflow on last iteration */
    }
    return 1;
}

/* Safely copy a null-terminated string from user space to a kernel buffer.
 * Validates each page boundary. Returns string length, or -1 on error. */
static int copy_str_from_user(char *dst, uint64_t src, int max_len) {
    if (src < USER_ADDR_MIN || src >= USER_HEAP_MAX)
        return -1;
    if (!sched_current || sched_current->cr3 == 0)
        return -1;

    uint64_t cr3 = sched_current->cr3;
    int i;
    for (i = 0; i < max_len; i++) {
        /* Check page mapping at each page boundary */
        if ((i == 0) || (((src + i) & 0xFFF) == 0)) {
            uint64_t pte = get_page(cr3, src + i);
            if (!(pte & PTE_PRESENT) || !(pte & PTE_USER))
                return -1;
        }
        char c = *(volatile char *)(unsigned long)(src + i);
        dst[i] = c;
        if (c == '\0')
            return i;
    }
    dst[max_len - 1] = '\0';
    return max_len - 1;
}

/* RAX=number, RDI=arg1, RSI=arg2, RDX=arg3 (Phase 6) */
void syscall_handler(struct registers *regs) {
    switch (regs->rax) {
        case 0: /* write(fd, buf, len) */
            serial_puts((const char *)(unsigned long)regs->rdi);
            regs->rax = regs->rsi;
            break;
        case 1: /* exit(status) */
            {
                char buf[32];
                serial_puts("[+] Process PID=");
                serial_puts(uitoa(proc_current_pid(), buf));
                serial_puts(" exited normally\n");
            }
            proc_terminate(proc_current_pid());
            __asm__ volatile ("sti");
            for (;;) __asm__ volatile ("hlt");
            break;
        case 2: /* getpid() */
            regs->rax = (uint64_t)proc_current_pid();
            break;
        case 3: /* sbrk(increment) */
            {
                int64_t incr = (int64_t)regs->rdi;
                uint64_t old_limit = sched_current->user_heap_limit;
                if (incr == 0) {
                    regs->rax = old_limit;
                    break;
                }
                uint64_t new_limit = old_limit + (uint64_t)incr;
                if (new_limit < sched_current->user_heap_base ||
                    new_limit > USER_HEAP_MAX) {
                    regs->rax = (uint64_t)-1;
                    break;
                }
                sched_current->user_heap_limit = new_limit;
                regs->rax = old_limit;
            }
            break;
        case 4: /* read(buf, len) — non-blocking keyboard read */
            {
                char *ubuf = (char *)(unsigned long)regs->rdi;
                uint64_t ulen = regs->rsi;
                uint64_t nread = 0;
                while (nread < ulen && kb_head != kb_tail) {
                    ubuf[nread++] = kb_buffer[kb_head];
                    kb_head = (kb_head + 1) % KB_BUF_SIZE;
                }
                regs->rax = nread;
            }
            break;
        case 5: /* sleep(ticks) — suspend until system_ticks + ticks */
            {
                uint64_t ticks = regs->rdi;
                if (ticks == 0) {
                    sched_yield();
                } else {
                    sched_current->sleep_until = system_ticks + ticks;
                    sched_current->state = SLEEPING;
                    sched_yield();
                }
                regs->rax = 0;
            }
            break;
        case 6: /* yield() — give up CPU time slice */
            regs->rax = 0;
            sched_yield();
            break;
        case 7: /* getpages() — return free page count */
            regs->rax = count_free_pages();
            break;

        /* ===== Phase 6: Filesystem syscalls ===== */

        case 8: { /* open(path) → fd  —  RDI = user path pointer */
            uint64_t path_user = regs->rdi;
            if (!validate_user_ptr(path_user, 1, 0)) {
                regs->rax = (uint64_t)(int64_t)VFS_ERR_NOT_FOUND;
                break;
            }
            char kpath[64];
            int plen = copy_str_from_user(kpath, path_user, sizeof(kpath));
            if (plen < 0) {
                regs->rax = (uint64_t)(int64_t)VFS_ERR_NOT_FOUND;
                break;
            }
            regs->rax = (uint64_t)(int64_t)vfs_open(kpath);
            break;
        }

        case 9: { /* close(fd) —  RDI = fd */
            int fd = (int)(int64_t)regs->rdi;
            regs->rax = (uint64_t)(int64_t)vfs_close(fd);
            break;
        }

        case 10: { /* read(fd, buf, len) → bytes read
                   * RDI = fd, RSI = user buffer, RDX = length */
            int fd = (int)(int64_t)regs->rdi;
            uint64_t buf_user = regs->rsi;
            uint32_t len = (uint32_t)regs->rdx;

            if (len == 0) {
                regs->rax = 0;
                break;
            }
            if (!validate_user_ptr(buf_user, len, 1)) {
                regs->rax = (uint64_t)(int64_t)-1;  /* invalid pointer */
                break;
            }

            /* Read via kernel buffer, then copy to user space */
            static uint8_t kbuf[512];
            if (len > sizeof(kbuf))
                len = sizeof(kbuf);

            int n = vfs_read(fd, kbuf, len);
            if (n < 0) {
                regs->rax = (uint64_t)(int64_t)n;
                break;
            }
            /* Copy read data to user space */
            uint8_t *ubuf = (uint8_t *)(unsigned long)buf_user;
            for (int i = 0; i < n; i++)
                ubuf[i] = kbuf[i];
            regs->rax = (uint64_t)n;
            break;
        }

        default:
            regs->rax = (uint64_t)-1;  /* error: unknown syscall */
            break;
    }
}

/* ===== ISR handler ===== */
void isr_handler(struct registers *regs) {
    if (regs->int_no < 32) {
        if (regs->int_no == 14 && test_fault_addr != 0 && !(regs->err_code & 4)) {
            uint64_t cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            if (cr2 == test_fault_addr) {
                test_fault_caught = 1;
                test_fault_addr = 0;
                regs->rip += 3;
                return;
            }
        }

        if (regs->int_no == 14 && (regs->err_code & 4)) {
            uint64_t cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            char buf[32];

            if (!(regs->err_code & 1)) {
                struct task *t = (struct task *)sched_current;

                if (t->is_user) {
                    if (cr2 >= t->user_heap_base && cr2 < t->user_heap_limit) {
                        uint64_t pa = alloc_page();
                        if (pa) {
                            int r = map_page(t->cr3, cr2 & ~0xFFFULL, pa,
                                             PTE_PRESENT | PTE_WRITABLE | PTE_USER);
                            if (r == 0) {
                                __asm__ volatile("invlpg (%0)" : : "r"(cr2 & ~0xFFFULL) : "memory");
                                return;
                            }
                            free_page(pa);
                        }
                    }

                    if (cr2 >= USER_STACK_BASE && cr2 < t->user_stack_limit) {
                        uint64_t va = cr2 & ~0xFFFULL;
                        int ok = 1;
                        while (va < t->user_stack_limit) {
                            uint64_t pa = alloc_page();
                            if (!pa) { ok = 0; break; }
                            int r = map_page(t->cr3, va, pa,
                                             PTE_PRESENT | PTE_WRITABLE | PTE_USER);
                            if (r != 0) { free_page(pa); ok = 0; break; }
                            __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
                            va += PAGE_SIZE;
                        }
                        if (ok) {
                            t->user_stack_limit = cr2 & ~0xFFFULL;
                            return;
                        }
                    }
                }
            }

            serial_puts("\n[+] Page fault in Ring 3 (CR2=0x");
            serial_puts(uxtoa(cr2, buf));
            serial_puts(")\n");
            serial_puts("[+] Process PID=");
            serial_puts(uitoa(proc_current_pid(), buf));
            serial_puts(" terminated (page fault)\n");
            proc_terminate(proc_current_pid());
            __asm__ volatile ("sti");
            for (;;) __asm__ volatile ("hlt");
        }

        exception_handler(regs->int_no, regs->err_code);
    }
    else if (regs->int_no == 128)
        syscall_handler(regs);
}

void irq_handler(struct registers *regs) {
    uint8_t irq = regs->int_no - 32;
    irq_default_handler(irq);
}
