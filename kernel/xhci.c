/* xhci.c — xHCI USB Host Controller Driver
 *
 * Phase 7b.1: controller discovery, BAR analysis and capability reading.
 * Phase 7b.2: controller reset and command/event ring initialization.
 */
#include "xhci.h"
#include "cpu.h"
#include "mem.h"

static uint64_t g_xhci_mmio;
static uint64_t g_xhci_dcbaa;
static uint64_t g_xhci_cmd_ring;
static uint64_t g_xhci_event_ring;
static uint64_t g_xhci_erst;
static uint32_t g_xhci_cmd_cycle = 1;
static uint32_t g_xhci_event_cycle = 1;
static uint32_t g_xhci_max_slots;
static uint32_t g_xhci_max_ports;
static uint32_t g_xhci_ctx_size;

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

static void xhci_write32(uint64_t base, uint32_t off, uint32_t value) {
    *(volatile uint32_t *)(unsigned long)(base + off) = value;
}

static void xhci_write64(uint64_t base, uint32_t off, uint64_t value) {
    xhci_write32(base, off, (uint32_t)value);
    xhci_write32(base, off + 4, (uint32_t)(value >> 32));
}

static int xhci_ffs(uint32_t x) {
    if (!x) return 0;
    int n = 1;
    while (!(x & 1)) { x >>= 1; n++; }
    return n;
}

static void xhci_zero_page(uint64_t phys) {
    volatile uint64_t *p = (volatile uint64_t *)(unsigned long)phys;
    for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++)
        p[i] = 0;
}

static uint64_t xhci_alloc_dma_page(int ac64) {
    uint64_t phys = alloc_page();
    if (!phys) return 0;
    if (!ac64 && phys >= 0x100000000ULL) {
        free_page(phys);
        return 0;
    }
    xhci_zero_page(phys);
    return phys;
}

static void xhci_put_u64(struct xhci_trb *trb, uint64_t value) {
    trb->parameter_lo = (uint32_t)value;
    trb->parameter_hi = (uint32_t)(value >> 32);
}

static void xhci_make_link_trb(uint64_t ring_phys, uint32_t cycle) {
    struct xhci_trb *ring = (struct xhci_trb *)(unsigned long)ring_phys;
    struct xhci_trb *link = &ring[255];
    xhci_put_u64(link, ring_phys);
    link->status = 0;
    link->control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) |
                    XHCI_TRB_TC | cycle;
}

/*
 * Probe BAR0 without touching BAR1 unless BAR0 explicitly advertises a
 * 64-bit memory BAR. BAR1 is otherwise a separate PCI BAR.
 */
