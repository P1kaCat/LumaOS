/* e1000.c — Intel 82540EM Gigabit Ethernet Network Driver
 *
 * Phase 8: Network subsystem & Intel 82540EM (e1000) driver.
 */
#include "net.h"
#include "cpu.h"
#include "mem.h"

static uint64_t g_e1000_mmio;
static uint8_t  g_e1000_mac[6];
static uint64_t g_e1000_rx_ring;
static uint64_t g_e1000_tx_ring;
static uint64_t g_e1000_rx_bufs[E1000_NUM_RX_DESC];
static uint64_t g_e1000_tx_bufs[E1000_NUM_TX_DESC];
static uint16_t g_e1000_rx_cur = 0;
static uint16_t g_e1000_tx_cur = 0;

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

static uint32_t e1000_read32(uint32_t reg) {
    return *(volatile uint32_t *)(unsigned long)(g_e1000_mmio + reg);
}

static void e1000_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(unsigned long)(g_e1000_mmio + reg) = val;
}

static uint16_t e1000_read_eeprom(uint8_t addr) {
    e1000_write32(E1000_REG_EERD, (1u << 0) | ((uint32_t)addr << 8));
    for (uint32_t i = 0; i < 100000; i++) {
        uint32_t val = e1000_read32(E1000_REG_EERD);
        if (val & (1u << 4)) {
            return (uint16_t)((val >> 16) & 0xFFFF);
        }
    }
    return 0;
}

void e1000_init(void) {
    char buf[32];
    serial_puts("\n[*] Initializing Intel e1000 Gigabit Ethernet controller...\n");

    struct pci_device *net_dev = pci_find_device(0x8086, 0x100E);
    if (!net_dev) net_dev = pci_find_device(0x8086, 0x100F);
    if (!net_dev) net_dev = pci_find_device(0x8086, 0x10D3);
    if (!net_dev) net_dev = pci_find_class(0x02, 0x00, 0x00);

    if (!net_dev) {
        serial_puts("  [!] No Intel e1000 network controller found\n");
        return;
    }

    serial_puts("  [+] e1000 controller found at ");
    serial_puts(uxtoa_pad(net_dev->bus, buf, 2)); serial_puts(":");
    serial_puts(uxtoa_pad(net_dev->device, buf, 2)); serial_puts(".");
    serial_puts(uitoa_local(net_dev->func, buf));
    serial_puts(" vendor=");
    serial_puts(uxtoa_pad(net_dev->vendor, buf, 4));
    serial_puts(" device=");
    serial_puts(uxtoa_pad(net_dev->device_id, buf, 4));
    serial_puts("\n");

    uint32_t bar0_raw = net_dev->bars[0];
    if (!bar0_raw) {
        serial_puts("  [!] e1000 BAR0 is zero\n");
        return;
    }

    uint64_t mmio = (uint64_t)(bar0_raw & 0xFFFFFFF0u);
    g_e1000_mmio = mmio;
    pci_enable_device(net_dev);

    /* Map MMIO pages if above 4GB */
    if (mmio >= 0x100000000ULL) {
        uint64_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        cr3 &= ~0xFFFULL;
        for (uint64_t i = 0; i < 32; i++) {
            map_page(cr3, mmio + i * PAGE_SIZE, mmio + i * PAGE_SIZE, PTE_PRESENT | PTE_WRITABLE);
        }
    }

    /* Set Link Up */
    uint32_t ctrl = e1000_read32(E1000_REG_CTRL);
    ctrl |= E1000_CTRL_SLU;
    e1000_write32(E1000_REG_CTRL, ctrl);

    /* Read MAC address from RAL0/RAH0 or EEPROM */
    uint32_t ral = e1000_read32(E1000_REG_RAL0);
    uint32_t rah = e1000_read32(E1000_REG_RAH0);

    if (ral != 0) {
        g_e1000_mac[0] = (uint8_t)(ral & 0xFF);
        g_e1000_mac[1] = (uint8_t)((ral >> 8) & 0xFF);
        g_e1000_mac[2] = (uint8_t)((ral >> 16) & 0xFF);
        g_e1000_mac[3] = (uint8_t)((ral >> 24) & 0xFF);
        g_e1000_mac[4] = (uint8_t)(rah & 0xFF);
        g_e1000_mac[5] = (uint8_t)((rah >> 8) & 0xFF);
    } else {
        uint16_t w0 = e1000_read_eeprom(0);
        uint16_t w1 = e1000_read_eeprom(1);
        uint16_t w2 = e1000_read_eeprom(2);
        g_e1000_mac[0] = (uint8_t)(w0 & 0xFF);
        g_e1000_mac[1] = (uint8_t)(w0 >> 8);
        g_e1000_mac[2] = (uint8_t)(w1 & 0xFF);
        g_e1000_mac[3] = (uint8_t)(w1 >> 8);
        g_e1000_mac[4] = (uint8_t)(w2 & 0xFF);
        g_e1000_mac[5] = (uint8_t)(w2 >> 8);
    }

    serial_puts("  [+] MAC Address: ");
    for (int i = 0; i < 6; i++) {
        serial_puts(uxtoa_pad(g_e1000_mac[i], buf, 2));
        if (i < 5) serial_puts(":");
    }
    serial_puts("\n");

    /* Allocate RX Ring and Buffers */
    g_e1000_rx_ring = alloc_page();
    if (g_e1000_rx_ring) {
        struct e1000_rx_desc *rx = (struct e1000_rx_desc *)(unsigned long)g_e1000_rx_ring;
        for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
            g_e1000_rx_bufs[i] = alloc_page();
            rx[i].buffer_addr = g_e1000_rx_bufs[i];
            rx[i].status = 0;
        }

        e1000_write32(E1000_REG_RDBAL, (uint32_t)g_e1000_rx_ring);
        e1000_write32(E1000_REG_RDBAH, (uint32_t)(g_e1000_rx_ring >> 32));
        e1000_write32(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
        e1000_write32(E1000_REG_RDH, 0);
        e1000_write32(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);

        uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2K | E1000_RCTL_SECRC;
        e1000_write32(E1000_REG_RCTL, rctl);
    }

    /* Allocate TX Ring and Buffers */
    g_e1000_tx_ring = alloc_page();
    if (g_e1000_tx_ring) {
        struct e1000_tx_desc *tx = (struct e1000_tx_desc *)(unsigned long)g_e1000_tx_ring;
        for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
            g_e1000_tx_bufs[i] = alloc_page();
            tx[i].buffer_addr = g_e1000_tx_bufs[i];
            tx[i].cmd = 0;
            tx[i].status = E1000_TXD_STAT_DD;
        }

        e1000_write32(E1000_REG_TDBAL, (uint32_t)g_e1000_tx_ring);
        e1000_write32(E1000_REG_TDBAH, (uint32_t)(g_e1000_tx_ring >> 32));
        e1000_write32(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
        e1000_write32(E1000_REG_TDH, 0);
        e1000_write32(E1000_REG_TDT, 0);

        uint32_t tctl = E1000_TCTL_EN | E1000_TCTL_PSP |
                        (0x10u << E1000_TCTL_CT_SHIFT) | (0x40u << E1000_TCTL_COLD_SHIFT);
        e1000_write32(E1000_REG_TCTL, tctl);
    }

    /* Test transmit: send test packet */
    uint8_t test_pkt[64];
    for (int i = 0; i < 6; i++) test_pkt[i] = 0xFF; /* Broadcast */
    for (int i = 0; i < 6; i++) test_pkt[6 + i] = g_e1000_mac[i];
    test_pkt[12] = 0x08; test_pkt[13] = 0x06; /* ARP */
    for (int i = 14; i < 64; i++) test_pkt[i] = 0;

    e1000_send_packet(test_pkt, sizeof(test_pkt));
    serial_puts("  [+] Test Ethernet packet transmitted\n");

    serial_puts("[E1000-8] ethernet controller initialized + MAC read\n");
}

