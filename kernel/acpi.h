/* acpi.h — ACPI table parsing (Phase 7a.2)
 *
 * Parses ACPI 2.0+ tables from the RSDP provided by UEFI:
 *   RSDP → XSDT → MADT, FADT, MCFG
 *
 * The kernel receives the RSDP physical address from the bootloader
 * via the handoff struct. All ACPI tables are in memory that survives
 * ExitBootServices (EFI_RUNTIME_SERVICES_DATA).
 *
 * Memory model: identity-mapped, so physical addresses can be used
 * directly as pointers.
 */
#ifndef LUMAOS_ACPI_H
#define LUMAOS_ACPI_H

#include <stdint.h>

/* ===== RSDP (Root System Description Pointer) =====
 * ACPI 2.0+: 36 bytes (extended fields present)
 * ACPI 1.0:   20 bytes (no extended fields)
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
};

/* ===== ACPI table header (common to all SDTs) ===== */
struct acpi_sdt_header {
    char     signature[4];      /* e.g. "XSDT", "APIC", "FACP", "MCFG" */
    uint32_t length;            /* Total length including header */
    uint8_t  revision;
    uint8_t  checksum;          /* Sum of all bytes = 0 mod 256 */
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};  /* 36 bytes */

/* ===== XSDT (Extended System Description Table) =====
 * Header + array of 64-bit pointers to other SDTs
 */
struct acpi_xsdt {
    struct acpi_sdt_header header;
    uint64_t entries[0];       /* Variable length array */
};

/* ===== MADT (Multiple APIC Description Table) =====
 * Signature: "APIC"
 */
struct acpi_madt {
    struct acpi_sdt_header header;
    uint32_t local_apic_address;   /* Physical address of LAPIC */
    uint32_t flags;                 /* Bit 0 = PCAT_COMPAT (8259 dual PIC) */
    /* Followed by interrupt controller structures (variable length) */
};

/* MADT interrupt controller structure header */
struct acpi_madt_entry_header {
    uint8_t type;   /* 0 = LAPIC, 1 = IOAPIC, 2 = Int Source Override, ... */
    uint8_t length; /* Length of this entry including header */
};

/* MADT entry types */
#define ACPI_MADT_TYPE_LAPIC        0
#define ACPI_MADT_TYPE_IOAPIC       1
#define ACPI_MADT_TYPE_INT_SRC_OVR   2
#define ACPI_MADT_TYPE_LAPIC_NMI    4

/* ===== FADT (Fixed ACPI Description Table) =====
 * Signature: "FACP"
 * Minimal definition — enough to read key fields.
 */
struct acpi_fadt {
    struct acpi_sdt_header header;
    uint32_t firmware_ctrl;        /* FACS (32-bit, ACPI 1.0) */
    uint32_t dsdt;                 /* DSDT (32-bit, ACPI 1.0) */
    uint8_t  reserved;
    uint8_t  preferred_pm_profile;
    uint16_t sci_int;              /* SCI interrupt vector (GSI) */
    uint32_t smi_cmd;              /* SMI command port */
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
    /* ACPI 2.0+ extended fields follow... */
    /* We read x_firmware_ctrl and x_dsdt at known offsets */
};

/* FADT ACPI 2.0+ extended field offsets (from FADT start) */
/* After the 32-bit fields, at offset 76 in the struct above,
 * ACPI 2.0+ adds:
 *   uint32_t flags2;
 *   uint64_t x_firmware_ctrl;  (FACS, 64-bit)
 *   uint64_t x_dsdt;           (DSDT, 64-bit)
 * We read these as raw offsets to avoid struct complexity.
 */
#define ACPI_FADT_X_DSDT_OFFSET  88   /* offset of x_dsdt from FADT start (ACPI 2.0+) */

/* ===== MCFG (PCI Express Memory Mapped Config) =====
 * Signature: "MCFG"
 */
struct acpi_mcfg {
    struct acpi_sdt_header header;
    uint64_t reserved;       /* Must be 0 */
    /* Followed by allocation entries (16 bytes each) */
};

struct acpi_mcfg_allocation {
    uint64_t base_address;       /* ECAM base address */
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

/* Parse ACPI tables from RSDP. Called from kernel_main with ho->rsdp. */
void acpi_init(uint64_t rsdp_phys);

/* Validate an ACPI SDT checksum (sum of all bytes = 0 mod 256). */
int acpi_validate_checksum(struct acpi_sdt_header *sdt);

/* Find an SDT in the XSDT by 4-char signature. Returns NULL if not found. */
struct acpi_sdt_header *acpi_find_table(const char signature[4]);

#endif /* LUMAOS_ACPI_H */
