/* apic.c — Local APIC + I/O APIC (Phase 7a.3)
 *
 * Replaces the legacy 8259 PIC with the APIC subsystem:
 *   1. Disable PIC (mask all 16 ISA IRQs)
 *   2. Enable LAPIC (spurious vector, TPR=0, mask LINT0/LINT1/timer)
 *   3. Mask all I/O APIC redirection entries
 *   4. Configure I/O APIC routing: ISA IRQ → GSI → vector 32+IRQ
 *      (using ACPI interrupt source overrides for correct GSI mapping)
 *   5. Unmask the IRQs we actually use (timer=IRQ0, keyboard=IRQ1)
 *
 * After init, EOI is done via the LAPIC EOI register, not the PIC.
 * The existing IDT entries (vectors 32-47) continue to work unchanged.
 *
 * QEMU i440FX specifics:
 *   - LAPIC at 0xFEE00000, I/O APIC at 0xFEC00000 (from MADT)
 *   - ISA IRQ 0 (PIT timer) → GSI 2 (via IntSrcOvr)
 *   - ISA IRQ 1 (keyboard) → GSI 1 (identity)
 *   - I/O APIC has 24 redirection entries
 */
#include "apic.h"
#include "acpi.h"
#include "cpu.h"

/* ===== State ===== */
static uint32_t lapic_base_addr = 0xFEE00000;
static uint32_t ioapic_base_addr = 0xFEC00000;
static int      apic_active = 0;

/* ===== LAPIC MMIO access ===== */

static inline volatile uint32_t *lapic_reg(uint32_t offset) {
    return (volatile uint32_t *)(unsigned long)(lapic_base_addr + offset);
}

static uint32_t lapic_read(uint32_t offset) {
    return *lapic_reg(offset);
}

static void lapic_write(uint32_t offset, uint32_t val) {
    *lapic_reg(offset) = val;
}

/* ===== I/O APIC access (via index/data window) ===== */

static inline volatile uint32_t *ioapic_regsel(void) {
    return (volatile uint32_t *)(unsigned long)(ioapic_base_addr + IOAPIC_IOREGSEL);
}

static inline volatile uint32_t *ioapic_win(void) {
    return (volatile uint32_t *)(unsigned long)(ioapic_base_addr + IOAPIO_IOWIN);
}

static uint32_t ioapic_read(uint8_t reg) {
    *ioapic_regsel() = reg;
    return *ioapic_win();
}

static void ioapic_write(uint8_t reg, uint32_t val) {
    *ioapic_regsel() = reg;
    *ioapic_win() = val;
}

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

/* ===== PIC disable ===== */

