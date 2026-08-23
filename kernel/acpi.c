/* acpi.c — ACPI table parsing (Phase 7a.2)
 *
 * Parses ACPI 2.0+ tables provided by UEFI firmware:
 *   RSDP → XSDT → MADT (APIC), FADT (FACP), MCFG (if available)
 *
 * The RSDP physical address comes from the bootloader via the handoff
 * struct (ho->rsdp). The kernel uses identity mapping, so physical
 * addresses work directly as pointers.
 *
 * XSDT entries are accessed via explicit pointer arithmetic:
 *   uint64_t *entries = (uint64_t *)((uint8_t *)xsdt + 36);
 * Never via struct flexible array (compiler padding would misalign).
 */
#include "acpi.h"
#include "cpu.h"  /* serial_puts */

/* ===== Global state ===== */
struct acpi_rsdp       *g_acpi_rsdp  = 0;
struct acpi_sdt_header *g_acpi_xsdt  = 0;
struct acpi_madt       *g_acpi_madt  = 0;
struct acpi_fadt       *g_acpi_fadt  = 0;
struct acpi_mcfg       *g_acpi_mcfg  = 0;
int                    g_acpi_num_tables = 0;

/* IOAPIC info (from MADT) */
uint32_t g_acpi_ioapic_addr   = 0xFEC00000;  /* default QEMU */
uint8_t  g_acpi_ioapic_id     = 0;
uint32_t g_acpi_ioapic_gsi_base = 0;
int      g_acpi_ioapic_found  = 0;

/* LAPIC info (from MADT — first enabled CPU) */
uint8_t  g_acpi_lapic_apic_id = 0;
int      g_acpi_lapic_found   = 0;

/* Interrupt source overrides */
struct acpi_int_src_override g_acpi_int_src_overrides[ACPI_MAX_INT_SRC_OVERRIDES];
int g_acpi_num_int_src_overrides = 0;

/* ===== Local helpers ===== */

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

