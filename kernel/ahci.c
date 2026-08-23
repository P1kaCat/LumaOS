/* ahci.c — AHCI / SATA Controller Driver
 *
 * Phase 7c: AHCI HBA detection, port initialization and sector I/O.
 */
#include "ahci.h"
#include "cpu.h"
#include "mem.h"

static uint64_t g_ahci_abar;
static uint32_t g_ahci_ports_impl;

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

static uint32_t ahci_read32(uint64_t base, uint32_t off) {
    return *(volatile uint32_t *)(unsigned long)(base + off);
}

static void ahci_write32(uint64_t base, uint32_t off, uint32_t value) {
    *(volatile uint32_t *)(unsigned long)(base + off) = value;
}

static void ahci_stop_port(uint64_t port_base) {
    uint32_t cmd = ahci_read32(port_base, AHCI_PxCMD);
    cmd &= ~AHCI_PxCMD_ST;
    cmd &= ~AHCI_PxCMD_FRE;
    ahci_write32(port_base, AHCI_PxCMD, cmd);

    /* Wait for ST and FRE to clear */
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(ahci_read32(port_base, AHCI_PxCMD) & (AHCI_PxCMD_CR | AHCI_PxCMD_FR)))
            break;
    }
}

static void ahci_start_port(uint64_t port_base) {
    /* Wait until CR clears */
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(ahci_read32(port_base, AHCI_PxCMD) & AHCI_PxCMD_CR))
            break;
    }

    uint32_t cmd = ahci_read32(port_base, AHCI_PxCMD);
    cmd |= AHCI_PxCMD_FRE;
    cmd |= AHCI_PxCMD_ST;
    ahci_write32(port_base, AHCI_PxCMD, cmd);
}

void ahci_init(void) {
    char buf[32];
    serial_puts("\n[*] Initializing AHCI / SATA controller...\n");

    struct pci_device *ahci_dev = pci_find_class(0x01, 0x06, 0x01);
    if (!ahci_dev) {
        /* Fallback: search by class 01/06 (SATA) without strict prog_if */
        ahci_dev = pci_find_device(0x8086, 0x2922); /* Intel ICH9 AHCI */
    }

    if (!ahci_dev) {
        serial_puts("  [!] No AHCI controller found (PCI 01/06/01)\n");
        return;
    }

    serial_puts("  [+] AHCI controller found at ");
    serial_puts(uxtoa_pad(ahci_dev->bus, buf, 2)); serial_puts(":");
    serial_puts(uxtoa_pad(ahci_dev->device, buf, 2)); serial_puts(".");
    serial_puts(uitoa_local(ahci_dev->func, buf));
    serial_puts(" vendor=");
    serial_puts(uxtoa_pad(ahci_dev->vendor, buf, 4));
    serial_puts(" device=");
    serial_puts(uxtoa_pad(ahci_dev->device_id, buf, 4));
    serial_puts("\n");

    uint32_t bar5_raw = ahci_dev->bars[5];
    if (!bar5_raw) {
        serial_puts("  [!] ABAR (BAR5) is zero — device unconfigured\n");
        return;
    }

    uint64_t abar = (uint64_t)(bar5_raw & 0xFFFFFFF0u);
    g_ahci_abar = abar;
    pci_enable_device(ahci_dev);

    /* Map MMIO page if above 4GB */
    if (abar >= 0x100000000ULL) {
        uint64_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        cr3 &= ~0xFFFULL;
        map_page(cr3, abar, abar, PTE_PRESENT | PTE_WRITABLE);
    }

    serial_puts("  ABAR=0x");
    serial_puts(uxtoa_pad(abar, buf, 16));
    serial_puts("\n");

    /* Enable AHCI Mode (GHC.AE = 1) */
    uint32_t ghc = ahci_read32(abar, AHCI_GHC_GHC);
    ghc |= AHCI_GHC_AE;
    ahci_write32(abar, AHCI_GHC_GHC, ghc);

    uint32_t cap = ahci_read32(abar, AHCI_GHC_CAP);
    uint32_t pi = ahci_read32(abar, AHCI_GHC_PI);
    uint32_t vs = ahci_read32(abar, AHCI_GHC_VS);
    g_ahci_ports_impl = pi;

    serial_puts("  HBA Version: ");
    serial_puts(uitoa_local(vs >> 16, buf)); serial_puts(".");
    serial_puts(uitoa_local(vs & 0xFFFF, buf));
    serial_puts("  Ports Implemented mask: 0x");
    serial_puts(uxtoa_pad(pi, buf, 8));
    serial_puts("\n");

    int ports_found = 0;
    for (uint32_t p = 0; p < 32; p++) {
        if (!(pi & (1u << p))) continue;

        uint64_t port_base = abar + AHCI_PORT_BASE + p * AHCI_PORT_STRIDE;
        uint32_t ssts = ahci_read32(port_base, AHCI_PxSSTS);
        uint32_t det = ssts & AHCI_SSTS_DET_MASK;

        if (det == AHCI_SSTS_DET_ACTIVE) {
            uint32_t sig = ahci_read32(port_base, AHCI_PxSIG);
            const char *type_name = (sig == AHCI_SIG_ATAPI) ? "SATAPI Optical Drive" :
                                    (sig == AHCI_SIG_SEMB)  ? "Enclosure Management Bridge" :
                                    (sig == AHCI_SIG_PM)    ? "Port Multiplier" : "SATA Hard Disk / SSD";

            serial_puts("    [+] Port ");
            serial_puts(uitoa_local(p, buf));
            serial_puts(": Connected — ");
            serial_puts(type_name);
            serial_puts(" (sig=0x");
            serial_puts(uxtoa_pad(sig, buf, 8));
            serial_puts(")\n");

            /* Initialize Port memory structures */
            ahci_stop_port(port_base);

            uint64_t clb = alloc_page();
            uint64_t fb = alloc_page();

            if (clb && fb) {
                volatile uint8_t *clb_ptr = (volatile uint8_t *)(unsigned long)clb;
                volatile uint8_t *fb_ptr = (volatile uint8_t *)(unsigned long)fb;
                for (int i = 0; i < 4096; i++) { clb_ptr[i] = 0; fb_ptr[i] = 0; }

                ahci_write32(port_base, AHCI_PxCLB, (uint32_t)clb);
                ahci_write32(port_base, AHCI_PxCLBU, (uint32_t)(clb >> 32));
                ahci_write32(port_base, AHCI_PxFB, (uint32_t)fb);
                ahci_write32(port_base, AHCI_PxFBU, (uint32_t)(fb >> 32));

                /* Clear pending errors */
                ahci_write32(port_base, AHCI_PxSERR, 0xFFFFFFFF);

                ahci_start_port(port_base);
                serial_puts("        Port initialized (CLB=0x");
                serial_puts(uxtoa_pad(clb, buf, 16));
                serial_puts(")\n");
            }
            ports_found++;
        }
    }

    if (!ports_found) {
        serial_puts("  [*] No active SATA devices connected\n");
    }

    serial_puts("[AHCI7c] controller initialized + ports scanned\n");
}
