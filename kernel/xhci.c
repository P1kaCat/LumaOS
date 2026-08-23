/* xhci.c — xHCI USB Host Controller Driver (Phase 7b.1)
 *
 * Phase 7b.1: controller discovery, BAR analysis, MMIO mapping and
 * capability register reading.
 */
#include "xhci.h"
#include "cpu.h"
#include "mem.h"

static char *uitoa_local(uint64_t n, char *buf) {
    if (!n) { buf[0] = '0'; buf[1] = 0; return buf; }
    char tmp[32]; int i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
    return buf;
}

static char *uxtoa_pad(uint64_t n, char *buf, int width) {
    char tmp[32]; int i = 0;
    const char *hex = "0123456789ABCDEF";
    if (!n) tmp[i++] = '0';
    while (n) { tmp[i++] = hex[n & 0xF]; n >>= 4; }
    while (i < width) tmp[i++] = '0';
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
    return buf;
}

static uint32_t xhci_read32(uint64_t base, uint32_t off) {
    return *(volatile uint32_t *)(unsigned long)(base + off);
}

static uint16_t xhci_read16(uint64_t base, uint32_t off) {
    return *(volatile uint16_t *)(unsigned long)(base + off);
}

static uint8_t xhci_read8(uint64_t base, uint32_t off) {
    return *(volatile uint8_t *)(unsigned long)(base + off);
}

static int xhci_ffs(uint32_t x) {
    if (!x) return 0;
    int n = 1;
    while (!(x & 1)) { x >>= 1; n++; }
    return n;
}

/*
 * Probe BAR0 without touching BAR1 unless BAR0 explicitly advertises a
 * 64-bit memory BAR.  This matters because BAR1 is a completely separate
 * BAR on 32-bit devices.
 */
static uint64_t xhci_bar_size(struct pci_device *dev) {
    uint32_t orig0 = pci_config_read32(dev->bus, dev->device, dev->func, 0x10);
    if (!orig0) return 0;

    /* xHCI uses a memory BAR. I/O BARs cannot describe its MMIO registers. */
    if (orig0 & 0x1) return 0;

    uint32_t type = (orig0 >> 1) & 0x3;
    int is_64 = (type == 2);
    uint32_t orig1 = 0;

    if (is_64)
        orig1 = pci_config_read32(dev->bus, dev->device, dev->func, 0x14);

    pci_config_write32(dev->bus, dev->device, dev->func, 0x10, 0xFFFFFFFFu);
    if (is_64)
        pci_config_write32(dev->bus, dev->device, dev->func, 0x14, 0xFFFFFFFFu);

    uint32_t mask0 = pci_config_read32(dev->bus, dev->device, dev->func, 0x10);
    uint32_t mask1 = is_64
        ? pci_config_read32(dev->bus, dev->device, dev->func, 0x14)
        : 0;

    pci_config_write32(dev->bus, dev->device, dev->func, 0x10, orig0);
    if (is_64)
        pci_config_write32(dev->bus, dev->device, dev->func, 0x14, orig1);

    mask0 &= 0xFFFFFFF0u;
    if (is_64) {
        uint64_t mask = ((uint64_t)mask1 << 32) | mask0;
        if (!mask) return 0;
        return (~mask) + 1;
    }

    if (!mask0) return 0;
    return (uint64_t)(~mask0 + 1u);
}

