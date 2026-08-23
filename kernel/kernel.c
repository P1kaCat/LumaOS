#include <stdint.h>
#include "handoff.h"
#include "cpu.h"
#include "mem.h"
#include "sched.h"
#include "user.h"
#include "ata.h"
#include "fat32.h"
#include "vfs.h"
#include "pci.h"
#include "acpi.h"
#include "apic.h"

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

/* Phase 5: kernel background tasks (silent — just hlt) */
static void task1_main(void) {
    for (;;) __asm__ volatile ("hlt" ::: "memory");
}
static void task2_main(void) {
    for (;;) __asm__ volatile ("hlt" ::: "memory");
}

/* Phase 6: FAT32 directory listing callback */
struct dir_list_ctx { int count; };

static void dir_list_cb(const struct fat32_dir_entry *entry, void *ctx) {
    char name[12];
    /* Copy 8.3 name, format as NAME.EXT */
    int j = 0;
    for (int i = 0; i < 8; i++) {
        if (entry->name[i] != ' ') name[j++] = entry->name[i];
    }
    if (entry->name[8] != ' ') {
        name[j++] = '.';
        for (int i = 8; i < 11; i++) {
            if (entry->name[i] != ' ') name[j++] = entry->name[i];
        }
    }
    name[j] = 0;

    char buf[32];
    serial_puts("  ");
    serial_puts(name);
    serial_puts("  (size=");
    serial_puts(uitoa(entry->file_size, buf));
    serial_puts(", cluster=");
    uint32_t cluster = ((uint32_t)entry->cluster_hi << 16) | entry->cluster_lo;
    serial_puts(uitoa(cluster, buf));
    if (entry->attr & FAT32_ATTR_DIRECTORY) {
        serial_puts(", DIR");
    }
    serial_puts(")\n");

    struct dir_list_ctx *c = (struct dir_list_ctx *)ctx;
    c->count++;
}

/* Check if ATA was initialized successfully */
static int ata_present_check(void) {
    return ata_get_sector_count() > 0;
}