int e1000_send_packet(const void *data, uint16_t len) {
    if (!g_e1000_tx_ring || !data || !len) return -1;

    struct e1000_tx_desc *tx = (struct e1000_tx_desc *)(unsigned long)g_e1000_tx_ring;
    uint16_t cur = g_e1000_tx_cur;

    uint8_t *dst = (uint8_t *)(unsigned long)g_e1000_tx_bufs[cur];
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];

    tx[cur].length = len;
    tx[cur].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    tx[cur].status = 0;

    g_e1000_tx_cur = (cur + 1) % E1000_NUM_TX_DESC;
    e1000_write32(E1000_REG_TDT, g_e1000_tx_cur);

    return 0;
}

int e1000_recv_packet(void *buf, uint16_t max_len) {
    if (!g_e1000_rx_ring || !buf) return -1;

    struct e1000_rx_desc *rx = (struct e1000_rx_desc *)(unsigned long)g_e1000_rx_ring;
    uint16_t cur = g_e1000_rx_cur;

    if (!(rx[cur].status & E1000_RXD_STAT_DD)) return 0; /* No packet */

    uint16_t len = rx[cur].length;
    if (len > max_len) len = max_len;

    const uint8_t *src = (const uint8_t *)(unsigned long)g_e1000_rx_bufs[cur];
    uint8_t *dst = (uint8_t *)buf;
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];

    rx[cur].status = 0;
    g_e1000_rx_cur = (cur + 1) % E1000_NUM_RX_DESC;
    e1000_write32(E1000_REG_RDT, cur);

    return (int)len;
}
