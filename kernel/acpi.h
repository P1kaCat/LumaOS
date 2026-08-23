/* acpi.h — ACPI table parsing (Phase 7a.2)
 *
 * Parses ACPI 2.0+ tables from the RSDP provided by UEFI:
 *   RSDP → XSDT → MADT, FADT, MCFG
 *
 * IMPORTANT: ACPI table layout is packed — no compiler padding.
 * XSDT entries start at offset 36 (sizeof acpi_sdt_header), NOT
 * at a compiler-aligned offset. We use explicit pointer arithmetic
 * to access entries and never rely on struct flexible arrays.
 */
#ifndef LUMAOS_ACPI_H
#define LUMAOS_ACPI_H

#include <stdint.h>

/* ===== RSDP (Root System Description Pointer) =====
 * ACPI 2.0+: 36 bytes. Naturally aligned (uint64_t at offset 24).
 */
struct acpi_rsdp {
    char     signature[8];      /* "RSD PTR " (note trailing space) */
    uint8_t  checksum;          /* ACPI 1.0 checksum (sum of first 20 bytes = 0) */
    char     oem_id[6];
    uint8_t  revision;          /* 0 = ACPI 1.0, 2 = ACPI 2.0+ */
    uint32_t rsdt_address;      /* 32-bit RSDT (ACPI 1.0) */
    uint32_t length;            /* Length of entire RSDP (36 for 2.0+) */
    uint64_t xsdt_address;      /* 64-bit XSDT (ACPI 2.0+) */
    uint8_t  extended_checksum; /* ACPI 2.0+ checksum (sum of all bytes = 0) */
    uint8_t  reserved[3];
};  /* 36 bytes — naturally aligned */

/* ===== ACPI table header (common to all SDTs) =====
 * 36 bytes. All fields ≤ uint32_t, natural alignment = 4.
 */
struct acpi_sdt_header {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};  /* 36 bytes */

/* ===== XSDT (Extended System Description Table) =====
 * Header (36 bytes) + array of uint64_t pointers.
 * We do NOT declare a flexible array member — the compiler would
 * insert padding to align uint64_t to 8 bytes, shifting entries
 * by 4 bytes. Instead, access entries via explicit pointer arithmetic:
 *
 *   uint64_t *entries = (uint64_t *)((uint8_t *)xsdt + sizeof(struct acpi_sdt_header));
 */
struct acpi_xsdt {
    struct acpi_sdt_header header;
};

/* ===== MADT (Multiple APIC Description Table) =====
 * Signature: "APIC"
 * Header (36) + uint32_t (4) + uint32_t (4) = 44 bytes. All aligned.
 */
struct acpi_madt {
    struct acpi_sdt_header header;
    uint32_t local_apic_address;   /* Physical address of LAPIC */
    uint32_t flags;                 /* Bit 0 = PCAT_COMPAT (8259 dual PIC) */
    /* Followed by interrupt controller structures (variable length) */
};

/* MADT interrupt controller structure header */
struct acpi_madt_entry_header {
    uint8_t type;
    uint8_t length;
};

#define ACPI_MADT_TYPE_LAPIC        0
#define ACPI_MADT_TYPE_IOAPIC       1
#define ACPI_MADT_TYPE_INT_SRC_OVR   2
#define ACPI_MADT_TYPE_LAPIC_NMI    4

/* ===== FADT (Fixed ACPI Description Table) =====
 * Signature: "FACP"
 * All fields are uint32_t/uint16_t/uint8_t — naturally aligned.
 * 64-bit extended fields (ACPI 2.0+) are read via raw offsets.
 */
struct acpi_fadt {
    struct acpi_sdt_header header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  reserved;
    uint8_t  preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_ctrl;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t  pm1_evt_len;
    uint8_t  pm1_cnt_len;
    uint8_t  pm2_cnt_len;
    uint8_t  pm_tmr_len;
    uint8_t  gpe0_blk_len;
    uint8_t  gpe1_blk_len;
    uint8_t  gpe1_base;
    uint8_t  cst_ctrl;
    uint16_t c2_latency;
    uint16_t c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;
    uint8_t  mon_alrm;
    uint8_t  century;
    uint8_t  reserved2[3];
    uint32_t flags2;
    /* ACPI 2.0+ extended fields follow — read via raw offset */
};

/* FADT ACPI 2.0+ x_dsdt offset from FADT start */
#define ACPI_FADT_X_DSDT_OFFSET  88

/* ===== MCFG (PCI Express Memory Mapped Config) =====
 * Signature: "MCFG"
 * Packed: header (36) + uint64_t reserved (8) = 44 bytes.
 * The uint64_t at offset 36 is NOT 8-byte aligned → packed required.
 */
struct acpi_mcfg {
    struct acpi_sdt_header header;
    uint64_t reserved;
} __attribute__((packed));

/* MCFG allocation entry (16 bytes, naturally aligned) */
struct acpi_mcfg_allocation {
    uint64_t base_address;
    uint16_t pci_segment_group;
    uint8_t  start_bus_number;
    uint8_t  end_bus_number;
    uint32_t reserved;
};

/* ===== Global state (set by acpi_init) ===== */

extern struct acpi_rsdp       *g_acpi_rsdp;
extern struct acpi_sdt_header *g_acpi_xsdt;
extern struct acpi_madt       *g_acpi_madt;
extern struct acpi_fadt       *g_acpi_fadt;
extern struct acpi_mcfg       *g_acpi_mcfg;
extern int                    g_acpi_num_tables;

/* ===== API ===== */

void acpi_init(uint64_t rsdp_phys);
int acpi_validate_checksum(struct acpi_sdt_header *sdt);
struct acpi_sdt_header *acpi_find_table(const char signature[4]);

#endif /* LUMAOS_ACPI_H */
