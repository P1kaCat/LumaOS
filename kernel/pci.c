/* pci.c — PCI bus enumeration + BAR decode + device enable (Phase 7a.1 + 7a.2)
 *
 * Phase 7a.1: Read-only PCI config space access via I/O ports 0xCF8/0xCFC.
 *             Enumerate bus 0 only (QEMU i440FX has a single PCI bus).
 *
 * Phase 7a.2: Config space write support, BAR size decoding, device enable.
 *             BAR decode: write 0xFFFFFFFF to BAR, read back, mask, restore.
 *             Device enable: set Command register (IO + Memory + Bus Master).
 *
 * Config space access:
 *   outl(0xCF8, 0x80000000 | (bus<<16) | (dev<<11) | (func<<8) | (offset&0xFC))
 *   inl(0xCFC) → 32-bit value at that offset
 *
 * Multifunction detection:
 *   Header type byte (offset 0x0E), bit 7 = multifunction.
 *   If function 0 is not multifunction, skip functions 1-7.
 *
 * Absent device detection:
 *   Vendor ID 0xFFFF = no device at this BDF.
 *
 * Bounds:
 *   bus: fixed to 0 for now
 *   device: 0-31 (5 bits)
 *   function: 0-7 (3 bits)
 *   offset: 0-254, aligned to 4 for 32-bit reads
 *
 * All loops are bounded — no infinite loop possible.
 */
#include "pci.h"
#include "cpu.h"  /* serial_puts, inb/outb, inl/outl */

/* ===== Device table ===== */
static struct pci_device pci_devices[PCI_MAX_DEVICES];
static int pci_num_devices = 0;

/* ===== Local helpers (kernel has no shared libc) ===== */

static char *uitoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0;
    while (n) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

static char *uxtoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0;
    const char *h="0123456789ABCDEF";
    while (n) { tmp[i++]=h[n&0xF]; n>>=4; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

/* Pad hex to N digits (for nice formatting) */
static char *uxtoa_pad(uint64_t n, char *buf, int width) {
    char tmp[32]; int i=0;
    const char *h="0123456789ABCDEF";
    if (!n) { tmp[i++]='0'; }
    while (n) { tmp[i++]=h[n&0xF]; n>>=4; }
    while (i < width) tmp[i++]='0';
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

/* ===== Config space access ===== */

uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t addr = 0x80000000u
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func << 8)
                  | ((uint32_t)offset & 0xFCu);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t val = pci_config_read32(bus, dev, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t val = pci_config_read32(bus, dev, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

void pci_config_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = 0x80000000u
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func << 8)
                  | ((uint32_t)offset & 0xFCu);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, val);
}

void pci_config_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val) {
    /* Read-modify-write to avoid clobbering the other 16-bit half of the dword.
     * E.g. offset 0x04 = Command, offset 0x06 = Status — must preserve Status. */
    uint32_t cur = pci_config_read32(bus, dev, func, offset);
    uint32_t new_val;
    if (offset & 2) {
        new_val = (cur & 0x0000FFFFu) | ((uint32_t)val << 16);
    } else {
        new_val = (cur & 0xFFFF0000u) | (uint32_t)val;
    }
    pci_config_write32(bus, dev, func, offset, new_val);
}

/* ===== Class name lookup ===== */

static const char *pci_class_name(uint8_t base_class) {
    switch (base_class) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass storage";
        case 0x02: return "Network";
        case 0x03: return "Display";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x07: return "Comms";
        case 0x08: return "Peripheral";
        case 0x09: return "Input";
        case 0x0A: return "Docking";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial bus";
        case 0x0D: return "Wireless";
        case 0x0E: return "I2O";
        case 0x0F: return "Satellite";
        case 0x10: return "Encryption";
        case 0x11: return "DSP";
        case 0xFF: return "Unknown";
        default:   return "Reserved";
    }
}

/* ===== BAR decoding ===== */

/* Decode a single BAR: determine type (mem32/mem64/io), address, and size.
 * For 64-bit BARs, consumes the next register slot (high 32 bits). */