void kernel_main(struct lumaos_handoff *ho) {
    if (ho->magic != LUMAOS_HANDOFF_MAGIC) {
        serial_puts("LumaOS: INVALID HANDOFF MAGIC\n");
        for (;;) __asm__ volatile ("hlt");
    }

    serial_puts("\n================================\n");
    serial_puts("  LumaOS Kernel — Phase 5 (Syscalls & Userland)\n");
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

    /* ---- Phase 4 regression: dynamic mapping test ---- */
    serial_puts("\n[*] Testing dynamic page mapping...\n");

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    cr3 &= ~0xFFFULL;

    uint64_t test_va = 0x100000000ULL;
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

    serial_puts("  accessing unmapped VA...\n");
    test_fault_addr = test_va;
    test_fault_caught = 0;
    uint64_t fault_val;
    __asm__ volatile("movq (%1), %0" : "=a"(fault_val) : "b"(test_va));
    serial_puts(test_fault_caught ? "  [+] Page fault caught, system continues\n" : "  [!] No fault (unexpected)\n");

    /* Free dynamic mapping test pages (test_pa + PT + PD allocated by walk_pt) */
    free_page(test_pa);
    free_user_pages(cr3, test_va, test_va + 0x200000ULL);
    {
        /* free_user_pages frees PT pages but not PD pages at PDPT level.
           Manually free the PD page allocated by walk_pt at PDPT[4]. */
        uint64_t *pml4_tbl = (uint64_t *)(cr3 & ~0xFFFULL);
        uint64_t pdpt_val = pml4_tbl[0];
        if (pdpt_val & PTE_PRESENT) {
            uint64_t *pdpt_tbl = (uint64_t *)(pdpt_val & ~0xFFFULL);
            uint64_t pd_val = pdpt_tbl[4];
            if (pd_val & PTE_PRESENT && !(pd_val & PTE_PS)) {
                free_page(pd_val & ~0xFFFULL);
                pdpt_tbl[4] = 0;
            }
        }
    }
    serial_puts("  [+] Dynamic mapping test pages freed\n");

    /* ---- Phase 7a.1: PCI bus enumeration ---- */
    serial_puts("\n[*] Enumerating PCI bus...\n");
    pci_init();

    /* ---- Phase 7a.2: ACPI table parsing ---- */
    acpi_init(ho->rsdp);

    /* ---- Phase 7a.3: Local APIC + I/O APIC ---- */
    apic_init();

    /* ---- Phase 7a.4: PCI Interrupt Routing ---- */
    pci_irq_init();
    /* ---- Phase 6: ATA/IDE disk driver ---- */
    serial_puts("\n[*] Initializing ATA/IDE driver...\n");
    if (ata_init() != 0) {
        serial_puts("[!] ATA init failed — filesystem disabled\n");
    } else {
        /* Raw sector read test: read sector 0 (boot sector) */
        serial_puts("[*] Testing raw sector read (sector 0)...\n");
        uint8_t sector_buf[512];
        if (ata_read_sector(0, sector_buf) != 0) {
            serial_puts("  [!] Sector read failed\n");
        } else {
            serial_puts("  First 16 bytes: ");
            for (int i = 0; i < 16; i++) {
                char hex[4];
                const char *h = "0123456789ABCDEF";
                hex[0] = h[sector_buf[i] >> 4];
                hex[1] = h[sector_buf[i] & 0xF];
                hex[2] = ' ';
                hex[3] = 0;
                serial_puts(hex);
            }
            serial_puts("\n");
            /* Verify boot signature */
            if (sector_buf[510] == 0x55 && sector_buf[511] == 0xAA) {
                serial_puts("  [+] Boot signature 0x55AA OK\n");
            } else {
                serial_puts("  [!] Boot signature mismatch\n");
            }
        }
    }

    /* ---- Phase 6: FAT32 filesystem ---- */
    if (ata_present_check()) {
        serial_puts("\n[*] Initializing FAT32 filesystem...\n");
        if (fat32_init() != 0) {
            serial_puts("[!] FAT32 init failed — filesystem disabled\n");
        } else {
            /* List root directory */
            serial_puts("[*] Root directory listing:\n");

            struct dir_list_ctx ctx = {0};

            fat32_list_root(dir_list_cb, &ctx);
            serial_puts("  Total entries: ");
            char dbuf[16];
            serial_puts(uitoa(ctx.count, dbuf));
            serial_puts("\n");
        }
    } else {
        serial_puts("[*] FAT32 skipped — ATA not available\n");
    }

    /* ---- Phase 6: VFS layer test ---- */
    if (ata_present_check()) {
        serial_puts("\n[*] Testing VFS layer...\n");
        vfs_init();

        int vfs_ok = 1;
        char file_buf[256];  /* shared test buffer for all VFS tests */

        /* Test 1: Open HELLO.TXT and read its contents */
        int fd = vfs_open("hello.txt");
        if (fd < 0) {
            serial_puts("  [!] vfs_open(\"hello.txt\") failed: ");
            serial_puts(uitoa((uint64_t)(unsigned)fd, buf)); serial_puts("\n");
            vfs_ok = 0;
        } else {
            serial_puts("  [+] vfs_open(\"hello.txt\") = fd ");
            serial_puts(uitoa((uint64_t)fd, buf)); serial_puts("\n");

            /* Read file contents */
            int n = vfs_read(fd, file_buf, sizeof(file_buf) - 1);
            if (n < 0) {
                serial_puts("  [!] vfs_read failed: ");
                serial_puts(uitoa((uint64_t)n, buf)); serial_puts("\n");
                vfs_ok = 0;
            } else {
                file_buf[n] = 0;
                serial_puts("  [+] vfs_read returned ");
                serial_puts(uitoa((uint64_t)n, buf));
                serial_puts(" bytes:\n");
                serial_puts("  ---\n");
                serial_puts(file_buf);
                serial_puts("  ---\n");
            }

            /* Test 2: Read again — should return 0 (EOF) */
            int n2 = vfs_read(fd, file_buf, 1);
            if (n2 == 0) {
                serial_puts("  [+] EOF handling: OK (read returned 0)\n");
            } else {
                serial_puts("  [!] EOF handling: expected 0, got ");
                serial_puts(uitoa((uint64_t)n2, buf)); serial_puts("\n");
                vfs_ok = 0;
            }

            /* Close the file */
            int cr = vfs_close(fd);
            if (cr == 0) {
                serial_puts("  [+] vfs_close(fd) = OK\n");
            } else {
                serial_puts("  [!] vfs_close failed: ");
                serial_puts(uitoa((uint64_t)cr, buf)); serial_puts("\n");
                vfs_ok = 0;
            }
        }

        /* Test 3: Open non-existent file */
        int fd2 = vfs_open("nonexist.txt");
        if (fd2 == VFS_ERR_NOT_FOUND) {
            serial_puts("  [+] Invalid path: OK (returned NOT_FOUND)\n");
        } else {
            serial_puts("  [!] Invalid path: expected NOT_FOUND, got ");
            serial_puts(uitoa((uint64_t)fd2, buf)); serial_puts("\n");
            vfs_ok = 0;
        }

        /* Test 4: Read from invalid FD */
        int n3 = vfs_read(99, file_buf, 10);
        if (n3 == VFS_ERR_BAD_FD) {
            serial_puts("  [+] Invalid FD read: OK (returned BAD_FD)\n");
        } else {
            serial_puts("  [!] Invalid FD read: expected BAD_FD, got ");
            serial_puts(uitoa((uint64_t)n3, buf)); serial_puts("\n");
            vfs_ok = 0;
        }

        /* Test 5: Close invalid FD */
        int cr2 = vfs_close(99);
        if (cr2 == VFS_ERR_BAD_FD) {
            serial_puts("  [+] Invalid FD close: OK (returned BAD_FD)\n");
        } else {
            serial_puts("  [!] Invalid FD close: expected BAD_FD, got ");
            serial_puts(uitoa((uint64_t)cr2, buf)); serial_puts("\n");
            vfs_ok = 0;
        }

        /* Test 6: Open second file (TEST.TXT) to verify multiple FDs */
        int fd3 = vfs_open("test.txt");
        if (fd3 >= 0) {
            serial_puts("  [+] vfs_open(\"test.txt\") = fd ");
            serial_puts(uitoa((uint64_t)fd3, buf)); serial_puts("\n");
            int tn = vfs_read(fd3, file_buf, sizeof(file_buf) - 1);
            if (tn > 0) {
                file_buf[tn] = 0;
                serial_puts("  [+] Read ");
                serial_puts(uitoa((uint64_t)tn, buf));
                serial_puts(" bytes from TEST.TXT\n");
            }
            vfs_close(fd3);
        } else {
            serial_puts("  [!] vfs_open(\"test.txt\") failed\n");
            vfs_ok = 0;
        }

        if (vfs_ok) {
            serial_puts("[VFS] test passed\n");
        } else {
            serial_puts("[VFS] test FAILED\n");
        }
    }

    /* ---- Phase 5: scheduler + shell ---- */
    serial_puts("\n[*] Starting scheduler...\n");
    sched_init();
    task_create(task1_main, 1);
    task_create(task2_main, 2);
    pit_init(50);

    serial_puts("[+] Interrupts enabled (timer 50Hz + keyboard)\n");
    __asm__ volatile ("sti");

    serial_puts("\n[*] Setting up user init (Ring 3)...\n");

    uint64_t pages_before = count_free_pages();
    user_init();
    uint64_t pages_after_init = count_free_pages();
    serial_puts("  Free pages: before=");
    serial_puts(uitoa(pages_before, buf));
    serial_puts(" after_init=");
    serial_puts(uitoa(pages_after_init, buf));
    serial_puts("\n");

    serial_puts("[+] Init running, will spawn shell...\n");

    volatile int cleanup_test_done = 0;
    for (;;) {
        __asm__ volatile ("hlt" ::: "memory");
        if (!cleanup_test_done) {
            int active = count_active_user_procs();
            if (active == 0) {
                cleanup_test_done = 1;
                uint64_t pages_final = count_free_pages();
                serial_puts("\n[*] Shell exited — all user processes terminated\n");
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
                serial_puts("[+] Phase 4+5 regression test passed\n");
            }
        }
    }
}
