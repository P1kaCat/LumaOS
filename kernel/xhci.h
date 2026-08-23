/* xhci.h — xHCI USB Host Controller Driver (Phase 7b.1)
 *
 * xHCI (eXtensible Host Controller Interface) for USB 3.0/2.0.
 * Discovered via PCI class 0C/03/30 (Serial bus / USB / xHCI).
 * Registers accessed via MMIO (BAR0).
 *
 * Phase 7b.1: Controller discovery + capability register reading.
 * Future phases: controller reset, command/event rings, device enumeration,
 * USB HID keyboard/mouse support.
 */
#ifndef LUMAOS_XHCI_H
#define LUMAOS_XHCI_H

#include <stdint.h>
#include "pci.h"

/* ===== xHCI Capability Registers (read-only, at BAR0 base) ===== */
#define XHCI_CAPLENGTH    0x00  /* 8-bit: length of capability regs */
#define XHCI_HCIVERSION   0x02  /* 16-bit: interface version (BCD) */
#define XHCI_HCSPARAMS1   0x04  /* 32-bit: structural parameters 1 */
#define XHCI_HCSPARAMS2   0x08  /* 32-bit: structural parameters 2 */
#define XHCI_HCSPARAMS3   0x0C  /* 32-bit: structural parameters 3 */
#define XHCI_HCCPARAMS1   0x10  /* 32-bit: capability parameters 1 */
#define XHCI_DBOFF        0x14  /* 32-bit: doorbell offset */
#define XHCI_RTSOFF       0x18  /* 32-bit: runtime register space offset */

/* HCSPARAMS1 fields */
#define XHCI_HCS1_MAX_SLOTS     0xFF000000  /* bits 24-31 */
#define XHCI_HCS1_MAX_INTRS     0x00FF0000  /* bits 16-23 */
#define XHCI_HCS1_MAX_PORTS     0x00000FFF  /* bits 0-11 */

/* HCCPARAMS1 fields */
#define XHCI_HCC1_AC64          (1 << 0)    /* 64-bit addressing */
#define XHCI_HCC1_BNC           (1 << 1)    /* BW negotiation */
#define XHCI_HCC1_CSZ           (1 << 2)    /* 64-byte context size */
#define XHCI_HCC1_PPC           (1 << 3)    /* port power control */
#define XHCI_HCC1_PIND          (1 << 4)    /* port indicators */
#define XHCI_HCC1_LHRC          (1 << 5)    /* light HC reset */
#define XHCI_HCC1_LTC           (1 << 6)    /* latency tolerance msg */
#define XHCI_HCC1_NSS           (1 << 7)    /* no secondary SID */
#define XHCI_HCC1_SEC_SID(x)   (((x) >> 24) & 0xFF)
#define XHCI_HCC1_PSI(x)       (((x) >> 28) & 0x0F)

/* ===== xHCI Operational Registers (at BAR0 + CAPLENGTH) ===== */
#define XHCI_USBCMD        0x00  /* 32-bit: USB command */
#define XHCI_USBSTS        0x04  /* 32-bit: USB status */
#define XHCI_PAGESIZE      0x08  /* 32-bit: page size */
#define XHCI_DNCTRL       0x14  /* 32-bit: device notification control */
#define XHCI_CRCR_LOW     0x18  /* 64-bit: command ring control */
#define XHCI_DCBAAP_LOW   0x30  /* 64-bit: dev context base addr array ptr */
#define XHCI_CONFIG       0x38  /* 32-bit: configure */

/* USBCMD bits */
#define XHCI_USBCMD_RUN    (1 << 0)
#define XHCI_USBCMD_RESET  (1 << 1)

/* USBSTS bits */
#define XHCI_USBSTS_HCH    (1 << 0)  /* HC halted */
#define XHCI_USBSTS_PCD    (1 << 4)  /* port change detect */
#define XHCI_USBSTS_CNR    (1 << 11) /* controller not ready */

/* ===== API ===== */

/* Discover and initialize the xHCI host controller.
 * Finds the xHCI PCI device, enables it, reads BAR0,
 * and reads capability registers.
 * Called after pci_init() and apic_init().
 */
void xhci_init(void);

#endif /* LUMAOS_XHCI_H */
