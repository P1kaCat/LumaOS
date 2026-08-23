/* net.h — Network & Intel e1000 Gigabit Ethernet Driver
 *
 * Phase 8: Network subsystem & Intel 82540EM (e1000) driver.
 */
#ifndef LUMAOS_NET_H
#define LUMAOS_NET_H

#include <stdint.h>
#include "pci.h"

/* e1000 Register Offsets */
#define E1000_REG_CTRL      0x0000
#define E1000_REG_STATUS    0x0008
#define E1000_REG_EERD      0x0014
#define E1000_REG_ICR       0x00C0
#define E1000_REG_IMS       0x00D0
#define E1000_REG_RCTL      0x0100
#define E1000_REG_TCTL      0x0400
#define E1000_REG_RDBAL     0x2800
#define E1000_REG_RDBAH     0x2804
#define E1000_REG_RDLEN     0x2808
#define E1000_REG_RDH       0x2810
#define E1000_REG_RDT       0x2818
#define E1000_REG_TDBAL     0x3800
#define E1000_REG_TDBAH     0x3804
#define E1000_REG_TDLEN     0x3808
#define E1000_REG_TDH       0x3810
#define E1000_REG_TDT       0x3818
#define E1000_REG_RAL0      0x5400
#define E1000_REG_RAH0      0x5404

/* Control & Status Bits */
#define E1000_CTRL_SLU      (1u << 6)  /* Set Link Up */
#define E1000_STATUS_LU     (1u << 1)  /* Link Up */

#define E1000_RCTL_EN       (1u << 1)  /* Receiver Enable */
#define E1000_RCTL_SBP      (1u << 2)  /* Store Bad Packets */
#define E1000_RCTL_UPE      (1u << 3)  /* Unicast Promiscuous */
#define E1000_RCTL_MPE      (1u << 4)  /* Multicast Promiscuous */
#define E1000_RCTL_BAM      (1u << 15) /* Broadcast Accept */
#define E1000_RCTL_BSIZE_2K (0u << 16) /* 2048 byte RX buffers */
#define E1000_RCTL_SECRC    (1u << 26) /* Strip Ethernet CRC */

#define E1000_TCTL_EN       (1u << 1)  /* Transmitter Enable */
#define E1000_TCTL_PSP      (1u << 3)  /* Pad Short Packets */
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12

#define E1000_TXD_CMD_EOP   (1u << 0)  /* End of Packet */
#define E1000_TXD_CMD_IFCS  (1u << 1)  /* Insert FCS/CRC */
#define E1000_TXD_CMD_RS    (1u << 3)  /* Report Status */
#define E1000_TXD_STAT_DD   (1u << 0)  /* Descriptor Done */

#define E1000_RXD_STAT_DD   (1u << 0)  /* Descriptor Done */
#define E1000_RXD_STAT_EOP  (1u << 1)  /* End of Packet */

/* Number of descriptors in rings */
#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   16

/* e1000 RX Descriptor (16 bytes) */
struct __attribute__((packed)) e1000_rx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
};

/* e1000 TX Descriptor (16 bytes) */
struct __attribute__((packed)) e1000_tx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
};

/* Public API */
void e1000_init(void);
int e1000_send_packet(const void *data, uint16_t len);
int e1000_recv_packet(void *buf, uint16_t max_len);

#endif /* LUMAOS_NET_H */
