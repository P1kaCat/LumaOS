/* ahci.h — AHCI / SATA Controller Driver
 *
 * Phase 7c: AHCI HBA detection, port initialization and sector I/O.
 */
#ifndef LUMAOS_AHCI_H
#define LUMAOS_AHCI_H

#include <stdint.h>
#include "pci.h"

/* HBA Generic Host Control Registers */
#define AHCI_GHC_CAP     0x00
#define AHCI_GHC_GHC     0x04
#define AHCI_GHC_IS      0x08
#define AHCI_GHC_PI      0x0C
#define AHCI_GHC_VS      0x10

#define AHCI_GHC_AE      (1u << 31) /* AHCI Enable */
#define AHCI_GHC_HR      (1u << 0)  /* HBA Reset */

/* Port Registers (offset 0x100 + port * 0x80) */
#define AHCI_PORT_BASE   0x100
#define AHCI_PORT_STRIDE 0x80

#define AHCI_PxCLB       0x00
#define AHCI_PxCLBU      0x04
#define AHCI_PxFB        0x08
#define AHCI_PxFBU       0x0C
#define AHCI_PxIS        0x10
#define AHCI_PxIE        0x14
#define AHCI_PxCMD       0x18
#define AHCI_PxTFD       0x20
#define AHCI_PxSIG       0x24
#define AHCI_PxSSTS      0x28
#define AHCI_PxSCTL      0x2C
#define AHCI_PxSERR      0x30
#define AHCI_PxSACT      0x34
#define AHCI_PxCI        0x38

#define AHCI_PxCMD_ST    (1u << 0)  /* Start */
#define AHCI_PxCMD_FRE   (1u << 4)  /* FIS Receive Enable */
#define AHCI_PxCMD_FR    (1u << 14) /* FIS Receive Running */
#define AHCI_PxCMD_CR    (1u << 15) /* Command List Running */

#define AHCI_SSTS_DET_MASK   0x0F
#define AHCI_SSTS_DET_ACTIVE 0x03

#define AHCI_SIG_ATA     0x00000101
#define AHCI_SIG_ATAPI   0xEB140101
#define AHCI_SIG_SEMB    0xC33C0101
#define AHCI_SIG_PM      0x96690101

/* AHCI Command Header (32 bytes) */
struct __attribute__((packed)) ahci_cmd_header {
    uint8_t  flags;       /* CFL:5, A:1, W:1, P:1 */
    uint8_t  pm_pmp;      /* R:1, B:1, C:1, Rsvd:1, PMP:4 */
    uint16_t prdtl;       /* Physical Region Descriptor Table Length */
    uint32_t prdbc;       /* Physical Region Descriptor Byte Count */
    uint32_t ctba_lo;     /* Command Table Base Address Low */
    uint32_t ctba_hi;     /* Command Table Base Address High */
    uint32_t reserved[4];
};

/* AHCI Physical Region Descriptor Table Entry (16 bytes) */
struct __attribute__((packed)) ahci_prdt_entry {
    uint32_t dba_lo;      /* Data Base Address Low */
    uint32_t dba_hi;      /* Data Base Address High */
    uint32_t reserved;
    uint32_t dbc;         /* Byte Count:22, Rsvd:9, Interrupt on completion:1 */
};

/* Discover and initialize AHCI SATA controller */
void ahci_init(void);

#endif /* LUMAOS_AHCI_H */