void xhci_init(void) {
    char buf[32];

    serial_puts("\n[*] Initializing xHCI USB host controller...\n");

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

    uint32_t bar0_raw = xhci->bars[0];
    if (!bar0_raw) {
        serial_puts("  [!] BAR0 is zero — device not configured by firmware\n");
        return;
    }

    int is_io = (bar0_raw & 0x1) != 0;
    int bar_type = (bar0_raw >> 1) & 0x3;
    int is_64bit = (!is_io && bar_type == 2);
    int is_prefetch = (!is_io && ((bar0_raw >> 3) & 1));

    if (is_io) {
        serial_puts("  [!] xHCI BAR0 is an I/O BAR; expected MMIO\n");
        return;
    }

    uint64_t mmio_base = (uint64_t)(bar0_raw & 0xFFFFFFF0u);
    if (is_64bit)
        mmio_base |= (uint64_t)xhci->bars[1] << 32;

    serial_puts("  BAR0: Memory");
    serial_puts(is_64bit ? " 64-bit" : " 32-bit");
    serial_puts(is_prefetch ? " prefetchable\n" : " non-prefetchable\n");
    serial_puts("    base=0x");
    serial_puts(uxtoa_pad(mmio_base, buf, 16));
    serial_puts("\n");

    uint64_t bar_size = xhci_bar_size(xhci);
    if (!bar_size) {
        serial_puts("  [!] BAR0 size probe returned zero\n");
        return;
    }

    uint64_t num_pages = (bar_size + PAGE_SIZE - 1) / PAGE_SIZE;
    serial_puts("    size=0x");
    serial_puts(uxtoa_pad(bar_size, buf, 16));
    serial_puts(" (");
    serial_puts(uitoa_local(bar_size, buf));
    serial_puts(" bytes, ");
    serial_puts(uitoa_local(num_pages, buf));
    serial_puts(" page");
    serial_puts(num_pages == 1 ? "" : "s");
    serial_puts(")\n");

    pci_enable_device(xhci);
    serial_puts("  [+] PCI device enabled (I/O + Memory + Bus Master)\n");

    /*
     * The first 4 GiB are already identity-mapped by 2 MiB pages.  Do not
     * try to replace those large mappings with 4 KiB mappings.  For BARs
     * above 4 GiB, extend the existing page-table hierarchy with 4 KiB
     * identity mappings.  This also handles a BAR that straddles 4 GiB.
     */
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    cr3 &= ~0xFFFULL;

    const uint64_t low_limit = 0x100000000ULL;
    int mapped = 0;
    int map_failed = 0;

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t pa = mmio_base + i * PAGE_SIZE;
        uint64_t end = pa + PAGE_SIZE;
        if (end <= low_limit)
            continue; /* covered by the existing 2 MiB identity map */

        uint64_t map_pa = pa < low_limit ? low_limit : pa;
        if (map_pa != pa) {
            /* A page cannot be partially remapped; the crossing page is
             * expected to be page-aligned only when BAR is page-aligned. */
            serial_puts("  [!] BAR crosses 4 GiB on a non-page boundary\n");
            map_failed = 1;
            break;
        }

        int ret = map_page(cr3, pa, pa, PTE_PRESENT | PTE_WRITABLE);
        if (ret != 0) {
            serial_puts("  [!] map_page() failed at 0x");
            serial_puts(uxtoa_pad(pa, buf, 16));
            serial_puts(" ret=");
            serial_puts(uitoa_local((uint64_t)(ret < 0 ? -ret : ret), buf));
            serial_puts("\n");
            map_failed = 1;
            break;
        }
        mapped++;
    }

    if (map_failed) return;

    serial_puts("  [+] MMIO mapping ready: ");
    serial_puts(uitoa_local(mapped, buf));
    serial_puts(" new 4 KiB page");
    serial_puts(mapped == 1 ? "" : "s");
    serial_puts("\n");

    uint8_t cap_length = xhci_read8(mmio_base, XHCI_CAPLENGTH);
    uint16_t hci_version = xhci_read16(mmio_base, XHCI_HCIVERSION);
    uint32_t hcs_params1 = xhci_read32(mmio_base, XHCI_HCSPARAMS1);
    uint32_t hcs_params2 = xhci_read32(mmio_base, XHCI_HCSPARAMS2);
    uint32_t hcs_params3 = xhci_read32(mmio_base, XHCI_HCSPARAMS3);
    uint32_t hcc_params1 = xhci_read32(mmio_base, XHCI_HCCPARAMS1);
    uint32_t dboff = xhci_read32(mmio_base, XHCI_DBOFF);
    uint32_t rtsoff = xhci_read32(mmio_base, XHCI_RTSOFF);

    uint32_t max_slots = (hcs_params1 & XHCI_HCS1_MAX_SLOTS) >> 24;
    uint32_t max_intrs = (hcs_params1 & XHCI_HCS1_MAX_INTRS) >> 16;
    uint32_t max_ports = hcs_params1 & XHCI_HCS1_MAX_PORTS;
    int ac64 = (hcc_params1 & XHCI_HCC1_AC64) != 0;
    int csz = (hcc_params1 & XHCI_HCC1_CSZ) != 0;
    int ppc = (hcc_params1 & XHCI_HCC1_PPC) != 0;

    serial_puts("  Capability registers:\n");
    serial_puts("    CAPLENGTH="); serial_puts(uitoa_local(cap_length, buf));
    serial_puts(" HCIVERSION=0x");
    serial_puts(uxtoa_pad(hci_version >> 8, buf, 2)); serial_puts(".");
    serial_puts(uxtoa_pad(hci_version & 0xFF, buf, 2)); serial_puts("\n");

    serial_puts("    HCSPARAMS1: max_slots=");
    serial_puts(uitoa_local(max_slots, buf)); serial_puts(" max_intrs=");
    serial_puts(uitoa_local(max_intrs, buf)); serial_puts(" max_ports=");
    serial_puts(uitoa_local(max_ports, buf)); serial_puts("\n");

    serial_puts("    HCSPARAMS2=0x");
    serial_puts(uxtoa_pad(hcs_params2, buf, 8)); serial_puts(" HCSPARAMS3=0x");
    serial_puts(uxtoa_pad(hcs_params3, buf, 8)); serial_puts("\n");

    serial_puts("    HCCPARAMS1: 64-bit=");
    serial_puts(uitoa_local(ac64, buf)); serial_puts(" ctx_size=");
    serial_puts(csz ? "64" : "32"); serial_puts(" port_power=");
    serial_puts(uitoa_local(ppc, buf)); serial_puts("\n");

    serial_puts("    DBOFF=0x"); serial_puts(uxtoa_pad(dboff, buf, 8));
    serial_puts(" RTSOFF=0x"); serial_puts(uxtoa_pad(rtsoff, buf, 8));
    serial_puts("\n");

    uint64_t op_base = mmio_base + cap_length;
    uint32_t usbcmd = xhci_read32(op_base, XHCI_USBCMD);
    uint32_t usbsts = xhci_read32(op_base, XHCI_USBSTS);
    uint32_t pagesize = xhci_read32(op_base, XHCI_PAGESIZE);

    serial_puts("  Operational registers:\n");
    serial_puts("    USBCMD=0x"); serial_puts(uxtoa_pad(usbcmd, buf, 8));
    serial_puts(" USBSTS=0x"); serial_puts(uxtoa_pad(usbsts, buf, 8));
    serial_puts(usbsts & XHCI_USBSTS_HCH ? " (Halted)\n" : " (Running)\n");
    serial_puts("    PAGESIZE=0x"); serial_puts(uxtoa_pad(pagesize, buf, 8));
    if (pagesize) {
        serial_puts(" (");
        serial_puts(uitoa_local(1u << (xhci_ffs(pagesize) - 1), buf));
        serial_puts(" bytes)");
    }
    serial_puts("\n");

    serial_puts("[XHCI7b1] controller discovered\n");
}