void pic_disable(void) {
    /* Mask all interrupts on both 8259 PICs */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* ===== EOI ===== */

void apic_eoi(uint8_t irq) {
    if (apic_active) {
        /* LAPIC EOI: write 0 to EOI register */
        lapic_write(LAPIC_EOI, 0);
    } else {
        /* PIC fallback */
        pic_eoi(irq);
    }
}

/* ===== Status ===== */

int apic_is_active(void) {
    return apic_active;
}

uint8_t lapic_get_id(void) {
    return (uint8_t)(lapic_read(LAPIC_ID) >> 24);
}

/* ===== GSI mapping ===== */

uint32_t isa_irq_to_gsi(uint8_t isa_irq) {
    /* Check ACPI interrupt source overrides */
    for (int i = 0; i < g_acpi_num_int_src_overrides; i++) {
        if (g_acpi_int_src_overrides[i].source == isa_irq)
            return g_acpi_int_src_overrides[i].gsi;
    }
    /* Identity mapping: ISA IRQ = GSI */
    return isa_irq;
}

/* ===== Generic I/O APIC routing (public API) ===== */

void apic_route_irq(uint8_t gsi, uint8_t vector, uint8_t dest_apic_id,
                     int polarity_low, int trigger_level, int mask) {
    /* Low 32 bits: vector | delivery=Fixed | dest=Physical
     *              | polarity | trigger mode | mask */
    uint32_t low = (uint32_t)vector
                 | IOAPIC_REDIR_DELIVERY_FIXED
                 | IOAPIC_REDIR_DEST_PHYSICAL
                 | (polarity_low  ? IOAPIC_REDIR_POLARITY_LOW  : IOAPIC_REDIR_POLARITY_HIGH)
                 | (trigger_level ? IOAPIC_REDIR_TRIGGER_LEVEL : IOAPIC_REDIR_TRIGGER_EDGE)
                 | (mask ? IOAPIC_REDIR_MASKED : IOAPIC_REDIR_UNMASKED);

    /* High 32 bits: destination APIC ID in bits 24-31 */
    uint32_t high = ((uint32_t)dest_apic_id) << 24;

    uint8_t reg = (uint8_t)(IOAPIC_REG_REDTBL + gsi * 2);
    ioapic_write(reg, low);
    ioapic_write((uint8_t)(reg + 1), high);
}

/* ===== I/O APIC configuration ===== */

static int ioapic_max_redir(void) {
    uint32_t ver = ioapic_read(IOAPIC_REG_VER);
    /* Max redirection entry = bits 16-23 */
    return (int)((ver >> 16) & 0xFF) + 1;
}

static void ioapic_mask_all_entries(void) {
    int max = ioapic_max_redir();
    for (int i = 0; i < max; i++) {
        uint8_t reg = (uint8_t)(IOAPIC_REG_REDTBL + i * 2);
        ioapic_write(reg, IOAPIC_REDIR_MASKED);
        ioapic_write((uint8_t)(reg + 1), 0);
    }
}

static void ioapic_set_redirect(uint8_t gsi, uint8_t vector,
                                 uint8_t dest_apic_id, int mask) {
    /* Low 32 bits: vector | delivery=Fixed | dest=Physical
     *              | polarity=ActiveHigh | trigger=Edge | mask */
    uint32_t low = (uint32_t)vector
                 | IOAPIC_REDIR_DELIVERY_FIXED
                 | IOAPIC_REDIR_DEST_PHYSICAL
                 | IOAPIC_REDIR_POLARITY_HIGH
                 | IOAPIC_REDIR_TRIGGER_EDGE
                 | (mask ? IOAPIC_REDIR_MASKED : IOAPIC_REDIR_UNMASKED);

    /* High 32 bits: destination APIC ID in bits 24-31 */
    uint32_t high = ((uint32_t)dest_apic_id) << 24;

    uint8_t reg = (uint8_t)(IOAPIC_REG_REDTBL + gsi * 2);
    ioapic_write(reg, low);
    ioapic_write((uint8_t)(reg + 1), high);
}

/* ===== Main init ===== */

void apic_init(void) {
    char buf[16];

    serial_puts("\n[*] Initializing APIC (Local + I/O)...\n");

    /* Get addresses from ACPI MADT */
    if (g_acpi_madt) {
        lapic_base_addr = g_acpi_madt->local_apic_address;
    }
    if (g_acpi_ioapic_found) {
        ioapic_base_addr = g_acpi_ioapic_addr;
    }

    serial_puts("  LAPIC base=0x");
    serial_puts(uxtoa_local(lapic_base_addr, buf));
    serial_puts(" IOAPIC base=0x");
    serial_puts(uxtoa_local(ioapic_base_addr, buf));
    serial_puts("\n");

    /* --- 1. Disable PIC --- */
    pic_disable();
    serial_puts("  [+] PIC disabled (all IRQs masked)\n");

    /* --- 2. Enable LAPIC --- */
    /* Read LAPIC ID and version */
    uint32_t lapic_id_reg = lapic_read(LAPIC_ID);
    uint32_t lapic_ver = lapic_read(LAPIC_VER);
    uint8_t my_apic_id = (uint8_t)(lapic_id_reg >> 24);
    uint8_t ver = (uint8_t)(lapic_ver & 0xFF);
    uint8_t max_lvt = (uint8_t)((lapic_ver >> 16) & 0xFF);

    serial_puts("  LAPIC ID=");
    serial_puts(uitoa_local(my_apic_id, buf));
    serial_puts(" version=");
    serial_puts(uitoa_local(ver, buf));
    serial_puts(" max_lvt=");
    serial_puts(uitoa_local(max_lvt, buf));
    serial_puts("\n");

    /* Enable LAPIC via Spurious Interrupt Vector Register:
     *   bit 8 = APIC Software Enable (1 = enabled)
     *   bits 0-7 = spurious vector (0xFF) */
    lapic_write(LAPIC_SVR, LAPIC_SPURIOUS_VECTOR | 0x100);
    serial_puts("  [+] LAPIC enabled (SVR=0x");
    serial_puts(uxtoa_local(LAPIC_SPURIOUS_VECTOR | 0x100, buf));
    serial_puts(")\n");

    /* Set Task Priority to 0 (accept all interrupts) */
    lapic_write(LAPIC_TPR, 0);

    /* Mask LINT0, LINT1, and LAPIC timer (no legacy PIC through LINT,
     * timer configured later) */
    lapic_write(LAPIC_LVT_LINT0, IOAPIC_REDIR_MASKED);
    lapic_write(LAPIC_LVT_LINT1, IOAPIC_REDIR_MASKED);
    lapic_write(LAPIC_LVT_TIMER, IOAPIC_REDIR_MASKED);
    lapic_write(LAPIC_LVT_ERROR, IOAPIC_REDIR_MASKED);

    /* --- 3. Configure I/O APIC --- */
    /* Read I/O APIC version and entry count */
    uint32_t ioapic_ver_reg = ioapic_read(IOAPIC_REG_VER);
    uint8_t ioapic_id_reg = (uint8_t)(ioapic_read(IOAPIC_REG_ID) >> 24);
    int ioapic_entries = ioapic_max_redir();

    serial_puts("  IOAPIC ID=");
    serial_puts(uitoa_local(ioapic_id_reg, buf));
    serial_puts(" version=");
    serial_puts(uitoa_local((uint8_t)(ioapic_ver_reg & 0xFF), buf));
    serial_puts(" redir_entries=");
    serial_puts(uitoa_local(ioapic_entries, buf));
    serial_puts("\n");

    /* Mask all I/O APIC entries first */
    ioapic_mask_all_entries();

    /* Configure routing for ISA IRQs 0-15:
     * Each ISA IRQ maps to a GSI (via int source overrides or identity),
     * and the GSI maps to vector 32 + ISA_IRQ (same as PIC remap).
     *
     * Key QEMU overrides:
     *   ISA IRQ 0 (PIT timer) → GSI 2
     *   ISA IRQ 1 (keyboard)  → GSI 1
     *
     * We unmask only IRQ 0 (timer) and IRQ 1 (keyboard).
     * All other IRQs are left masked in the I/O APIC. */

    /* Determine destination APIC ID */
    uint8_t dest_apic_id = my_apic_id;
    if (g_acpi_lapic_found)
        dest_apic_id = g_acpi_lapic_apic_id;

    /* Configure all 16 ISA IRQs in the I/O APIC */
    for (int isa_irq = 0; isa_irq < 16; isa_irq++) {
        uint32_t gsi = isa_irq_to_gsi((uint8_t)isa_irq);
        uint8_t vector = (uint8_t)(32 + isa_irq);

        /* Skip cascaded IRQ 2 (PIC2 cascade — not a real device IRQ) */
        if (isa_irq == 2) continue;

        /* Only timer (IRQ 0) and keyboard (IRQ 1) are unmasked.
         * All others start masked. */
        int mask = (isa_irq == 0 || isa_irq == 1) ? 0 : 1;

        ioapic_set_redirect((uint8_t)gsi, vector, dest_apic_id, mask);
    }

    serial_puts("  [+] I/O APIC configured (16 ISA IRQs routed to vectors 32-47)\n");

    /* Print the actual routing for timer and keyboard */
    {
        uint32_t gsi_timer = isa_irq_to_gsi(0);
        uint32_t gsi_kb = isa_irq_to_gsi(1);
        serial_puts("  Timer:  ISA IRQ 0 -> GSI ");
        serial_puts(uitoa_local(gsi_timer, buf));
        serial_puts(" -> vector 32 (unmasked)\n");
        serial_puts("  Keyboard: ISA IRQ 1 -> GSI ");
        serial_puts(uitoa_local(gsi_kb, buf));
        serial_puts(" -> vector 33 (unmasked)\n");
    }

    apic_active = 1;
    serial_puts("  [+] APIC active — EOI via LAPIC\n");

    serial_puts("[APIC7a3] LAPIC + IOAPIC enabled\n");
}
