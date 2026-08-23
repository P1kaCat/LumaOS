/* net.h — Network Subsystem & TCP/IP Stack
 *
 * Phase 8: Ethernet, ARP, IPv4, ICMP, UDP and Intel e1000 Gigabit Driver.
 */
#ifndef LUMAOS_NET_H
#define LUMAOS_NET_H

#include <stdint.h>
#include "pci.h"

/* ===== Ethernet Layer ===== */
#define ETHERTYPE_IPV4      0x0800
#define ETHERTYPE_ARP       0x0806

struct __attribute__((packed)) eth_header {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
};

/* ===== ARP Layer ===== */
#define ARP_HW_ETHERNET     1
#define ARP_PROTO_IPV4      0x0800
#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

struct __attribute__((packed)) arp_header {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
};

/* ===== IPv4 Layer ===== */
#define IP_PROTO_ICMP       1
#define IP_PROTO_TCP        6
#define IP_PROTO_UDP        17

struct __attribute__((packed)) ipv4_header {
    uint8_t  ver_ihl;       /* Version (4 bits) + IHL (4 bits, usually 5 = 20 bytes) */
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
};

/* ===== ICMP Layer ===== */
#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

struct __attribute__((packed)) icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

/* ===== UDP Layer ===== */
struct __attribute__((packed)) udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
};

/* IP address representation helper (Little Endian host order) */
#define MAKE_IPV4(a,b,c,d) (((uint32_t)(a)) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/* Network Interface State */
struct net_if {
    uint8_t  mac[6];
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
};

/* ===== Intel e1000 Driver Registers & Structures ===== */
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

#define E1000_CTRL_SLU      (1u << 6)
#define E1000_STATUS_LU     (1u << 1)

#define E1000_RCTL_EN       (1u << 1)
#define E1000_RCTL_SBP      (1u << 2)
#define E1000_RCTL_UPE      (1u << 3)
#define E1000_RCTL_MPE      (1u << 4)
#define E1000_RCTL_BAM      (1u << 15)
#define E1000_RCTL_BSIZE_2K (0u << 16)
#define E1000_RCTL_SECRC    (1u << 26)

#define E1000_TCTL_EN       (1u << 1)
#define E1000_TCTL_PSP      (1u << 3)
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12

#define E1000_TXD_CMD_EOP   (1u << 0)
#define E1000_TXD_CMD_IFCS  (1u << 1)
#define E1000_TXD_CMD_RS    (1u << 3)
#define E1000_TXD_STAT_DD   (1u << 0)

#define E1000_RXD_STAT_DD   (1u << 0)
#define E1000_RXD_STAT_EOP  (1u << 1)

#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   16

struct __attribute__((packed)) e1000_rx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
};

struct __attribute__((packed)) e1000_tx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
};

/* Hardware Driver API */
void e1000_init(void);
void e1000_get_mac(uint8_t *mac_out);
int  e1000_send_packet(const void *data, uint16_t len);
int  e1000_recv_packet(void *buf, uint16_t max_len);

/* TCP/IP Stack API */
void net_init(void);
void net_poll(void);
int  arp_resolve(uint32_t ip, uint8_t *mac_out);
int  ipv4_send(uint32_t dst_ip, uint8_t protocol, const void *payload, uint16_t len);
int  icmp_ping(uint32_t dst_ip, uint16_t seq);
int  udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void *data, uint16_t len);
struct net_if *net_get_interface(void);

#endif /* LUMAOS_NET_H */
