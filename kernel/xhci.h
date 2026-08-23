/* xhci.h — xHCI USB Host Controller Driver
 *
 * Phase 7b.1: controller discovery and capability reading.
 * Phase 7b.2: controller reset and command/event ring setup.
 */
#ifndef LUMAOS_XHCI_H
#define LUMAOS_XHCI_H

#include <stdint.h>
#include "pci.h"

/* ===== xHCI Capability Registers ===== */
#define XHCI_CAPLENGTH    0x00
#define XHCI_HCIVERSION   0x02
#define XHCI_HCSPARAMS1   0x04
#define XHCI_HCSPARAMS2   0x08
#define XHCI_HCSPARAMS3   0x0C
#define XHCI_HCCPARAMS1   0x10
#define XHCI_DBOFF        0x14
#define XHCI_RTSOFF       0x18

#define XHCI_HCS1_MAX_SLOTS     0xFF000000
#define XHCI_HCS1_MAX_INTRS     0x00FF0000
#define XHCI_HCS1_MAX_PORTS     0x00000FFF

#define XHCI_HCC1_AC64          (1u << 0)
#define XHCI_HCC1_BNC           (1u << 1)
#define XHCI_HCC1_CSZ           (1u << 2)
#define XHCI_HCC1_PPC           (1u << 3)
#define XHCI_HCC1_PIND          (1u << 4)
#define XHCI_HCC1_LHRC          (1u << 5)
#define XHCI_HCC1_LTC           (1u << 6)
#define XHCI_HCC1_NSS           (1u << 7)

/* ===== xHCI Operational Registers ===== */
#define XHCI_USBCMD        0x00
#define XHCI_USBSTS        0x04
#define XHCI_PAGESIZE      0x08
#define XHCI_DNCTRL        0x14
#define XHCI_CRCR_LOW      0x18
#define XHCI_CRCR_HIGH     0x1C
#define XHCI_DCBAAP_LOW    0x30
#define XHCI_DCBAAP_HIGH   0x34
#define XHCI_CONFIG        0x38

#define XHCI_USBCMD_RUN    (1u << 0)
#define XHCI_USBCMD_RESET  (1u << 1)

#define XHCI_USBSTS_HCH    (1u << 0)
#define XHCI_USBSTS_PCD    (1u << 4)
#define XHCI_USBSTS_CNR    (1u << 11)
#define XHCI_USBSTS_HCE    (1u << 12)

/* ===== Runtime register space ===== */
#define XHCI_RT_INTR_BASE      0x20
#define XHCI_RT_INTR_STRIDE    0x20
#define XHCI_IMAN              0x00
#define XHCI_IMOD              0x04
#define XHCI_ERSTSZ            0x08
#define XHCI_ERSTBA_LOW        0x10
#define XHCI_ERSTBA_HIGH       0x14
#define XHCI_ERDP_LOW          0x18
#define XHCI_ERDP_HIGH         0x1C
#define XHCI_IMAN_IE           (1u << 1)
#define XHCI_IMAN_IP           (1u << 0)
#define XHCI_ERDP_EHB          (1u << 3)

/* ===== Doorbell registers ===== */
#define XHCI_DOORBELL_COMMAND  0

/* ===== Port Register Space ===== */
#define XHCI_PORT_BASE         0x400
#define XHCI_PORT_STRIDE       0x10
#define XHCI_PORTSC            0x00
#define XHCI_PORTSC_CCS        (1u << 0)
#define XHCI_PORTSC_PED        (1u << 1)
#define XHCI_PORTSC_OCA        (1u << 3)
#define XHCI_PORTSC_PR         (1u << 4)
#define XHCI_PORTSC_PLS_MASK   (0x0Fu << 5)
#define XHCI_PORTSC_PP         (1u << 9)
#define XHCI_PORTSC_SPEED_SHIFT 10
#define XHCI_PORTSC_SPEED_MASK  (0x0Fu << 10)
#define XHCI_PORTSC_CSC        (1u << 17)
#define XHCI_PORTSC_PEC        (1u << 18)
#define XHCI_PORTSC_WRC        (1u << 19)
#define XHCI_PORTSC_OCC        (1u << 20)
#define XHCI_PORTSC_PRC        (1u << 21)
#define XHCI_PORTSC_PLC        (1u << 22)
#define XHCI_PORTSC_CEC        (1u << 23)
#define XHCI_PORTSC_RW1C_MASK  (0x00FE0000u)

/* Port Speeds */
#define XHCI_SPEED_FULL        1
#define XHCI_SPEED_LOW         2
#define XHCI_SPEED_HIGH        3
#define XHCI_SPEED_SUPER       4
#define XHCI_SPEED_SUPER_PLUS  5

/* ===== TRB definitions ===== */
#define XHCI_TRB_TYPE_SHIFT    10
#define XHCI_TRB_TYPE_MASK     (0x3Fu << XHCI_TRB_TYPE_SHIFT)
#define XHCI_TRB_CYCLE         (1u << 0)
#define XHCI_TRB_TC            (1u << 1) /* Link TRB: toggle cycle */

#define XHCI_TRB_TYPE_NORMAL               1
#define XHCI_TRB_TYPE_SETUP_STAGE          2
#define XHCI_TRB_TYPE_DATA_STAGE           3
#define XHCI_TRB_TYPE_STATUS_STAGE         4
#define XHCI_TRB_TYPE_LINK                 6
#define XHCI_TRB_TYPE_ENABLE_SLOT          9
#define XHCI_TRB_TYPE_DISABLE_SLOT         10
#define XHCI_TRB_TYPE_ADDRESS_DEVICE       11
#define XHCI_TRB_TYPE_CONFIG_ENDPOINT      12
#define XHCI_TRB_TYPE_CMD_NOOP             23
#define XHCI_TRB_TYPE_TRANSFER_EVENT       32
#define XHCI_TRB_TYPE_CMD_COMPLETION       33
#define XHCI_TRB_TYPE_PORT_STATUS_CHANGE   34

/* Completion codes */
#define XHCI_COMP_SUCCESS      1

struct xhci_trb {
    uint32_t parameter_lo;
    uint32_t parameter_hi;
    uint32_t status;
    uint32_t control;
};

struct xhci_erst_entry {
    uint32_t ring_seg_base_lo;
    uint32_t ring_seg_base_hi;
    uint32_t ring_seg_size;
    uint32_t reserved;
};

/* Send a command on the command ring and wait for completion event */
int xhci_send_command(struct xhci_trb *cmd, struct xhci_trb *event_out);

/* Enable a device slot (returns allocated slot ID) */
int xhci_enable_slot(uint32_t *slot_id_out);

/* Probe and initialize connected USB ports */
void xhci_probe_ports(void);

/* Discover, reset and initialize the xHCI host controller. */
void xhci_init(void);

#endif /* LUMAOS_XHCI_H */