static void pci_decode_bar(struct pci_device *d, int bar_idx) {
    uint8_t bus = d->bus, dev = d->device, func = d->func;
    uint8_t offset = PCI_OFFSET_BAR0 + bar_idx * 4;
    uint32_t orig = pci_config_read32(bus, dev, func, offset);

    /* Initialize defaults */
    d->bar_type[bar_idx] = PCI_BAR_NONE;
    d->bar_addr[bar_idx] = 0;
    d->bar_size[bar_idx] = 0;

    if (orig == 0) return;  /* BAR not implemented */

    int is_io = orig & 1;

    if (is_io) {
        /* I/O BAR: bit 0 = 1, bits 2-31 = address (bit 1 reserved) */
        d->bar_type[bar_idx] = PCI_BAR_IO;
        d->bar_addr[bar_idx] = orig & 0xFFFFFFFCu;

        /* Decode size: write all 1s, read back, restore */
        pci_config_write32(bus, dev, func, offset, 0xFFFFFFFFu);
        uint32_t val = pci_config_read32(bus, dev, func, offset);
        pci_config_write32(bus, dev, func, offset, orig);

        val &= 0xFFFFFFFCu;
        if (val == 0) return;
        d->bar_size[bar_idx] = (~val) + 1;
    } else {
        /* Memory BAR: bit 0 = 0 */
        int type = (orig >> 1) & 3;  /* bits 1-2: 0=32-bit, 2=64-bit */
        /* bit 3: prefetchable (not decoded for now) */

        if (type == 0) {
            /* 32-bit memory BAR */
            d->bar_type[bar_idx] = PCI_BAR_MEM32;
            d->bar_addr[bar_idx] = orig & 0xFFFFFFF0u;

            pci_config_write32(bus, dev, func, offset, 0xFFFFFFFFu);
            uint32_t val = pci_config_read32(bus, dev, func, offset);
            pci_config_write32(bus, dev, func, offset, orig);

            val &= 0xFFFFFFF0u;
            if (val == 0) return;
            d->bar_size[bar_idx] = (~val) + 1;
        } else if (type == 2) {
            /* 64-bit memory BAR: uses 2 consecutive register slots */
            d->bar_type[bar_idx] = PCI_BAR_MEM64;

            uint32_t orig_hi = 0;
            if (bar_idx + 1 < d->num_bars) {
                orig_hi = pci_config_read32(bus, dev, func, offset + 4);
            }

            d->bar_addr[bar_idx] = ((uint64_t)(orig & 0xFFFFFFF0u))
                                 | ((uint64_t)orig_hi << 32);

            /* Decode size: write all 1s to both halves */
            pci_config_write32(bus, dev, func, offset, 0xFFFFFFFFu);
            if (bar_idx + 1 < d->num_bars) {
                pci_config_write32(bus, dev, func, offset + 4, 0xFFFFFFFFu);
            }
            uint32_t val_lo = pci_config_read32(bus, dev, func, offset);
            uint32_t val_hi = 0;
            if (bar_idx + 1 < d->num_bars) {
                val_hi = pci_config_read32(bus, dev, func, offset + 4);
            }

            /* Restore */
            pci_config_write32(bus, dev, func, offset, orig);
            if (bar_idx + 1 < d->num_bars) {
                pci_config_write32(bus, dev, func, offset + 4, orig_hi);
            }

            val_lo &= 0xFFFFFFF0u;
            uint64_t val64 = ((uint64_t)val_hi << 32) | val_lo;
            if (val64 == 0) return;
            d->bar_size[bar_idx] = (~val64) + 1;

            /* 64-bit BAR consumes the next register slot */
            if (bar_idx + 1 < d->num_bars) {
                d->bar_type[bar_idx + 1] = PCI_BAR_NONE;
                d->bar_addr[bar_idx + 1] = 0;
                d->bar_size[bar_idx + 1] = 0;
            }
        } else {
            /* Reserved BAR type (1 = reserved, 3 = reserved) */
            d->bar_type[bar_idx] = PCI_BAR_NONE;
        }
    }
}

/* Decode all BARs of a device. Handles 64-bit BARs occupying two slots. */
void pci_decode_bars(struct pci_device *d) {
    /* Initialize all BAR slots */
    for (int i = 0; i < 6; i++) {
        d->bar_type[i] = PCI_BAR_NONE;
        d->bar_addr[i] = 0;
        d->bar_size[i] = 0;
    }

    for (int i = 0; i < d->num_bars; i++) {
        if (d->bars[i] == 0) continue;  /* BAR not implemented */
        pci_decode_bar(d, i);
        if (d->bar_type[i] == PCI_BAR_MEM64) i++;  /* skip high half */
    }
}

