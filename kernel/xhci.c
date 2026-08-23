/* xhci.c — xHCI USB Host Controller Driver (Phase 7b.1)
 *
 * Phase 7b.1: Controller discovery + capability register reading.
 *
 * The xHCI controller is discovered via PCI class 0C/03/30.
 * On QEMU, adding `-device qemu-xhci` creates an xHCI controller
 * (typically Red Hat vendor 0x1B36, device 0x000C).
 *
 * The controller's registers are MMIO-mapped at BAR0.
 * Since LumaOS uses identity mapping (physical = virtual) for the
 * first 4 GB with 2 MB pages, BAR0 addresses are directly accessible.
 *
 * This phase reads the xHCI capability registers to verify the
 * controller is present and understand its configuration. It does
 * not reset or configure the controller yet.
 */
#include "xhci.h"
#include "cpu.h"

/* ===== Helpers ===== */

static char *uitoa_local(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0;
    while (n) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

static char *uxtoa_local(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0;
    const char *h="0123456789ABCDEF";
    while (n) { tmp[i++]=h[n&0xF]; n>>=4; }
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
 * The kernel identity-maps the first 4 GB with 2 MB pages,
 * so PCI MMIO BARs (typically 0xFExxxxxx) are directly accessible. */
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

/* ===== Init ===== */

void xhci_init(void) {
    char buf[16];

    serial_puts("\n[*] Initializing xHCI USB host controller...\n");

    /* --- 1. Find the xHCI controller via PCI class --- */
    /* Class 0x0C (Serial bus), Subclass 0x03 (USB), ProgIF 0x30 (xHCI) */
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

    /* --- 2. Read BAR0 (MMIO base address) --- */
    uint32_t bar0_raw = xhci->bars[0];
    uint64_t mmio_base = bar0_raw & 0xFFFFFFF0u;  /* mask type + enable bits */

    /* Check if it's a 64-bit BAR (BAR0 type = 0b10 in low 2 bits) */
    if ((bar0_raw & 0x06) == 0x04) {
        /* 64-bit BAR: upper 32 bits in BAR1 */
        mmio_base |= ((uint64_t)xhci->bars[1] << 32);
    }

    serial_puts("  BAR0 raw=0x");
    serial_puts(uxtoa_pad(bar0_raw, buf, 8));
    serial_puts(" -> MMIO base=0x");
    serial_puts(uxtoa_pad(mmio_base, buf, 8));
    serial_puts("\n");

    if (!mmio_base) {
        serial_puts("  [!] BAR0 is zero — device not configured by BIOS\n");
        return;
    }

    /* --- 3. Enable the PCI device (Memory Space + Bus Master) --- */
    pci_enable_device(xhci);
    serial_puts("  [+] PCI device enabled (Memory Space + Bus Master)\n");

    /* --- 4. Read xHCI capability registers --- */
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
    /* BCD version: e.g. 0x0100 = 1.00, 0x0110 = 1.10 */
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

    /* --- 5. Read operational registers (at MMIO base + CAPLENGTH) --- */
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