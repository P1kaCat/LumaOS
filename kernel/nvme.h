/* nvme.h — NVMe PCIe SSD Controller Driver
 *
 * Phase 7d: NVMe controller initialization, Admin Queue setup and IDENTIFY.
 */
#ifndef LUMAOS_NVME_H
#define LUMAOS_NVME_H

#include <stdint.h>
#include "pci.h"

/* NVMe Controller Register Offsets */
#define NVME_REG_CAP      0x00
#define NVME_REG_VS       0x08
#define NVME_REG_INTMS    0x0C
#define NVME_REG_INTMC    0x10
#define NVME_REG_CC       0x14
#define NVME_REG_CSTS     0x1C
#define NVME_REG_AQA      0x24
#define NVME_REG_ASQ      0x28
#define NVME_REG_ACQ      0x30

#define NVME_CC_EN        (1u << 0)
#define NVME_CC_CSS_NVM   (0u << 4)
#define NVME_CC_MPS_4K    (0u << 7)
#define NVME_CC_IOSQES_64 (6u << 16) /* 2^6 = 64 bytes */
#define NVME_CC_IOCQES_16 (4u << 20) /* 2^4 = 16 bytes */

#define NVME_CSTS_RDY     (1u << 0)
#define NVME_CSTS_CFS     (1u << 1)

#define NVME_ADMIN_IDENTIFY 0x06

/* NVMe Submission Queue Entry (64 bytes) */
struct __attribute__((packed)) nvme_sqe {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t reserved;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
};

/* NVMe Completion Queue Entry (16 bytes) */
struct __attribute__((packed)) nvme_cqe {
    uint32_t result;
    uint32_t reserved;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status; /* bit 0 = Phase / Cycle Tag */
};

/* NVMe Identify Controller Data Structure (first fields) */
struct __attribute__((packed)) nvme_id_ctrl {
    uint16_t vid;
    uint16_t ssvid;
    char     sn[20];
    char     mn[40];
    char     fr[8];
    uint8_t  rab;
    uint8_t  ieee[3];
    uint8_t  cmic;
    uint8_t  mdts;
    uint16_t cntlid;
    uint32_t ver;
};

/* Discover and initialize NVMe PCIe controller */
void nvme_init(void);

#endif /* LUMAOS_NVME_H */