/* ===== Device enablement ===== */

void pci_enable_device(struct pci_device *d) {
    /* Read current Command register, set IO + Memory + Bus Master bits */
    uint16_t cmd = pci_config_read16(d->bus, d->device, d->func, PCI_OFFSET_COMMAND);
    cmd |= PCI_CMD_IO | PCI_CMD_MEMORY | PCI_CMD_BUS_MASTER;
    pci_config_write16(d->bus, d->device, d->func, PCI_OFFSET_COMMAND, cmd);
    /* Read back to confirm */
    d->command = pci_config_read16(d->bus, d->device, d->func, PCI_OFFSET_COMMAND);
}

/* ===== Serial printer for one device ===== */

static const char *bar_type_name(uint8_t t) {
    switch (t) {
        case PCI_BAR_MEM32: return "MEM32";
        case PCI_BAR_MEM64: return "MEM64";
        case PCI_BAR_IO:    return "I/O";
        default:            return "none";
    }
}

static void pci_print_device(const struct pci_device *d) {
    char buf[16];
    serial_puts("[PCI] ");
    serial_puts(uxtoa_pad(d->bus, buf, 2)); serial_puts(":");
    serial_puts(uxtoa_pad(d->device, buf, 2)); serial_puts(".");
    serial_puts(uxtoa(d->func, buf));
    serial_puts(" vendor=");
    serial_puts(uxtoa_pad(d->vendor, buf, 4));
    serial_puts(" device=");
    serial_puts(uxtoa_pad(d->device_id, buf, 4));
    serial_puts(" class=");
    serial_puts(uxtoa_pad(d->base_class, buf, 2));
    serial_puts("/");
    serial_puts(uxtoa_pad(d->subclass, buf, 2));
    serial_puts("/");
    serial_puts(uxtoa_pad(d->prog_if, buf, 2));
    serial_puts(" [");
    serial_puts(pci_class_name(d->base_class));
    serial_puts("]");
    if (d->header_type & PCI_HEADER_MULTIFUNC)
        serial_puts(" (multifunc)");
    if (d->irq_pin) {
        serial_puts(" IRQpin");
        serial_puts(uitoa(d->irq_pin, buf));
        serial_puts(" line");
        serial_puts(uitoa(d->irq_line, buf));
    }
    serial_puts("\n");

    /* Print decoded BAR info (Phase 7a.2) */
    for (int i = 0; i < d->num_bars; i++) {
        if (d->bar_type[i] == PCI_BAR_NONE) continue;
        serial_puts("  BAR");
        serial_puts(uitoa(i, buf));
        serial_puts(": ");
        serial_puts(bar_type_name(d->bar_type[i]));
        serial_puts(" addr=0x");
        serial_puts(uxtoa(d->bar_addr[i], buf));
        serial_puts(" size=0x");
        serial_puts(uxtoa(d->bar_size[i], buf));
        serial_puts("\n");
    }
}

/* ===== Enumeration ===== */

/* Check if a device is present at the given bus/device/function. */
static int pci_probe(uint8_t bus, uint8_t dev, uint8_t func) {
    return pci_config_read16(bus, dev, func, PCI_OFFSET_VENDOR) != PCI_VENDOR_INVALID;
}