/* Compare first 4 bytes (ACPI signatures are exactly 4 chars) */
static int sig4_equal(const char *a, const char *b) {
    for (int i = 0; i < 4; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

/* ===== Checksum validation ===== */

int acpi_validate_checksum(struct acpi_sdt_header *sdt) {
    uint32_t len = sdt->length;
    if (len < sizeof(struct acpi_sdt_header)) return 0;

    uint8_t *p = (uint8_t *)sdt;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++)
        sum = (uint8_t)(sum + p[i]);
    return sum == 0;
}

/* Validate RSDP (ACPI 2.0+: checksum over full length) */
static int rsdp_validate(struct acpi_rsdp *rsdp) {
    const char expected[8] = {'R','S','D',' ','P','T','R',' '};
    for (int i = 0; i < 8; i++)
        if (rsdp->signature[i] != expected[i]) return 0;

    if (rsdp->revision >= 2 && rsdp->length >= 36) {
        uint8_t *p = (uint8_t *)rsdp;
        uint8_t sum = 0;
        for (uint32_t i = 0; i < rsdp->length; i++)
            sum = (uint8_t)(sum + p[i]);
        return sum == 0;
    } else {
        /* ACPI 1.0: checksum over first 20 bytes */
        uint8_t *p = (uint8_t *)rsdp;
        uint8_t sum = 0;
        for (int i = 0; i < 20; i++)
            sum = (uint8_t)(sum + p[i]);
        return sum == 0;
    }
}

/* ===== XSDT entry access via explicit pointer arithmetic =====
 *
 * XSDT layout:  [acpi_sdt_header (36 bytes)] [uint64_t entry[0]] [uint64_t entry[1]] ...
 *
 * We compute entries pointer as: (uint64_t *)(base + 36)
 * This avoids any compiler-inserted padding that a flexible array
 * member would introduce.
 */

static uint64_t *xsdt_entries(struct acpi_xsdt *xsdt) {
    return (uint64_t *)((uint8_t *)xsdt + sizeof(struct acpi_sdt_header));
}

static uint32_t xsdt_entry_count(struct acpi_xsdt *xsdt) {
    uint32_t length = xsdt->header.length;
    if (length < sizeof(struct acpi_sdt_header))
        return 0;
    uint32_t entries_bytes = length - sizeof(struct acpi_sdt_header);
    return entries_bytes / sizeof(uint64_t);
}

/* ===== Find table by signature in XSDT ===== */

struct acpi_sdt_header *acpi_find_table(const char signature[4]) {
    if (!g_acpi_xsdt) return 0;

    struct acpi_xsdt *xsdt = (struct acpi_xsdt *)g_acpi_xsdt;
    uint32_t count = xsdt_entry_count(xsdt);
    uint64_t *entries = xsdt_entries(xsdt);

    for (uint32_t i = 0; i < count; i++) {
        struct acpi_sdt_header *sdt = (struct acpi_sdt_header *)entries[i];
        if (!sdt) continue;
        if (sig4_equal(sdt->signature, signature))
            return sdt;
    }
    return 0;
}

/* ===== MADT parsing ===== */

static void parse_madt(struct acpi_madt *madt) {
    char buf[16];

    serial_puts("[ACPI] MADT (APIC) at 0x");
    serial_puts(uxtoa((uint64_t)(unsigned long)madt, buf));
    serial_puts("\n");
    serial_puts("  LAPIC addr=0x");
    serial_puts(uxtoa(madt->local_apic_address, buf));
    serial_puts(" flags=0x");
    serial_puts(uxtoa(madt->flags, buf));
    serial_puts(madt->flags & 1 ? " (PCAT_COMPAT)\n" : "\n");

    /* Walk interrupt controller structures */
    uint8_t *p = (uint8_t *)madt + sizeof(struct acpi_madt);
    uint8_t *end = (uint8_t *)madt + madt->header.length;
    int num_lapic = 0, num_ioapic = 0, num_isor = 0;

    while (p + 2 <= end) {
        struct acpi_madt_entry_header *eh = (struct acpi_madt_entry_header *)p;
        if (eh->length == 0) break;

        switch (eh->type) {
            case ACPI_MADT_TYPE_LAPIC:
                num_lapic++;
                /* LAPIC entry: type(1) len(1) acpi_proc_id(1) apic_id(1) flags(4) */
                if (p + 8 <= end && !g_acpi_lapic_found) {
                    uint8_t apic_id = p[3];
                    uint32_t cpu_flags = *(uint32_t *)(p + 4);
                    if (cpu_flags & 1) {  /* Processor Enabled */
                        g_acpi_lapic_apic_id = apic_id;
                        g_acpi_lapic_found = 1;
                        serial_puts("  LAPIC CPU apic_id=");
                        serial_puts(uitoa(apic_id, buf));
                        serial_puts("\n");
                    }
                }
                break;
            case ACPI_MADT_TYPE_IOAPIC:
                num_ioapic++;
                /* IOAPIC entry: type(1) len(1) id(1) reserved(1) addr(4) gsi_base(4) = 12 bytes */
                if (p + 12 <= end) {
                    uint8_t ioapic_id = p[2];
                    uint32_t ioapic_addr = *(uint32_t *)(p + 4);
                    uint32_t gsi_base = *(uint32_t *)(p + 8);
                    g_acpi_ioapic_id = ioapic_id;
                    g_acpi_ioapic_addr = ioapic_addr;
                    g_acpi_ioapic_gsi_base = gsi_base;
                    g_acpi_ioapic_found = 1;
                    serial_puts("  IOAPIC id=");
                    serial_puts(uitoa(ioapic_id, buf));
                    serial_puts(" addr=0x");
                    serial_puts(uxtoa(ioapic_addr, buf));
                    serial_puts(" gsi_base=");
                    serial_puts(uitoa(gsi_base, buf));
                    serial_puts("\n");
                }
                break;
            case ACPI_MADT_TYPE_INT_SRC_OVR:
                num_isor++;
                /* Int Source Override: type(1) len(1) bus(1) source(1) gsi(4) flags(2) = 10 bytes */
                if (p + 10 <= end &&
                    g_acpi_num_int_src_overrides < ACPI_MAX_INT_SRC_OVERRIDES) {
                    struct acpi_int_src_override *iso =
                        &g_acpi_int_src_overrides[g_acpi_num_int_src_overrides];
                    iso->bus = p[2];
                    iso->source = p[3];
                    iso->gsi = *(uint32_t *)(p + 4);
                    iso->flags = *(uint16_t *)(p + 8);
                    g_acpi_num_int_src_overrides++;
                    serial_puts("  IntSrcOvr: bus=");
                    serial_puts(uitoa(iso->bus, buf));
                    serial_puts(" irq=");
                    serial_puts(uitoa(iso->source, buf));
                    serial_puts(" -> gsi=");
                    serial_puts(uitoa(iso->gsi, buf));
                    serial_puts(" flags=0x");
                    serial_puts(uxtoa(iso->flags, buf));
                    serial_puts("\n");
                }
                break;
            default:
                break;
        }
        p += eh->length;
    }

    serial_puts("  LAPIC entries: ");
    serial_puts(uitoa(num_lapic, buf));
    serial_puts(", IOAPIC entries: ");
    serial_puts(uitoa(num_ioapic, buf));
    serial_puts(", IntSrcOvr: ");
    serial_puts(uitoa(num_isor, buf));
    serial_puts("\n");
}

/* ===== FADT parsing ===== */

static void parse_fadt(struct acpi_fadt *fadt) {
    char buf[16];

    serial_puts("[ACPI] FADT (FACP) at 0x");
    serial_puts(uxtoa((uint64_t)(unsigned long)fadt, buf));
    serial_puts("\n");
    serial_puts("  SCI_INT=");
    serial_puts(uitoa(fadt->sci_int, buf));
    serial_puts(" SMI_CMD=0x");
    serial_puts(uxtoa(fadt->smi_cmd, buf));
    serial_puts(" PM1a_EVT=0x");
    serial_puts(uxtoa(fadt->pm1a_evt_blk, buf));
    serial_puts(" PM_TMR=0x");
    serial_puts(uxtoa(fadt->pm_tmr_blk, buf));
    serial_puts("\n");

    /* 64-bit DSDT pointer (ACPI 2.0+) via raw offset */
    if (fadt->header.length >= ACPI_FADT_X_DSDT_OFFSET + 8) {
        uint64_t x_dsdt = *(uint64_t *)((uint8_t *)fadt + ACPI_FADT_X_DSDT_OFFSET);
        if (x_dsdt) {
            serial_puts("  DSDT (64-bit)=0x");
            serial_puts(uxtoa(x_dsdt, buf));
            serial_puts("\n");
        }
    } else if (fadt->dsdt) {
        serial_puts("  DSDT (32-bit)=0x");
        serial_puts(uxtoa(fadt->dsdt, buf));
        serial_puts("\n");
    }
}

/* ===== MCFG parsing ===== */

static void parse_mcfg(struct acpi_mcfg *mcfg) {
    char buf[16];

    serial_puts("[ACPI] MCFG at 0x");
    serial_puts(uxtoa((uint64_t)(unsigned long)mcfg, buf));
    serial_puts("\n");

    /* MCFG: header (36) + reserved (8) = 44, then allocation entries (16 each) */
    uint8_t *p = (uint8_t *)mcfg + sizeof(struct acpi_mcfg);
    uint8_t *end = (uint8_t *)mcfg + mcfg->header.length;
    int num_entries = 0;

    while (p + 16 <= end) {
        struct acpi_mcfg_allocation *alloc = (struct acpi_mcfg_allocation *)p;
        if (alloc->base_address == 0) break;

        serial_puts("  ECAM base=0x");
        serial_puts(uxtoa(alloc->base_address, buf));
        serial_puts(" segment=");
        serial_puts(uitoa(alloc->pci_segment_group, buf));
        serial_puts(" buses ");
        serial_puts(uitoa(alloc->start_bus_number, buf));
        serial_puts("-");
        serial_puts(uitoa(alloc->end_bus_number, buf));
        serial_puts("\n");

        num_entries++;
        p += 16;
    }

    if (num_entries == 0)
        serial_puts("  No ECAM allocation entries\n");
}

/* ===== Main init ===== */

void acpi_init(uint64_t rsdp_phys) {
    char buf[16];

    if (!rsdp_phys) {
        serial_puts("[ACPI] No RSDP from bootloader — ACPI disabled\n");
        return;
    }

    serial_puts("[*] Parsing ACPI tables...\n");

    /* === 1. Validate RSDP === */
    struct acpi_rsdp *rsdp = (struct acpi_rsdp *)(unsigned long)rsdp_phys;
    g_acpi_rsdp = rsdp;

    serial_puts("[ACPI] RSDP at 0x");
    serial_puts(uxtoa(rsdp_phys, buf));
    serial_puts("\n");

    if (!rsdp_validate(rsdp)) {
        serial_puts("[!] RSDP validation failed (bad signature or checksum)\n");
        return;
    }

    serial_puts("  Revision: ");
    serial_puts(uitoa(rsdp->revision, buf));
    serial_puts(rsdp->revision >= 2 ? " (ACPI 2.0+)\n" : " (ACPI 1.0)\n");

    if (rsdp->revision < 2 || !rsdp->xsdt_address) {
        serial_puts("[!] No XSDT (ACPI 1.0 only — not supported)\n");
        return;
    }

    serial_puts("  XSDT at 0x");
    serial_puts(uxtoa(rsdp->xsdt_address, buf));
    serial_puts("\n");

    /* === 2. Validate XSDT === */
    struct acpi_sdt_header *xsdt_hdr =
        (struct acpi_sdt_header *)(unsigned long)rsdp->xsdt_address;
    g_acpi_xsdt = xsdt_hdr;

    if (!sig4_equal(xsdt_hdr->signature, "XSDT")) {
        serial_puts("[!] XSDT signature mismatch\n");
        return;
    }

    if (!acpi_validate_checksum(xsdt_hdr)) {
        serial_puts("[!] XSDT checksum invalid\n");
        return;
    }

    /* === 3. Enumerate XSDT entries (explicit pointer arithmetic) === */
    struct acpi_xsdt *xsdt = (struct acpi_xsdt *)xsdt_hdr;
    uint32_t entry_count = xsdt_entry_count(xsdt);
    uint64_t *entries = xsdt_entries(xsdt);
    g_acpi_num_tables = (int)entry_count;

    serial_puts("  XSDT has ");
    serial_puts(uitoa(entry_count, buf));
    serial_puts(" table entries\n");

    for (uint32_t i = 0; i < entry_count; i++) {
        struct acpi_sdt_header *entry =
            (struct acpi_sdt_header *)entries[i];
        if (!entry) continue;

        char sig[5];
        sig[0] = entry->signature[0];
        sig[1] = entry->signature[1];
        sig[2] = entry->signature[2];
        sig[3] = entry->signature[3];
        sig[4] = 0;

        serial_puts("  [");
        serial_puts(uitoa(i, buf));
        serial_puts("] ");
        serial_puts(sig);
        serial_puts(" at 0x");
        serial_puts(uxtoa((uint64_t)(unsigned long)entry, buf));
        serial_puts(" len=");
        serial_puts(uitoa(entry->length, buf));
        serial_puts("\n");
    }

    /* === 4. Find and parse key tables === */

    /* MADT (APIC) */
    struct acpi_sdt_header *madt_hdr = acpi_find_table("APIC");
    if (madt_hdr) {
        if (acpi_validate_checksum(madt_hdr)) {
            g_acpi_madt = (struct acpi_madt *)madt_hdr;
            parse_madt(g_acpi_madt);
        } else {
            serial_puts("[!] MADT checksum invalid\n");
        }
    } else {
        serial_puts("[ACPI] MADT not found\n");
    }

    /* FADT (FACP) */
    struct acpi_sdt_header *fadt_hdr = acpi_find_table("FACP");
    if (fadt_hdr) {
        if (acpi_validate_checksum(fadt_hdr)) {
            g_acpi_fadt = (struct acpi_fadt *)fadt_hdr;
            parse_fadt(g_acpi_fadt);
        } else {
            serial_puts("[!] FADT checksum invalid\n");
        }
    } else {
        serial_puts("[ACPI] FADT not found\n");
    }

    /* MCFG (optional — not present on i440FX/QEMU PCI, only on PCIe) */
    struct acpi_sdt_header *mcfg_hdr = acpi_find_table("MCFG");
    if (mcfg_hdr) {
        if (acpi_validate_checksum(mcfg_hdr)) {
            g_acpi_mcfg = (struct acpi_mcfg *)mcfg_hdr;
            parse_mcfg(g_acpi_mcfg);
        } else {
            serial_puts("[!] MCFG checksum invalid\n");
        }
    } else {
        serial_puts("[ACPI] MCFG not found (PCIe ECAM not available — expected on i440FX)\n");
    }

    serial_puts("[ACPI7a2] tables parsed\n");
}
