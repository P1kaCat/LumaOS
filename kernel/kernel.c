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
        __asm__ volatile ("hlt" ::: "memory");
    }
}
static void task2_main(void) {
    uint64_t count = 0; char buf[32];
    for (;;) {
        serial_puts("  [Task 2] tick "); serial_puts(uitoa(count++, buf)); serial_puts("\n");
        __asm__ volatile ("hlt" ::: "memory");
    }
}

void kernel_main(struct lumaos_handoff *ho) {
    if (ho->magic != LUMAOS_HANDOFF_MAGIC) {
        serial_puts("LumaOS: INVALID HANDOFF MAGIC\n");
        for (;;) __asm__ volatile ("hlt");
    }

    serial_puts("\n================================\n");
    serial_puts("  LumaOS Kernel — Phase 4 (Virtual Memory)\n");
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

    serial_puts("\n[*] Initializing page allocator...\n");
    page_allocator_init(ho);
    uint64_t p1 = alloc_page();
    uint64_t p2 = alloc_page();
    uint64_t p3 = alloc_page();
    serial_puts("  alloc_page() = 0x"); serial_puts(uxtoa(p1, buf)); serial_puts("\n");
    serial_puts("  alloc_page() = 0x"); serial_puts(uxtoa(p2, buf)); serial_puts("\n");
    serial_puts("  alloc_page() = 0x"); serial_puts(uxtoa(p3, buf)); serial_puts("\n");
    free_page(p2);
    serial_puts("  free_page(0x"); serial_puts(uxtoa(p2, buf)); serial_puts(")\n");
    uint64_t p4 = alloc_page();
    serial_puts("  alloc_page() = 0x"); serial_puts(uxtoa(p4, buf)); serial_puts(" (should reuse freed page)\n");
    serial_puts("  free pages: "); serial_puts(uitoa(count_free_pages(), buf)); serial_puts("\n");

    /* ---- Dynamic mapping test (map / write / read / unmap / PF) ---- */
    serial_puts("\n[*] Testing dynamic page mapping...\n");

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    cr3 &= ~0xFFFULL;

    uint64_t test_va = 0x100000000ULL;  /* 4GB — above existing 0-4GB mappings */
    uint64_t test_pa = alloc_page();
    serial_puts("  alloc_page() = 0x"); serial_puts(uxtoa(test_pa, buf)); serial_puts("\n");

    int mr = map_page(cr3, test_va, test_pa, PTE_PRESENT | PTE_WRITABLE);
    serial_puts("  map_page(0x100000000, 0x"); serial_puts(uxtoa(test_pa, buf));
    serial_puts(") = "); serial_puts(uitoa((uint64_t)mr, buf)); serial_puts(mr == 0 ? " (OK)\n" : " (FAIL)\n");

    volatile uint64_t *tptr = (volatile uint64_t *)(unsigned long)test_va;
    *tptr = 0xDEADBEEFC0FFEEULL;
    uint64_t tval = *tptr;
    serial_puts("  write/read 0x"); serial_puts(uxtoa(tval, buf));
    serial_puts(tval == 0xDEADBEEFC0FFEEULL ? " — OK\n" : " — MISMATCH\n");

    int ur = unmap_page(cr3, test_va);
    serial_puts("  unmap_page() = "); serial_puts(uitoa((uint64_t)ur, buf)); serial_puts(ur == 0 ? " (OK)\n" : " (FAIL)\n");

    uint64_t pte = get_page(cr3, test_va);
    serial_puts("  PTE after unmap = 0x"); serial_puts(uxtoa(pte, buf));
    serial_puts(pte == 0 ? " (unmapped)\n" : " (ERROR)\n");

    /* Access after unmap — should trigger page fault, caught by test handler */
    serial_puts("  accessing unmapped VA...\n");
    test_fault_addr = test_va;
    test_fault_caught = 0;
    uint64_t fault_val;
    __asm__ volatile("movq (%1), %0" : "=a"(fault_val) : "b"(test_va));
    serial_puts(test_fault_caught ? "  [+] Page fault caught, system continues\n" : "  [!] No fault (unexpected)\n");

    serial_puts("\n[*] Starting scheduler...\n");
    sched_init();
    task_create(task1_main, 1);
    task_create(task2_main, 2);
    pit_init(50);

    serial_puts("[+] Interrupts enabled (timer 50Hz + keyboard)\n");
    __asm__ volatile ("sti");

    serial_puts("\n[*] Setting up user processes (Ring 3)...\n");
    serial_puts("[+] Paging: kernel isolated, user @0x800000, stack @0xC00000, heap @0x1000000\n");

    uint64_t pages_before = count_free_pages();
    user_init(); /* creates 2 user processes */
    uint64_t pages_after_init = count_free_pages();
    serial_puts("  Free pages: before=");
    serial_puts(uitoa(pages_before, buf));
    serial_puts(" after_init=");
    serial_puts(uitoa(pages_after_init, buf));
    serial_puts("\n");

    serial_puts("[+] Scheduler running (kernel tasks + 2 user processes)\n");

    /* Wait for all user processes to terminate, then check for page leaks.
       The "memory" clobber on hlt is critical: without it, -O2 may cache
       reads from the tasks array across hlt, since the compiler doesn't
       know that the interrupt handler modifies tasks[].state via
       proc_terminate(). */
    volatile int cleanup_test_done = 0;
    for (;;) {
        __asm__ volatile ("hlt" ::: "memory");
        if (!cleanup_test_done) {
            int active = count_active_user_procs();
            if (active == 0) {
                cleanup_test_done = 1;
                uint64_t pages_final = count_free_pages();
                serial_puts("\n[*] All user processes terminated\n");
                serial_puts("  Free pages: before=");
                serial_puts(uitoa(pages_before, buf));
                serial_puts(" final=");
                serial_puts(uitoa(pages_final, buf));
                serial_puts("\n");
                if (pages_final >= pages_before) {
                    serial_puts("  [+] No page leak (pages_final >= pages_before)\n");
                } else {
                    serial_puts("  [!] Page leak detected!\n");
                }
                serial_puts("[+] Phase 4 complete: protection, cleanup, heap, lazy alloc, stack\n");
            }
        }
    }
}
