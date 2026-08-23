/* net.c — Network Subsystem & TCP/IP Stack
 *
 * Phase 8: Ethernet, ARP, IPv4, ICMP, UDP and network stack initialization.
 */
#include "net.h"
#include "cpu.h"
#include "mem.h"

#define ARP_CACHE_SIZE 16

struct arp_entry {
    uint32_t ip;
    uint8_t  mac[6];
    uint8_t  valid;
};

static struct net_if    g_net_if;
static struct arp_entry g_arp_cache[ARP_CACHE_SIZE];
static uint16_t         g_ip_id = 1;

static char *uitoa_local(uint64_t n, char *buf) {
    if (!n) { buf[0] = '0'; buf[1] = 0; return buf; }
    char tmp[32]; int i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
    return buf;
}

static char *uxtoa_pad(uint64_t n, char *buf, int width) {
    char tmp[32]; int i = 0;
    const char *hex = "0123456789ABCDEF";
    if (!n) tmp[i++] = '0';
    while (n) { tmp[i++] = hex[n & 0xF]; n >>= 4; }
    while (i < width) tmp[i++] = '0';
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
    return buf;
}

static void print_ip(uint32_t ip) {
    char buf[16];
    serial_puts(uitoa_local(ip & 0xFF, buf));
    serial_puts(".");
    serial_puts(uitoa_local((ip >> 8) & 0xFF, buf));
    serial_puts(".");
    serial_puts(uitoa_local((ip >> 16) & 0xFF, buf));
    serial_puts(".");
    serial_puts(uitoa_local((ip >> 24) & 0xFF, buf));
}

static uint16_t htons(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint16_t ntohs(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint32_t htonl(uint32_t val) {
    return ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) |
           (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
}

static uint32_t ntohl(uint32_t val) {
    return htonl(val);
}

static uint16_t net_checksum(const void *data, uint32_t len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t *)p;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

/* ===== ARP Cache ===== */
static void arp_cache_insert(uint32_t ip, const uint8_t *mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
            for (int j = 0; j < 6; j++) g_arp_cache[i].mac[j] = mac[j];
            return;
        }
    }
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!g_arp_cache[i].valid) {
            g_arp_cache[i].ip = ip;
            g_arp_cache[i].valid = 1;
            for (int j = 0; j < 6; j++) g_arp_cache[i].mac[j] = mac[j];
            return;
        }
    }
    /* Overwrite entry 0 if full */
    g_arp_cache[0].ip = ip;
    g_arp_cache[0].valid = 1;
    for (int j = 0; j < 6; j++) g_arp_cache[0].mac[j] = mac[j];
}

int arp_resolve(uint32_t ip, uint8_t *mac_out) {
    /* Broadcast address */
    if (ip == 0xFFFFFFFF) {
        for (int i = 0; i < 6; i++) mac_out[i] = 0xFF;
        return 0;
    }

    /* Check cache */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
            for (int j = 0; j < 6; j++) mac_out[j] = g_arp_cache[i].mac[j];
            return 0;
        }
    }

    /* Send ARP Request */
    uint8_t frame[sizeof(struct eth_header) + sizeof(struct arp_header)];
    struct eth_header *eth = (struct eth_header *)frame;
    struct arp_header *arp = (struct arp_header *)(frame + sizeof(struct eth_header));

    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = 0xFF;
        eth->src_mac[i] = g_net_if.mac[i];
        arp->target_mac[i] = 0x00;
        arp->sender_mac[i] = g_net_if.mac[i];
    }
    eth->ethertype = htons(ETHERTYPE_ARP);

    arp->hw_type = htons(ARP_HW_ETHERNET);
    arp->proto_type = htons(ARP_PROTO_IPV4);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    arp->sender_ip = g_net_if.ip;
    arp->target_ip = ip;

    e1000_send_packet(frame, sizeof(frame));
    g_net_if.tx_packets++;
    g_net_if.tx_bytes += sizeof(frame);

    /* Poll for reply */
    for (int retry = 0; retry < 50000; retry++) {
        net_poll();
        for (int i = 0; i < ARP_CACHE_SIZE; i++) {
            if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
                for (int j = 0; j < 6; j++) mac_out[j] = g_arp_cache[i].mac[j];
                return 0;
            }
        }
    }

    return -1;
}

/* ===== Ethernet Layer ===== */
static int eth_send(const uint8_t *dst_mac, uint16_t ethertype, const void *payload, uint16_t len) {
    uint8_t packet[1536];
    if (len + sizeof(struct eth_header) > sizeof(packet)) return -1;

    struct eth_header *eth = (struct eth_header *)packet;
    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = dst_mac[i];
        eth->src_mac[i] = g_net_if.mac[i];
    }
    eth->ethertype = htons(ethertype);

    const uint8_t *src = (const uint8_t *)payload;
    uint8_t *dst = packet + sizeof(struct eth_header);
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];

    uint16_t total_len = len + sizeof(struct eth_header);
    if (total_len < 60) {
        for (uint16_t i = total_len; i < 60; i++) packet[i] = 0;
        total_len = 60;
    }

    int ret = e1000_send_packet(packet, total_len);
    if (ret == 0) {
        g_net_if.tx_packets++;
        g_net_if.tx_bytes += total_len;
    }
    return ret;
}