static uint64_t xhci_bar_size(struct pci_device *dev) {
    uint32_t orig0 = pci_config_read32(dev->bus, dev->device, dev->func, 0x10);
    if (!orig0) return 0;
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

static int xhci_wait_status(uint64_t op_base, uint32_t mask, uint32_t value) {
    for (uint32_t i = 0; i < 10000000; i++) {
        if ((xhci_read32(op_base, XHCI_USBSTS) & mask) == value)
            return 0;
    }
    return -1;
}

static int xhci_reset(uint64_t op_base) {
    uint32_t cmd = xhci_read32(op_base, XHCI_USBCMD);
    cmd &= ~XHCI_USBCMD_RUN;
    xhci_write32(op_base, XHCI_USBCMD, cmd);

    /* The controller must be halted before HCRST is asserted. */
    if (xhci_wait_status(op_base, XHCI_USBSTS_HCH, XHCI_USBSTS_HCH) != 0)
        return -1;

    xhci_write32(op_base, XHCI_USBCMD, XHCI_USBCMD_RESET);

    /* HCRST self-clears when reset completes. */
    for (uint32_t i = 0; i < 10000000; i++) {
        if (!(xhci_read32(op_base, XHCI_USBCMD) & XHCI_USBCMD_RESET))
            break;
        if (i == 9999999) return -2;
    }

    /* CNR must clear before operational registers are programmed. */
    if (xhci_wait_status(op_base, XHCI_USBSTS_CNR, 0) != 0)
        return -3;

    return 0;
}

static int xhci_setup_rings(uint64_t mmio_base, uint64_t op_base,
                            uint64_t rt_base, int ac64) {
    char buf[32];

    uint32_t pagesize = xhci_read32(op_base, XHCI_PAGESIZE);
    if (!(pagesize & 1)) {
        serial_puts("  [!] xHCI does not advertise 4 KiB page size\n");
        return -1;
    }

    g_xhci_dcbaa = xhci_alloc_dma_page(ac64);
    g_xhci_cmd_ring = xhci_alloc_dma_page(ac64);
    g_xhci_event_ring = xhci_alloc_dma_page(ac64);
    g_xhci_erst = xhci_alloc_dma_page(ac64);

    if (!g_xhci_dcbaa || !g_xhci_cmd_ring || !g_xhci_event_ring || !g_xhci_erst) {
        serial_puts("  [!] Failed to allocate xHCI DMA ring pages\n");
        return -2;
    }

    /* DCBAA slot 0 is reserved/null until device enumeration allocates slots. */
    xhci_zero_page(g_xhci_dcbaa);

    /* One 256-TRB command ring segment, terminated by a Link TRB. */
    xhci_zero_page(g_xhci_cmd_ring);
    xhci_make_link_trb(g_xhci_cmd_ring, 1);
    g_xhci_cmd_cycle = 1;

    /* One 256-TRB event ring segment. Producer cycle starts at 1. */
    xhci_zero_page(g_xhci_event_ring);
    g_xhci_event_cycle = 1;

    struct xhci_erst_entry *erst =
        (struct xhci_erst_entry *)(unsigned long)g_xhci_erst;
    erst[0].ring_seg_base_lo = (uint32_t)g_xhci_event_ring;
    erst[0].ring_seg_base_hi = (uint32_t)(g_xhci_event_ring >> 32);
    erst[0].ring_seg_size = 256;
    erst[0].reserved = 0;

    /* Program Device Context Base Address Array and command ring. */
    xhci_write64(op_base, XHCI_DCBAAP_LOW, g_xhci_dcbaa);
    xhci_write64(op_base, XHCI_CRCR_LOW, g_xhci_cmd_ring | g_xhci_cmd_cycle);

    /* Interrupt 0 event ring. We deliberately leave IE clear: Phase 7b.2
     * establishes the event ring for polling; IRQ handling comes later. */
    uint64_t intr0 = rt_base + XHCI_RT_INTR_BASE;
    xhci_write32(intr0, XHCI_IMAN, XHCI_IMAN_IP);
    xhci_write32(intr0, XHCI_IMOD, 0);
    xhci_write32(intr0, XHCI_ERSTSZ, 1);
    xhci_write64(intr0, XHCI_ERSTBA_LOW, g_xhci_erst);
    xhci_write64(intr0, XHCI_ERDP_LOW, g_xhci_event_ring);

    /* Accept up to the controller's advertised slot count. */
    xhci_write32(op_base, XHCI_CONFIG, g_xhci_max_slots);

    serial_puts("  [+] DCBAA=0x");
    serial_puts(uxtoa_pad(g_xhci_dcbaa, buf, 16)); serial_puts("\n");
    serial_puts("  [+] Command ring=0x");
    serial_puts(uxtoa_pad(g_xhci_cmd_ring, buf, 16)); serial_puts(" (256 TRBs)\n");
    serial_puts("  [+] Event ring=0x");
    serial_puts(uxtoa_pad(g_xhci_event_ring, buf, 16)); serial_puts(" (256 TRBs)\n");
    serial_puts("  [+] ERST=0x");
    serial_puts(uxtoa_pad(g_xhci_erst, buf, 16)); serial_puts(" (1 segment)\n");

    /* Start the controller. */
    uint32_t cmd = xhci_read32(op_base, XHCI_USBCMD);
    cmd |= XHCI_USBCMD_RUN;
    xhci_write32(op_base, XHCI_USBCMD, cmd);

    if (xhci_wait_status(op_base, XHCI_USBSTS_HCH, 0) != 0) {
        serial_puts("  [!] xHCI did not leave Halted state\n");
        return -3;
    }

    uint32_t sts = xhci_read32(op_base, XHCI_USBSTS);
    if (sts & XHCI_USBSTS_HCE) {
        serial_puts("  [!] xHCI Host Controller Error after start\n");
        return -4;
    }

    serial_puts("  [+] xHCI controller running, command/event rings ready\n");
    return 0;
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
            continue;

        if (pa < low_limit) {
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

    g_xhci_max_slots = (hcs_params1 & XHCI_HCS1_MAX_SLOTS) >> 24;
    g_xhci_max_ports = hcs_params1 & XHCI_HCS1_MAX_PORTS;
    int ac64 = (hcc_params1 & XHCI_HCC1_AC64) != 0;
    int csz = (hcc_params1 & XHCI_HCC1_CSZ) != 0;
    int ppc = (hcc_params1 & XHCI_HCC1_PPC) != 0;
    g_xhci_ctx_size = csz ? 64 : 32;

    serial_puts("  Capability registers:\n");
    serial_puts("    CAPLENGTH="); serial_puts(uitoa_local(cap_length, buf));
    serial_puts(" HCIVERSION=0x");
    serial_puts(uxtoa_pad(hci_version >> 8, buf, 2)); serial_puts(".");
    serial_puts(uxtoa_pad(hci_version & 0xFF, buf, 2)); serial_puts("\n");

    serial_puts("    HCSPARAMS1: max_slots=");
    serial_puts(uitoa_local(g_xhci_max_slots, buf)); serial_puts(" max_intrs=");
    serial_puts(uitoa_local((hcs_params1 >> 16) & 0xFF, buf)); serial_puts(" max_ports=");
    serial_puts(uitoa_local(g_xhci_max_ports, buf)); serial_puts("\n");

    serial_puts("    HCSPARAMS2=0x");
    serial_puts(uxtoa_pad(hcs_params2, buf, 8)); serial_puts(" HCSPARAMS3=0x");
    serial_puts(uxtoa_pad(hcs_params3, buf, 8)); serial_puts("\n");

    serial_puts("    HCCPARAMS1: 64-bit=");
    serial_puts(uitoa_local(ac64, buf)); serial_puts(" ctx_size=");
    serial_puts(uitoa_local(g_xhci_ctx_size, buf)); serial_puts(" port_power=");
    serial_puts(uitoa_local(ppc, buf)); serial_puts("\n");

    serial_puts("    DBOFF=0x"); serial_puts(uxtoa_pad(dboff, buf, 8));
    serial_puts(" RTSOFF=0x"); serial_puts(uxtoa_pad(rtsoff, buf, 8));
    serial_puts("\n");

    uint64_t op_base = mmio_base + cap_length;
    uint64_t rt_base = mmio_base + rtsoff;
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

    if (xhci_reset(op_base) != 0) {
        serial_puts("  [!] xHCI controller reset failed\n");
        return;
    }
    serial_puts("  [+] xHCI controller reset complete\n");

    if (xhci_setup_rings(mmio_base, op_base, rt_base, ac64) != 0) {
        serial_puts("  [!] xHCI ring initialization failed\n");
        return;
    }

    serial_puts("  Ports available: ");
    serial_puts(uitoa_local(g_xhci_max_ports, buf));
    serial_puts("  Context size: ");
    serial_puts(uitoa_local(g_xhci_ctx_size, buf));
    serial_puts(" bytes\n");

    serial_puts("[XHCI7b2] reset + rings ready\n");
}
