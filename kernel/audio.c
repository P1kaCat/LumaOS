/* audio.c — Intel High Definition Audio (HDA) Controller Driver
 *
 * Phase 7e: Intel HDA controller discovery, CORB/RIRB setup and codec discovery.
 */
#include "audio.h"
#include "cpu.h"
#include "mem.h"

static uint64_t g_hda_mmio;
static uint64_t g_hda_corb;
static uint64_t g_hda_rirb;

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

static uint32_t hda_read32(uint32_t reg) {
    return *(volatile uint32_t *)(unsigned long)(g_hda_mmio + reg);
}

static uint16_t hda_read16(uint32_t reg) {
    return *(volatile uint16_t *)(unsigned long)(g_hda_mmio + reg);
}

static uint8_t hda_read8(uint32_t reg) {
    return *(volatile uint8_t *)(unsigned long)(g_hda_mmio + reg);
}

static void hda_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(unsigned long)(g_hda_mmio + reg) = val;
}

static void hda_write16(uint32_t reg, uint16_t val) {
    *(volatile uint16_t *)(unsigned long)(g_hda_mmio + reg) = val;
}

static void hda_write8(uint32_t reg, uint8_t val) {
    *(volatile uint8_t *)(unsigned long)(g_hda_mmio + reg) = val;
}

void audio_init(void) {
    char buf[32];
    serial_puts("\n[*] Initializing Intel High Definition Audio (HDA) controller...\n");

    struct pci_device *hda_dev = pci_find_class(0x04, 0x03, 0x00);
    if (!hda_dev) {
        serial_puts("  [!] No Intel HDA audio controller found (PCI 04/03/00)\n");
        return;
    }

    serial_puts("  [+] HDA controller found at ");
    serial_puts(uxtoa_pad(hda_dev->bus, buf, 2)); serial_puts(":");
    serial_puts(uxtoa_pad(hda_dev->device, buf, 2)); serial_puts(".");
    serial_puts(uitoa_local(hda_dev->func, buf));
    serial_puts(" vendor=");
    serial_puts(uxtoa_pad(hda_dev->vendor, buf, 4));
    serial_puts(" device=");
    serial_puts(uxtoa_pad(hda_dev->device_id, buf, 4));
    serial_puts("\n");

    uint32_t bar0_raw = hda_dev->bars[0];
    if (!bar0_raw) {
        serial_puts("  [!] HDA BAR0 is zero\n");
        return;
    }

    int is_64 = ((bar0_raw >> 1) & 3) == 2;
    uint64_t mmio = (uint64_t)(bar0_raw & 0xFFFFFFF0u);
    if (is_64) mmio |= (uint64_t)hda_dev->bars[1] << 32;

    g_hda_mmio = mmio;
    pci_enable_device(hda_dev);

    /* Map MMIO pages if above 4GB */
    if (mmio >= 0x100000000ULL) {
        uint64_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        cr3 &= ~0xFFFULL;
        for (uint64_t i = 0; i < 4; i++) {
            map_page(cr3, mmio + i * PAGE_SIZE, mmio + i * PAGE_SIZE, PTE_PRESENT | PTE_WRITABLE);
        }
    }

    serial_puts("  BAR0=0x");
    serial_puts(uxtoa_pad(mmio, buf, 16));
    serial_puts("\n");

    /* Reset Controller: clear GCTL.CRST and wait for 0 */
    uint32_t gctl = hda_read32(HDA_REG_GCTL);
    hda_write32(HDA_REG_GCTL, gctl & ~HDA_GCTL_CRST);
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(hda_read32(HDA_REG_GCTL) & HDA_GCTL_CRST))
            break;
    }

    /* Bring out of reset: set GCTL.CRST and wait for 1 */
    hda_write32(HDA_REG_GCTL, HDA_GCTL_CRST);
    int rst_ok = 0;
    for (uint32_t i = 0; i < 100000; i++) {
        if (hda_read32(HDA_REG_GCTL) & HDA_GCTL_CRST) {
            rst_ok = 1;
            break;
        }
    }

    if (!rst_ok) {
        serial_puts("  [!] HDA controller reset failed\n");
        return;
    }

    /* Wait for codecs to enumerate on the link */
    for (volatile int d = 0; d < 500000; d++);

    uint16_t vmaj = hda_read8(HDA_REG_VMAJ);
    uint16_t vmin = hda_read8(HDA_REG_VMIN);
    uint16_t gcap = hda_read16(HDA_REG_GCAP);
    uint16_t codecs = hda_read16(HDA_REG_STATESTS);

    serial_puts("  HDA Spec Version: ");
    serial_puts(uitoa_local(vmaj, buf)); serial_puts(".");
    serial_puts(uitoa_local(vmin, buf));
    serial_puts(" (Out streams: ");
    serial_puts(uitoa_local((gcap >> 12) & 0xF, buf));
    serial_puts(", In streams: ");
    serial_puts(uitoa_local((gcap >> 8) & 0xF, buf));
    serial_puts(")\n");

    serial_puts("  [+] Codecs detected mask: 0x");
    serial_puts(uxtoa_pad(codecs, buf, 4));
    serial_puts("\n");

    /* Allocate CORB and RIRB */
    g_hda_corb = alloc_page();
    g_hda_rirb = alloc_page();

    if (g_hda_corb && g_hda_rirb) {
        volatile uint8_t *c_p = (volatile uint8_t *)(unsigned long)g_hda_corb;
        volatile uint8_t *r_p = (volatile uint8_t *)(unsigned long)g_hda_rirb;
        for (int i = 0; i < 4096; i++) { c_p[i] = 0; r_p[i] = 0; }

        /* Setup CORB (256 entries * 4 bytes) */
        hda_write8(HDA_REG_CORBCTL, 0); /* Stop CORB */
        hda_write32(HDA_REG_CORBLBASE, (uint32_t)g_hda_corb);
        hda_write32(HDA_REG_CORBUBASE, (uint32_t)(g_hda_corb >> 32));
        hda_write8(HDA_REG_CORBSIZE, 0x02); /* 256 entries */
        hda_write16(HDA_REG_CORBWP, 0);
        hda_write16(HDA_REG_CORBRP, 0x8000); /* Reset read pointer */
        for (uint32_t i = 0; i < 10000; i++) {
            if (hda_read16(HDA_REG_CORBRP) & 0x8000) break;
        }
        hda_write16(HDA_REG_CORBRP, 0); /* Clear reset bit */
        hda_write8(HDA_REG_CORBCTL, HDA_CORBCTL_RUN);

        /* Setup RIRB (256 entries * 8 bytes) */
        hda_write8(HDA_REG_RIRBCTL, 0); /* Stop RIRB */
        hda_write32(HDA_REG_RIRBLBASE, (uint32_t)g_hda_rirb);
        hda_write32(HDA_REG_RIRBUBASE, (uint32_t)(g_hda_rirb >> 32));
        hda_write8(HDA_REG_RIRBSIZE, 0x02); /* 256 entries */
        hda_write16(HDA_REG_RIRBWP, 0x8000); /* Reset write pointer */
        hda_write16(HDA_REG_RINTCNT, 1);
        hda_write8(HDA_REG_RIRBCTL, HDA_RIRBCTL_RUN);

        serial_puts("  [+] CORB/RIRB Ring Buffers active\n");
    }

    serial_puts("[HDA7e] audio controller initialized + CORB/RIRB ready\n");
}
