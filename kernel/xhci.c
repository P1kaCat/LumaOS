/* xhci.c — xHCI USB Host Controller Driver (Phase 7b.1)
 *
 * Phase 7b.1: Controller discovery + BAR analysis + MMIO mapping + capability reading.
 *
 * The xHCI controller is discovered via PCI class 0C/03/30.
 * On QEMU, `-device qemu-xhci` creates a controller
 * (Red Hat vendor 0x1B36, device 0x000D).
 *
 * The controller's registers are MMIO-mapped at BAR0.
 * OVMF may assign the BAR above 4 GB (e.g., 0x800000000 for a 64-bit BAR).
 * The kernel identity-maps the first 4 GB with 2 MB pages, but
 * map_page() can create 4 KB mappings at any address by walking
 * the 4-level page table hierarchy and creating missing tables.
 *
 * We identity-map the BAR's physical address range using map_page(),
 * then access the xHCI registers through the mapped virtual address.
 *
 *   PCI BAR (physical) → map_page(4KB) → identity VA → xHCI registers
 */
#include "xhci.h"
#include "cpu.h"
#include "mem.h"

/* ===== Helpers ===== */

static char *uitoa_local(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0;
    while (n) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

static char *uxtoa_pad(uint64_t n, char *buf, int width) {
    char tmp[32]; int i=0;
    const char *h="0123456789ABCDEF";
    if (!n) { tmp[i++]='0'; }
    while (n) { tmp[i++]=h[n&0xF]; n>>=4; }
    while (i < width) tmp[i++]='0';
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

/* MMIO register access — direct pointer dereference.
 * The kernel identity-maps MMIO regions via map_page() (4 KB pages),
 * so the physical address is also the virtual address. */
static uint32_t xhci_read32(uint64_t base, uint32_t offset) {
    return *(volatile uint32_t *)(unsigned long)(base + offset);
}

static uint16_t xhci_read16(uint64_t base, uint32_t offset) {
    return *(volatile uint16_t *)(unsigned long)(base + offset);
}

static uint8_t xhci_read8(uint64_t base, uint32_t offset) {
    return *(volatile uint8_t *)(unsigned long)(base + offset);
}

/* Find first set bit (1-indexed), like libc ffs() */
static int xhci_ffs(uint32_t x) {
    if (!x) return 0;
    int n = 1;
    while (!(x & 1)) { x >>= 1; n++; }
    return n;
}

/* ===== BAR sizing ===== */

/* Size a PCI BAR (32-bit or 64-bit).
 * Writes 0xFFFFFFFF to the BAR(s), reads back the mask, restores original.
 * Returns the BAR size in bytes, or 0 if the BAR is unused.
 * For 64-bit BARs, sizes both BAR0 and BAR1. */
static uint64_t xhci_bar_size(struct pci_device *dev) {
    uint8_t off0 = 0x10;  /* BAR0 */
    uint8_t off1 = 0x14;  /* BAR1 (upper 32 bits for 64-bit BAR) */

    /* Save original values */
    uint32_t orig0 = pci_config_read32(dev->bus, dev->device, dev->func, off0);
    uint32_t orig1 = pci_config_read32(dev->bus, dev->device, dev->func, off1);

    /* Write all 1s to both registers */
    pci_config_write32(dev->bus, dev->device, dev->func, off0, 0xFFFFFFFF);
    pci_config_write32(dev->bus, dev->device, dev->func, off1, 0xFFFFFFFF);

    /* Read back the size masks */
    uint32_t mask_lo = pci_config_read32(dev->bus, dev->device, dev->func, off0);
    uint32_t mask_hi = pci_config_read32(dev->bus, dev->device, dev->func, off1);

    /* Restore original values */
    pci_config_write32(dev->bus, dev->device, dev->func, off0, orig0);
    pci_config_write32(dev->bus, dev->device, dev->func, off1, orig1);

    /* Clear lower 4 bits (type, prefetchable, enable) */
    mask_lo &= 0xFFFFFFF0u;

    if (mask_lo == 0 && mask_hi == 0) return 0;  /* BAR unused */

    /* Combined 64-bit mask → size = ~mask + 1 */
    uint64_t mask = ((uint64_t)mask_hi << 32) | mask_lo;
    return ~mask + 1;
}

/* ===== Init ===== */

void xhci_init(void) {
    char buf[16];

    serial_puts("\n[*] Initializing xHCI USB host controller...\n");

    /* --- 1. Find the xHCI controller via PCI class --- */
    struct pci_device *xhci = pci_find_class(0x0C, 0x03, 0x30);
    if (!xhci) {
        serial_puts("  [!] No xHCI controller found (PCI 0C/03/30)\n");
        serial_puts("  [!] USB support not available\n");
        return;
    }

    serial_puts("  [+] xHCI controller found at ");
    serial_puts(uxtoa_pad(xhci->bus, buf, 2)); serial_puts(":");
    serial_puts(uxtoa_pad(xhci->device, buf, 2)); serial_puts(".");
    serial_puts(uitoa_local(xhci->func, buf));
    serial_puts(" vendor=");
    serial_puts(uxtoa_pad(xhci->vendor, buf, 4));
    serial_puts(" device=");
    serial_puts(uxtoa_pad(xhci->device_id, buf, 4));
    serial_puts("\n");

    /* --- 2. Analyze BAR0 --- */
    uint32_t bar0_raw = xhci->bars[0];

    if (!bar0_raw) {
        serial_puts("  [!] BAR0 is zero — device not configured by BIOS\n");
        return;
    }

    /* Decode BAR type bits */
    int is_io       = bar0_raw & 0x01;            /* bit 0: 0=Memory, 1=I/O */
    int bar_type    = (bar0_raw >> 1) & 0x03;      /* bits 2:1: 00=32-bit, 10=64-bit (for Memory) */
    int is_prefetch = (bar0_raw >> 3) & 0x01;      /* bit 3: prefetchable */
    int is_64bit    = (!is_io && bar_type == 2);   /* 64-bit memory BAR */

    /* Compute full MMIO base address */
    uint64_t mmio_base = bar0_raw & 0xFFFFFFF0u;  /* clear type bits */
    if (is_64bit) {
        uint32_t bar1_raw = xhci->bars[1];
        mmio_base |= (uint64_t)bar1_raw << 32;
    }

    /* Print BAR analysis */
    serial_puts("  BAR0: ");
    serial_puts(is_io ? "I/O" : "Memory");
    serial_puts(is_64bit ? " 64-bit" : " 32-bit");
    serial_puts(is_prefetch ? " prefetchable" : " non-prefetchable");
    serial_puts("\n");
    serial_puts("    raw=0x");
    serial_puts(uxtoa_pad(bar0_raw, buf, 8));
    if (is_64bit) {
        serial_puts(" BAR1 raw=0x");
        serial_puts(uxtoa_pad(xhci->bars[1], buf, 8));
    }
    serial_puts("\n");
    serial_puts("    base=0x");
    serial_puts(uxtoa_pad(mmio_base, buf, 16));
    serial_puts("\n");

    /* --- 3. Size the BAR --- */
    uint64_t bar_size = xhci_bar_size(xhci);
    uint64_t num_pages = (bar_size + PAGE_SIZE - 1) / PAGE_SIZE;

    serial_puts("    size=0x");
    serial_puts(uxtoa_pad(bar_size, buf, 8));
    serial_puts(" (");
    serial_puts(uitoa_local(bar_size, buf));
    serial_puts(" bytes, ");
    serial_puts(uitoa_local(num_pages, buf));
    serial_puts(" page");
    serial_puts(num_pages > 1 ? "s" : "");
    serial_puts(")\n");

    if (!bar_size) {
        serial_puts("  [!] BAR0 size is zero — BAR not implemented?\n");
        return;
    }

    /* --- 4. Enable the PCI device --- */
    pci_enable_device(xhci);
    serial_puts("  [+] PCI device enabled (Memory Space + Bus Master)\n");

    /* --- 5. Identity-map the BAR's physical address range --- *
     *
     * The kernel's paging_init() identity-maps the first 4 GB with 2 MB
     * large pages. For BARs above 4 GB (like 0x800000000), we extend the
     * page table hierarchy by creating 4 KB identity mappings (VA = PA)
     * using map_page(). This walks PML4 → PDPT → PD → PT and allocates
     * missing tables via alloc_page().
     *
     * MMIO pages are supervisor-only (no PTE_USER) and writable.
     */
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    cr3 &= ~0xFFFULL;

    int mapped = 0;
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t pa = mmio_base + i * PAGE_SIZE;
        int ret = map_page(cr3, pa, pa, PTE_PRESENT | PTE_WRITABLE);
        if (ret != 0) {
            serial_puts("  [!] map_page() failed at 0x");
            serial_puts(uxtoa_pad(pa, buf, 16));
            serial_puts(" (ret=");
            serial_puts(uitoa_local(ret, buf));
            serial_puts(")\n");
            break;
        }
        mapped++;
    }

    if (mapped == 0) {
        serial_puts("  [!] Failed to map any MMIO pages\n");
        return;
    }

    serial_puts("  [+] MMIO identity-mapped: ");
    serial_puts(uitoa_local(mapped, buf));
    serial_puts(" page");
    serial_puts(mapped > 1 ? "s" : "");
    serial_puts(" at 0x");
    serial_puts(uxtoa_pad(mmio_base, buf, 16));
    serial_puts("\n");

    /* --- 6. Read xHCI capability registers --- */
    uint8_t  cap_length = xhci_read8(mmio_base, XHCI_CAPLENGTH);
    uint16_t hci_version = xhci_read16(mmio_base, XHCI_HCIVERSION);
    uint32_t hcs_params1 = xhci_read32(mmio_base, XHCI_HCSPARAMS1);
    uint32_t hcs_params2 = xhci_read32(mmio_base, XHCI_HCSPARAMS2);
    uint32_t hcs_params3 = xhci_read32(mmio_base, XHCI_HCSPARAMS3);
    uint32_t hcc_params1 = xhci_read32(mmio_base, XHCI_HCCPARAMS1);
    uint32_t dboff = xhci_read32(mmio_base, XHCI_DBOFF);
    uint32_t rtsoff = xhci_read32(mmio_base, XHCI_RTSOFF);

    /* Parse HCSPARAMS1 */
    uint32_t max_slots = (hcs_params1 & XHCI_HCS1_MAX_SLOTS) >> 24;
    uint32_t max_intrs = (hcs_params1 & XHCI_HCS1_MAX_INTRS) >> 16;
    uint32_t max_ports = (hcs_params1 & XHCI_HCS1_MAX_PORTS);

    /* Parse HCCPARAMS1 */
    int ac64 = (hcc_params1 & XHCI_HCC1_AC64) ? 1 : 0;
    int csz = (hcc_params1 & XHCI_HCC1_CSZ) ? 1 : 0;
    int ppc = (hcc_params1 & XHCI_HCC1_PPC) ? 1 : 0;

    serial_puts("  Capability registers:\n");
    serial_puts("    CAPLENGTH=");
    serial_puts(uitoa_local(cap_length, buf));
    serial_puts(" (operational regs at +0x");
    serial_puts(uxtoa_pad(cap_length, buf, 2));
    serial_puts(")\n");

    serial_puts("    HCIVERSION=0x");
    serial_puts(uxtoa_pad(hci_version >> 8, buf, 2));
    serial_puts(".");
    serial_puts(uxtoa_pad(hci_version & 0xFF, buf, 2));
    serial_puts("\n");

    serial_puts("    HCSPARAMS1: max_slots=");
    serial_puts(uitoa_local(max_slots, buf));
    serial_puts(" max_intrs=");
    serial_puts(uitoa_local(max_intrs, buf));
    serial_puts(" max_ports=");
    serial_puts(uitoa_local(max_ports, buf));
    serial_puts("\n");

    serial_puts("    HCSPARAMS2=0x");
    serial_puts(uxtoa_pad(hcs_params2, buf, 8));
    serial_puts(" HCSPARAMS3=0x");
    serial_puts(uxtoa_pad(hcs_params3, buf, 8));
    serial_puts("\n");

    serial_puts("    HCCPARAMS1: ");
    serial_puts("64-bit=");
    serial_puts(uitoa_local(ac64, buf));
    serial_puts(" ctx_size=");
    serial_puts(csz ? "64" : "32");
    serial_puts(" port_power=");
    serial_puts(uitoa_local(ppc, buf));
    serial_puts("\n");

    serial_puts("    DBOFF=0x");
    serial_puts(uxtoa_pad(dboff, buf, 8));
    serial_puts(" RTSOFF=0x");
    serial_puts(uxtoa_pad(rtsoff, buf, 8));
    serial_puts("\n");

    /* --- 7. Read operational registers (at MMIO base + CAPLENGTH) --- */
    uint64_t op_base = mmio_base + cap_length;
    uint32_t usbcmd = xhci_read32(op_base, XHCI_USBCMD);
    uint32_t usbsts = xhci_read32(op_base, XHCI_USBSTS);
    uint32_t pagesize = xhci_read32(op_base, XHCI_PAGESIZE);

    serial_puts("  Operational registers:\n");
    serial_puts("    USBCMD=0x");
    serial_puts(uxtoa_pad(usbcmd, buf, 8));
    serial_puts(" USBSTS=0x");
    serial_puts(uxtoa_pad(usbsts, buf, 8));
    serial_puts(usbsts & XHCI_USBSTS_HCH ? " (Halted)" : " (Running)");
    serial_puts("\n");

    serial_puts("    PAGESIZE=0x");
    serial_puts(uxtoa_pad(pagesize, buf, 8));
    if (pagesize) {
        serial_puts(" (");
        serial_puts(uitoa_local(1u << (xhci_ffs(pagesize) - 1), buf));
        serial_puts(" bytes)");
    }
    serial_puts("\n");

    serial_puts("[XHCI7b1] controller discovered\n");
}