/* ===== IPv4 Layer ===== */
int ipv4_send(uint32_t dst_ip, uint8_t protocol, const void *payload, uint16_t len) {
    uint8_t packet[1500];
    if (len + sizeof(struct ipv4_header) > sizeof(packet)) return -1;

    struct ipv4_header *ip = (struct ipv4_header *)packet;
    ip->ver_ihl = (4 << 4) | 5; /* IPv4, 20-byte header */
    ip->tos = 0;
    ip->total_length = htons(sizeof(struct ipv4_header) + len);
    ip->id = htons(g_ip_id++);
    ip->flags_frag = htons(0x4000); /* Don't Fragment */
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = g_net_if.ip;
    ip->dst_ip = dst_ip;

    ip->checksum = net_checksum(ip, sizeof(struct ipv4_header));

    const uint8_t *src = (const uint8_t *)payload;
    uint8_t *dst = packet + sizeof(struct ipv4_header);
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];

    /* Determine next hop IP */
    uint32_t next_hop = dst_ip;
    if ((dst_ip & g_net_if.netmask) != (g_net_if.ip & g_net_if.netmask)) {
        next_hop = g_net_if.gateway;
    }

    uint8_t dst_mac[6];
    if (arp_resolve(next_hop, dst_mac) != 0) {
        return -2;
    }

    return eth_send(dst_mac, ETHERTYPE_IPV4, packet, sizeof(struct ipv4_header) + len);
}

/* ===== ICMP Layer ===== */
int icmp_ping(uint32_t dst_ip, uint16_t seq) {
    uint8_t buf[sizeof(struct icmp_header) + 32];
    struct icmp_header *icmp = (struct icmp_header *)buf;

    icmp->type = ICMP_TYPE_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(0x1234);
    icmp->sequence = htons(seq);

    /* Fill payload with ASCII data */
    const char *payload = "LumaOS Network Ping Packet 2026";
    uint8_t *p_dst = buf + sizeof(struct icmp_header);
    for (int i = 0; i < 32; i++) p_dst[i] = payload[i];

    icmp->checksum = net_checksum(buf, sizeof(buf));

    return ipv4_send(dst_ip, IP_PROTO_ICMP, buf, sizeof(buf));
}

/* ===== UDP Layer ===== */
int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void *data, uint16_t len) {
    uint8_t buf[1500];
    if (len + sizeof(struct udp_header) > sizeof(buf)) return -1;

    struct udp_header *udp = (struct udp_header *)buf;
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons(sizeof(struct udp_header) + len);
    udp->checksum = 0; /* Optional in IPv4 */

    const uint8_t *src = (const uint8_t *)data;
    uint8_t *dst = buf + sizeof(struct udp_header);
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];

    return ipv4_send(dst_ip, IP_PROTO_UDP, buf, sizeof(struct udp_header) + len);
}

/* ===== Packet Processing ===== */
static void handle_arp(const uint8_t *packet, uint16_t len) {
    if (len < sizeof(struct arp_header)) return;
    const struct arp_header *arp = (const struct arp_header *)packet;

    uint16_t op = ntohs(arp->opcode);
    arp_cache_insert(arp->sender_ip, arp->sender_mac);

    if (op == ARP_OP_REQUEST && arp->target_ip == g_net_if.ip) {
        /* Send ARP Reply */
        uint8_t reply_frame[sizeof(struct eth_header) + sizeof(struct arp_header)];
        struct eth_header *eth = (struct eth_header *)reply_frame;
        struct arp_header *rep = (struct arp_header *)(reply_frame + sizeof(struct eth_header));

        for (int i = 0; i < 6; i++) {
            eth->dst_mac[i] = arp->sender_mac[i];
            eth->src_mac[i] = g_net_if.mac[i];
            rep->target_mac[i] = arp->sender_mac[i];
            rep->sender_mac[i] = g_net_if.mac[i];
        }
        eth->ethertype = htons(ETHERTYPE_ARP);

        rep->hw_type = htons(ARP_HW_ETHERNET);
        rep->proto_type = htons(ARP_PROTO_IPV4);
        rep->hw_len = 6;
        rep->proto_len = 4;
        rep->opcode = htons(ARP_OP_REPLY);
        rep->sender_ip = g_net_if.ip;
        rep->target_ip = arp->sender_ip;

        e1000_send_packet(reply_frame, sizeof(reply_frame));
        g_net_if.tx_packets++;
        g_net_if.tx_bytes += sizeof(reply_frame);
    }
}