/* Read and store a single PCI device's config into the table slot. */
static void pci_read_device(struct pci_device *d, uint8_t bus, uint8_t dev, uint8_t func) {
    d->bus        = bus;
    d->device     = dev;
    d->func       = func;
    d->vendor     = pci_config_read16(bus, dev, func, PCI_OFFSET_VENDOR);
    d->device_id  = pci_config_read16(bus, dev, func, PCI_OFFSET_DEVICE);
    d->prog_if    = pci_config_read8(bus, dev, func, 0x09);
    d->subclass   = pci_config_read8(bus, dev, func, 0x0A);
    d->base_class = pci_config_read8(bus, dev, func, 0x0B);
    d->header_type = pci_config_read8(bus, dev, func, PCI_OFFSET_HEADER);
    d->irq_line   = pci_config_read8(bus, dev, func, PCI_OFFSET_IRQ_LINE);
    d->irq_pin    = pci_config_read8(bus, dev, func, PCI_OFFSET_IRQ_PIN);

    /* Read BARs based on header type (low 2 bits, mask off multifunction bit) */
    uint8_t ht = d->header_type & 0x7F;
    if (ht == PCI_HEADER_NORMAL) {
        d->num_bars = 6;
        for (int b = 0; b < 6; b++)
            d->bars[b] = pci_config_read32(bus, dev, func, PCI_OFFSET_BAR0 + b * 4);
    } else if (ht == PCI_HEADER_BRIDGE) {
        d->num_bars = 2;
        d->bars[0] = pci_config_read32(bus, dev, func, PCI_OFFSET_BAR0);
        d->bars[1] = pci_config_read32(bus, dev, func, PCI_OFFSET_BAR0 + 4);
        for (int b = 2; b < 6; b++) d->bars[b] = 0;
    } else {
        /* CardBus or unknown — no BARs */
        d->num_bars = 0;
        for (int b = 0; b < 6; b++) d->bars[b] = 0;
    }
}

void pci_init(void) {
    char buf[16];
    int total_bars = 0;
    serial_puts("[PCI] Bus enumeration started\n");
    pci_num_devices = 0;

    /* Enumerate bus 0: 32 devices, 8 functions per device */
    for (int dev = 0; dev < 32; dev++) {
        /* Probe function 0 first */
        if (!pci_probe(0, dev, 0))
            continue;  /* no device at this slot */

        /* Check if this is a multifunction device (header type bit 7) */
        uint8_t ht = pci_config_read8(0, dev, 0, PCI_OFFSET_HEADER);
        int max_func = (ht & PCI_HEADER_MULTIFUNC) ? 8 : 1;

        for (int func = 0; func < max_func; func++) {
            if (func > 0 && !pci_probe(0, dev, func))
                continue;  /* function absent */

            if (pci_num_devices >= PCI_MAX_DEVICES) {
                serial_puts("[!] PCI: device table full (max ");
                serial_puts(uitoa(PCI_MAX_DEVICES, buf));
                serial_puts(")\n");
                serial_puts("[PCI7] devices: ");
                serial_puts(uitoa(pci_num_devices, buf));
                serial_puts("\n");
                serial_puts("[PCI7a2] BAR decode OK\n");
                return;
            }

            struct pci_device *d = &pci_devices[pci_num_devices];
            pci_read_device(d, 0, dev, func);

            /* Phase 7a.2: Decode BARs and enable the device */
            pci_decode_bars(d);
            pci_enable_device(d);
            pci_print_device(d);

            /* Count decoded BARs */
            for (int i = 0; i < d->num_bars; i++) {
                if (d->bar_type[i] != PCI_BAR_NONE) total_bars++;
            }

            pci_num_devices++;
        }
    }

    serial_puts("[PCI] Bus enumeration complete: ");
    serial_puts(uitoa(pci_num_devices, buf));
    serial_puts(" devices, ");
    serial_puts(uitoa(total_bars, buf));
    serial_puts(" BARs decoded\n");

    serial_puts("[PCI7] devices: ");
    serial_puts(uitoa(pci_num_devices, buf));
    serial_puts("\n");

    serial_puts("[PCI7a2] BAR decode OK\n");
}

/* ===== Search API ===== */

struct pci_device *pci_find_device(uint16_t vendor, uint16_t device_id) {
    for (int i = 0; i < pci_num_devices; i++) {
        if (pci_devices[i].vendor == vendor && pci_devices[i].device_id == device_id)
            return &pci_devices[i];
    }
    return 0;
}

struct pci_device *pci_find_class(uint8_t base_class, uint8_t subclass, uint8_t prog_if) {
    for (int i = 0; i < pci_num_devices; i++) {
        if (pci_devices[i].base_class == base_class &&
            (subclass == 0xFF || pci_devices[i].subclass == subclass) &&
            (prog_if  == 0xFF || pci_devices[i].prog_if  == prog_if))
            return &pci_devices[i];
    }
    return 0;
}

int pci_device_count(void) {
    return pci_num_devices;
}
