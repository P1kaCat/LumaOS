/* pci.c — PCI bus enumeration (Phase 7a.1)
 *
 * Read-only PCI configuration space access via I/O ports 0xCF8/0xCFC.
 * Enumerates bus 0 only (QEMU i440FX has a single PCI bus).
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
#include "cpu.h"
#include "apic.h"
#include "acpi.h"  /* serial_puts, inb/outb, inl/outl */

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

/* ===== Config space write ===== */

void pci_config_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t addr = 0x80000000u
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func << 8)
                  | ((uint32_t)offset & 0xFCu);
    outl(PCI_CONFIG_ADDR, addr);
    /* For 16-bit write, we need to read-modify-write the 32-bit dword */
    uint32_t cur = inl(PCI_CONFIG_DATA);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFFu << shift;
    uint32_t newval = (cur & ~mask) | ((uint32_t)val << shift);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, newval);
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

void pci_enable_device(struct pci_device *dev) {
    /* Read current command register */
    uint16_t cmd = pci_config_read16(dev->bus, dev->device, dev->func, PCI_OFFSET_COMMAND);
    /* Enable Memory Space + Bus Master + I/O Space */
    cmd |= (PCI_CMD_MEMORY_SPACE | PCI_CMD_BUS_MASTER | PCI_CMD_IO_SPACE);
    pci_config_write16(dev->bus, dev->device, dev->func, PCI_OFFSET_COMMAND, cmd);
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

/* ===== Serial printer for one device ===== */

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
                /* Still print the marker with what we have */
                serial_puts("[PCI7] devices: ");
                serial_puts(uitoa(pci_num_devices, buf));
                serial_puts("\n");
                return;
            }

            struct pci_device *d = &pci_devices[pci_num_devices];
            pci_read_device(d, 0, dev, func);
            pci_print_device(d);
            pci_num_devices++;
        }
    }

    serial_puts("[PCI7] devices: ");
    serial_puts(uitoa(pci_num_devices, buf));
    serial_puts("\n");
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

/* ===== Phase 7a.4: PCI Interrupt Routing ===== */

/* PIIX3 ISA bridge: bus 0, device 1, function 0 */
#define PIIX3_BUS   0
#define PIIX3_DEV   1
#define PIIX3_FUNC  0

static const char *pirq_name(int idx) {
    static const char names[4] = {'A', 'B', 'C', 'D'};
    if (idx >= 0 && idx < 4) return &names[idx];
    return "?";
}

/* Print a single char via serial_puts */
static void serial_putc2(char c) {
    char s[2]; s[0] = c; s[1] = 0;
    serial_puts(s);
}

void pci_irq_init(void) {
    char buf[16];

    serial_puts("\n[*] Configuring PCI interrupt routing...\n");

    /* --- 1. Read PIIX3 PIRQ routing registers --- */
    uint8_t pirq_regs[4];
    pirq_regs[0] = pci_config_read8(PIIX3_BUS, PIIX3_DEV, PIIX3_FUNC, PIIX3_PIRQA_OFFSET);
    pirq_regs[1] = pci_config_read8(PIIX3_BUS, PIIX3_DEV, PIIX3_FUNC, PIIX3_PIRQB_OFFSET);
    pirq_regs[2] = pci_config_read8(PIIX3_BUS, PIIX3_DEV, PIIX3_FUNC, PIIX3_PIRQC_OFFSET);
    pirq_regs[3] = pci_config_read8(PIIX3_BUS, PIIX3_DEV, PIIX3_FUNC, PIIX3_PIRQD_OFFSET);

    serial_puts("  PIIX3 PIRQ routing:\n");
    for (int i = 0; i < 4; i++) {
        uint8_t r = pirq_regs[i];
        serial_puts("    PIRQ");
        serial_putc2(pirq_name(i)[0]);
        if (r & 0x80) {
            serial_puts(" = disabled\n");
        } else {
            serial_puts(" -> ISA IRQ ");
            serial_puts(uitoa(r & 0x0F, buf));
            serial_puts("\n");
        }
    }

    /* --- 2. Get destination LAPIC ID --- */
    uint8_t dest_apic_id = lapic_get_id();
    if (g_acpi_lapic_found)
        dest_apic_id = g_acpi_lapic_apic_id;

    /* --- 3. Configure I/O APIC for PCI GSI 16-19 ---
     * On QEMU i440FX, PIIX3 PIRQA-PIRQD are connected to I/O APIC
     * pins 16-19. PCI interrupts are active-low, level-triggered
     * (unlike ISA which is active-high, edge-triggered).
     *
     * All entries are masked — no PCI device drivers yet.
     * Future drivers will unmask their GSI when registering a handler.
     */
    for (int pirq = 0; pirq < PCI_NUM_PIRQ; pirq++) {
        uint8_t gsi = (uint8_t)(PCI_GSI_BASE + pirq);
        uint8_t vector = (uint8_t)(PCI_IRQ_VECTOR_BASE + pirq);

        /* PCI interrupts: active-low, level-triggered, masked */
        apic_route_irq(gsi, vector, dest_apic_id, 1, 1, 1 /* masked */);
    }

    serial_puts("  [+] I/O APIC GSI 16-19 configured (PCI, active-low, level)\n");

    /* --- 4. Map each PCI device to its PIRQ/GSI/vector --- */
    serial_puts("  PCI interrupt routing table:\n");

    int num_routed = 0;
    for (int i = 0; i < pci_num_devices; i++) {
        struct pci_device *d = &pci_devices[i];
        if (d->irq_pin == 0) continue;  /* no interrupt pin */

        /* Calculate PIRQ index from slot and pin */
        int pirq_idx = pci_pirq_index(d->device, d->irq_pin);
        uint8_t gsi = (uint8_t)(PCI_GSI_BASE + pirq_idx);
        uint8_t vector = (uint8_t)(PCI_IRQ_VECTOR_BASE + pirq_idx);
        uint8_t isa_irq_from_pirq = pirq_regs[pirq_idx] & 0x0F;

        serial_puts("    ");
        serial_puts(uxtoa_pad(d->bus, buf, 2)); serial_puts(":");
        serial_puts(uxtoa_pad(d->device, buf, 2)); serial_puts(".");
        serial_puts(uitoa(d->func, buf));
        serial_puts(" INT");
        serial_putc2(pirq_name(pirq_idx)[0]);
        serial_puts(" -> PIRQ");
        serial_putc2(pirq_name(pirq_idx)[0]);
        serial_puts(" -> GSI ");
        serial_puts(uitoa(gsi, buf));
        serial_puts(" -> vec ");
        serial_puts(uitoa(vector, buf));
        serial_puts(" (line=");
        serial_puts(uitoa(d->irq_line, buf));
        serial_puts(", PIRQ->ISA ");
        serial_puts(uitoa(isa_irq_from_pirq, buf));
        serial_puts(")\n");

        num_routed++;
    }

    serial_puts("  [+] ");
    serial_puts(uitoa(num_routed, buf));
    serial_puts(" PCI devices routed\n");

    serial_puts("[PCI7a4] IRQ routing OK\n");
}
