/* apic.h — Local APIC + I/O APIC (Phase 7a.3)
 *
 * Replaces the legacy 8259 PIC with the APIC interrupt subsystem:
 *   - Local APIC (LAPIC): per-CPU interrupt controller at 0xFEE00000
 *   - I/O APIC:           system interrupt router at 0xFEC00000
 *
 * The LAPIC and I/O APIC addresses come from the ACPI MADT table,
 * parsed in Phase 7a.2. The I/O APIC routes hardware interrupts
 * (ISA IRQs, via interrupt source overrides) to the LAPIC, which
 * delivers them to the CPU.
 *
 * After apic_init():
 *   - PIC is masked/disabled
 *   - LAPIC is enabled with spurious vector 0xFF
 *   - I/O APIC routes ISA IRQs 0-15 to vectors 32-47 (same as PIC remap)
 *   - EOI is done via LAPIC EOI register (lapic_eoi), not PIC
 *   - The existing IDT entries (vectors 32-47) work unchanged
 */
#ifndef LUMAOS_APIC_H
#define LUMAOS_APIC_H

#include <stdint.h>

/* ===== LAPIC register offsets (MMIO, 32-bit accesses) ===== */
#define LAPIC_ID          0x020
#define LAPIC_VER         0x030
#define LAPIC_TPR         0x080
#define LAPIC_EOI         0x0B0
#define LAPIC_LDR         0x0D0
#define LAPIC_SVR         0x0F0
#define LAPIC_LVT_TIMER   0x320
#define LAPIC_LVT_LINT0   0x350
#define LAPIC_LVT_LINT1   0x360
#define LAPIC_LVT_ERROR   0x370
#define LAPIC_TIMER_ICR   0x380
#define LAPIC_TIMER_DCR   0x3E0

/* Spurious vector: use 0xFF (must be ≥ 0x20 in x86-64) */
#define LAPIC_SPURIOUS_VECTOR 0xFF

/* ===== I/O APIC register access (via index/data window) ===== */
#define IOAPIC_IOREGSEL   0x00
#define IOAPIO_IOWIN      0x10

/* I/O APIC register indices (written to IOREGSEL) */
#define IOAPIC_REG_ID      0x00
#define IOAPIC_REG_VER     0x01
#define IOAPIC_REG_ARB     0x02
#define IOAPIC_REG_REDTBL  0x10  /* base; 24 entries × 2 registers each */

/* I/O APIC redirection entry bits (low 32 bits) */
#define IOAPIC_REDIR_VECTOR_MASK   0x000000FF
#define IOAPIC_REDIR_DELIVERY_FIXED (0 << 8)
#define IOAPIC_REDIR_DEST_PHYSICAL  (0 << 11)
#define IOAPIC_REDIR_POLARITY_HIGH  (0 << 13)
#define IOAPIC_REDIR_TRIGGER_EDGE   (0 << 15)
#define IOAPIC_REDIR_MASKED         (1 << 16)
#define IOAPIC_REDIR_UNMASKED       (0)

/* ===== API ===== */

/* Initialize LAPIC + I/O APIC, disable PIC. Called after acpi_init(). */
void apic_init(void);

/* End-of-interrupt: LAPIC EOI if APIC active, PIC EOI otherwise. */
void apic_eoi(uint8_t irq);

/* Returns 1 if APIC is active (LAPIC enabled), 0 if using PIC. */
int apic_is_active(void);

/* Get the LAPIC ID of the current CPU. */
uint8_t lapic_get_id(void);

/* Map an ISA IRQ to its GSI (considering ACPI interrupt source overrides). */
uint32_t isa_irq_to_gsi(uint8_t isa_irq);

#endif /* LUMAOS_APIC_H */