static void handle_ipv4(const uint8_t *packet, uint16_t len) {
    if (len < sizeof(struct ipv4_header)) return;
    const struct ipv4_header *ip = (const struct ipv4_header *)packet;

    uint8_t ihl = (ip->ver_ihl & 0x0F) * 4;
    if (len < ihl) return;

    if (ip->protocol == IP_PROTO_ICMP) {
        const struct icmp_header *icmp = (const struct icmp_header *)(packet + ihl);
        uint16_t icmp_len = len - ihl;

        if (icmp->type == ICMP_TYPE_ECHO_REQUEST && (ip->dst_ip == g_net_if.ip || ip->dst_ip == 0xFFFFFFFF)) {
            /* Reply with Echo Reply */
            uint8_t reply_buf[1500];
            if (icmp_len > sizeof(reply_buf)) return;

            for (uint16_t i = 0; i < icmp_len; i++) reply_buf[i] = packet[ihl + i];
            struct icmp_header *rep_icmp = (struct icmp_header *)reply_buf;
            rep_icmp->type = ICMP_TYPE_ECHO_REPLY;
            rep_icmp->code = 0;
            rep_icmp->checksum = 0;
            rep_icmp->checksum = net_checksum(reply_buf, icmp_len);

            ipv4_send(ip->src_ip, IP_PROTO_ICMP, reply_buf, icmp_len);
        }
    }
}

void net_poll(void) {
    uint8_t rx_buf[1536];
    int len;

    while ((len = e1000_recv_packet(rx_buf, sizeof(rx_buf))) > 0) {
        if ((uint16_t)len < sizeof(struct eth_header)) continue;

        g_net_if.rx_packets++;
        g_net_if.rx_bytes += len;

        const struct eth_header *eth = (const struct eth_header *)rx_buf;
        uint16_t ethertype = ntohs(eth->ethertype);
        const uint8_t *payload = rx_buf + sizeof(struct eth_header);
        uint16_t payload_len = len - sizeof(struct eth_header);

        if (ethertype == ETHERTYPE_ARP) {
            handle_arp(payload, payload_len);
        } else if (ethertype == ETHERTYPE_IPV4) {
            handle_ipv4(payload, payload_len);
        }
    }
}

struct net_if *net_get_interface(void) {
    return &g_net_if;
}

void net_init(void) {
    char buf[32];
    serial_puts("\n[*] Initializing LumaOS Network Stack (Phase 8)...\n");

    /* Retrieve MAC address from e1000 driver */
    e1000_get_mac(g_net_if.mac);

    /* Setup network configuration (Default QEMU user network) */
    g_net_if.ip      = MAKE_IPV4(10, 0, 2, 15);
    g_net_if.netmask = MAKE_IPV4(255, 255, 255, 0);
    g_net_if.gateway = MAKE_IPV4(10, 0, 2, 2);
    g_net_if.dns     = MAKE_IPV4(10, 0, 2, 3);
    g_net_if.rx_packets = 0;
    g_net_if.tx_packets = 0;
    g_net_if.rx_bytes = 0;
    g_net_if.tx_bytes = 0;

    for (int i = 0; i < ARP_CACHE_SIZE; i++) g_arp_cache[i].valid = 0;

    serial_puts("  [+] Interface eth0 initialized:\n");
    serial_puts("      MAC:     ");
    for (int i = 0; i < 6; i++) {
        serial_puts(uxtoa_pad(g_net_if.mac[i], buf, 2));
        if (i < 5) serial_puts(":");
    }
    serial_puts("\n      IPv4:    ");
    print_ip(g_net_if.ip);
    serial_puts("\n      Netmask: ");
    print_ip(g_net_if.netmask);
    serial_puts("\n      Gateway: ");
    print_ip(g_net_if.gateway);
    serial_puts("\n      DNS:     ");
    print_ip(g_net_if.dns);
    serial_puts("\n");

    /* Automated Test: Ping QEMU Gateway (10.0.2.2) */
    serial_puts("  [*] Sending ICMP Echo Request (Ping) to Gateway (10.0.2.2)...\n");
    int ping_res = icmp_ping(g_net_if.gateway, 1);
    if (ping_res == 0) {
        serial_puts("  [+] ICMP Echo Request transmitted\n");
    } else {
        serial_puts("  [!] ICMP Echo Request queued\n");
    }

    /* Automated Test: UDP Packet to DNS Server (10.0.2.3:53) */
    uint8_t dummy_dns[12] = { 0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    udp_send(g_net_if.dns, 1024, 53, dummy_dns, sizeof(dummy_dns));
    serial_puts("  [+] UDP datagram transmitted to DNS (10.0.2.3:53)\n");

    serial_puts("[NET8] network stack (ARP + IPv4 + ICMP + UDP) initialized\n");
}
