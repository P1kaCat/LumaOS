/* pci.h — PCI bus enumeration + BAR decode + device enable (Phase 7a.1 + 7a.2)
 *
 * PCI configuration space access via I/O ports 0xCF8/0xCFC.
 * Enumerates bus 0, devices 0-31, functions 0-7 (multifunction-aware).
 *
 * Phase 7a.1: read-only enumeration (vendor/device/class/BARs/IRQ).
 * Phase 7a.2: BAR size decoding, device enable (Command register),
 *             config space write support.
 */
#ifndef LUMAOS_PCI_H
#define LUMAOS_PCI_H

#include <stdint.h>

/* ===== Config space I/O ports ===== */
#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

/* ===== Capacity =====
 * QEMU i440FX typically exposes ~10-15 devices, but bus 0 can have
 * up to 32 devices × 8 functions = 256 functions. We set a generous
 * limit to avoid silently dropping devices on more complex setups. */
#define PCI_MAX_DEVICES 64

/* ===== Config space offsets ===== */
#define PCI_OFFSET_VENDOR    0x00  /* 16-bit */
#define PCI_OFFSET_DEVICE    0x02  /* 16-bit */
#define PCI_OFFSET_COMMAND   0x04  /* 16-bit */
#define PCI_OFFSET_STATUS    0x06  /* 16-bit */
#define PCI_OFFSET_CLASS     0x08  /* 8-bit prog_if at 0x09, subclass at 0x0A, class at 0x0B */
#define PCI_OFFSET_HEADER    0x0E  /* 8-bit */
#define PCI_OFFSET_BAR0      0x10  /* 32-bit, BAR0-BAR5 at 0x10-0x24 */
#define PCI_OFFSET_IRQ_LINE  0x3C  /* 8-bit */
#define PCI_OFFSET_IRQ_PIN   0x3D  /* 8-bit */

/* ===== Header types ===== */
#define PCI_HEADER_NORMAL    0x00
#define PCI_HEADER_BRIDGE    0x01
#define PCI_HEADER_CARDBUS   0x02
#define PCI_HEADER_MULTIFUNC 0x80  /* bit 7 = multifunction */

/* ===== Invalid vendor ID (no device at this BDF) ===== */
#define PCI_VENDOR_INVALID   0xFFFF

/* ===== Command register bits ===== */
#define PCI_CMD_IO          0x01  /* I/O space enable */
#define PCI_CMD_MEMORY      0x02  /* Memory space enable */
#define PCI_CMD_BUS_MASTER  0x04  /* Bus master enable */

/* ===== BAR types (decoded) ===== */
#define PCI_BAR_NONE  0   /* unused / not present */
#define PCI_BAR_MEM32 1   /* 32-bit memory BAR */
#define PCI_BAR_MEM64 2   /* 64-bit memory BAR (occupies 2 register slots) */
#define PCI_BAR_IO    3   /* I/O port BAR */

/* ===== PCI device structure ===== */
struct pci_device {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  func;
    uint16_t vendor;
    uint16_t device_id;
    uint8_t  base_class;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;   /* raw header type (includes multifunction bit) */
    uint32_t bars[6];       /* raw BAR values (0 = unused) */
    uint8_t  num_bars;      /* number of valid BARs (6 for normal, 2 for bridge) */
    uint8_t  irq_line;      /* IRQ line (from config space, not routed yet) */
    uint8_t  irq_pin;       /* IRQ pin: 0=none, 1=INTA, 2=INTB, 3=INTC, 4=INTD */

    /* Phase 7a.2: Decoded BAR info */
    uint64_t bar_addr[6];   /* decoded base address (0 = unused) */
    uint64_t bar_size[6];   /* decoded size in bytes (0 = unused) */
    uint8_t  bar_type[6];   /* PCI_BAR_* type for each BAR slot */
    uint16_t command;       /* Command register value after enable */
};

/* ===== API ===== */

/* Read 32 bits from PCI config space at the given BDF offset. */
uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/* Read 16 bits from PCI config space at the given BDF offset. */
uint16_t pci_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/* Read 8 bits from PCI config space at the given BDF offset. */
uint8_t pci_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/* Write 32 bits to PCI config space at the given BDF offset. */
void pci_config_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);

/* Write 16 bits to PCI config space at the given BDF offset. */
void pci_config_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val);

/* Initialize PCI: enumerate bus 0, decode BARs, enable devices. */
void pci_init(void);

/* Enable a PCI device: set I/O, Memory, and Bus Master bits in Command register. */
void pci_enable_device(struct pci_device *d);

/* Decode all BARs of a device (type, address, size). */
void pci_decode_bars(struct pci_device *d);

/* Find a device by vendor and device ID. Returns NULL if not found. */
struct pci_device *pci_find_device(uint16_t vendor, uint16_t device_id);

/* Find a device by class/subclass/prog_if.
 * Pass 0xFF for any field to wildcard it.
 * Returns NULL if not found. */
struct pci_device *pci_find_class(uint8_t base_class, uint8_t subclass, uint8_t prog_if);

/* Get number of detected PCI devices. */
int pci_device_count(void);

#endif /* LUMAOS_PCI_H */
