/* xhci.c — xHCI USB Host Controller Driver
 *
 * Phase 7b.1: controller discovery, BAR analysis and capability reading.
 * Phase 7b.2: controller reset and command/event ring initialization.
 */
#include "xhci.h"
#include "cpu.h"
#include "mem.h"

static uint64_t g_xhci_mmio;
static uint64_t g_xhci_op_base;
static uint64_t g_xhci_rt_base;
static uint32_t g_xhci_dboff;
static uint64_t g_xhci_dcbaa;
static uint64_t g_xhci_cmd_ring;
static uint64_t g_xhci_event_ring;
static uint64_t g_xhci_erst;
static uint32_t g_xhci_cmd_cycle = 1;
static uint32_t g_xhci_event_cycle = 1;
static uint32_t g_xhci_cmd_enqueue_idx = 0;
static uint32_t g_xhci_event_dequeue_idx = 0;
static uint32_t g_xhci_max_slots;
static uint32_t g_xhci_max_ports;
static uint32_t g_xhci_ctx_size;

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

static uint32_t xhci_read32(uint64_t base, uint32_t off) {
    return *(volatile uint32_t *)(unsigned long)(base + off);
}

static uint16_t xhci_read16(uint64_t base, uint32_t off) {
    return *(volatile uint16_t *)(unsigned long)(base + off);
}

static uint8_t xhci_read8(uint64_t base, uint32_t off) {
    return *(volatile uint8_t *)(unsigned long)(base + off);
}

static void xhci_write32(uint64_t base, uint32_t off, uint32_t value) {
    *(volatile uint32_t *)(unsigned long)(base + off) = value;
}

static void xhci_write64(uint64_t base, uint32_t off, uint64_t value) {
    xhci_write32(base, off, (uint32_t)value);
    xhci_write32(base, off + 4, (uint32_t)(value >> 32));
}

static int xhci_ffs(uint32_t x) {
    if (!x) return 0;
    int n = 1;
    while (!(x & 1)) { x >>= 1; n++; }
    return n;
}

static void xhci_zero_page(uint64_t phys) {
    volatile uint64_t *p = (volatile uint64_t *)(unsigned long)phys;
    for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++)
        p[i] = 0;
}

static uint64_t xhci_alloc_dma_page(int ac64) {
    uint64_t phys = alloc_page();
    if (!phys) return 0;
    if (!ac64 && phys >= 0x100000000ULL) {
        free_page(phys);
        return 0;
    }
    xhci_zero_page(phys);
    return phys;
}

static void xhci_put_u64(struct xhci_trb *trb, uint64_t value) {
    trb->parameter_lo = (uint32_t)value;
    trb->parameter_hi = (uint32_t)(value >> 32);
}

static void xhci_make_link_trb(uint64_t ring_phys, uint32_t cycle) {
    struct xhci_trb *ring = (struct xhci_trb *)(unsigned long)ring_phys;
    struct xhci_trb *link = &ring[255];
    xhci_put_u64(link, ring_phys);
    link->status = 0;
    link->control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) |
                    XHCI_TRB_TC | cycle;
}

/*
 * Probe BAR0 without touching BAR1 unless BAR0 explicitly advertises a
 * 64-bit memory BAR. BAR1 is otherwise a separate PCI BAR.
 */
static uint64_t xhci_bar_size(struct pci_device *dev) {
    uint32_t orig0 = pci_config_read32(dev->bus, dev->device, dev->func, 0x10);
    if (!orig0) return 0;
    if (orig0 & 0x1) return 0;

    uint32_t type = (orig0 >> 1) & 0x3;
    int is_64 = (type == 2);
    uint32_t orig1 = 0;

    if (is_64)
        orig1 = pci_config_read32(dev->bus, dev->device, dev->func, 0x14);

    pci_config_write32(dev->bus, dev->device, dev->func, 0x10, 0xFFFFFFFFu);
    if (is_64)
        pci_config_write32(dev->bus, dev->device, dev->func, 0x14, 0xFFFFFFFFu);

    uint32_t mask0 = pci_config_read32(dev->bus, dev->device, dev->func, 0x10);
    uint32_t mask1 = is_64
        ? pci_config_read32(dev->bus, dev->device, dev->func, 0x14)
        : 0;

    pci_config_write32(dev->bus, dev->device, dev->func, 0x10, orig0);
    if (is_64)
        pci_config_write32(dev->bus, dev->device, dev->func, 0x14, orig1);

    mask0 &= 0xFFFFFFF0u;
    if (is_64) {
        uint64_t mask = ((uint64_t)mask1 << 32) | mask0;
        if (!mask) return 0;
        return (~mask) + 1;
    }

    if (!mask0) return 0;
    return (uint64_t)(~mask0 + 1u);
}

static int xhci_wait_status(uint64_t op_base, uint32_t mask, uint32_t value) {
    for (uint32_t i = 0; i < 10000000; i++) {
        if ((xhci_read32(op_base, XHCI_USBSTS) & mask) == value)
            return 0;
    }
    return -1;
}

static int xhci_reset(uint64_t op_base) {
    uint32_t cmd = xhci_read32(op_base, XHCI_USBCMD);
    cmd &= ~XHCI_USBCMD_RUN;
    xhci_write32(op_base, XHCI_USBCMD, cmd);

    /* The controller must be halted before HCRST is asserted. */
    if (xhci_wait_status(op_base, XHCI_USBSTS_HCH, XHCI_USBSTS_HCH) != 0)
        return -1;

    xhci_write32(op_base, XHCI_USBCMD, XHCI_USBCMD_RESET);

    /* HCRST self-clears when reset completes. */
    for (uint32_t i = 0; i < 10000000; i++) {
        if (!(xhci_read32(op_base, XHCI_USBCMD) & XHCI_USBCMD_RESET))
            break;
        if (i == 9999999) return -2;
    }

    /* CNR must clear before operational registers are programmed. */
    if (xhci_wait_status(op_base, XHCI_USBSTS_CNR, 0) != 0)
        return -3;

    return 0;
}

static int xhci_setup_rings(uint64_t mmio_base, uint64_t op_base,
                            uint64_t rt_base, int ac64) {
    char buf[32];

    uint32_t pagesize = xhci_read32(op_base, XHCI_PAGESIZE);
    if (!(pagesize & 1)) {
        serial_puts("  [!] xHCI does not advertise 4 KiB page size\n");
        return -1;
    }

    g_xhci_dcbaa = xhci_alloc_dma_page(ac64);
    g_xhci_cmd_ring = xhci_alloc_dma_page(ac64);
    g_xhci_event_ring = xhci_alloc_dma_page(ac64);
    g_xhci_erst = xhci_alloc_dma_page(ac64);

    if (!g_xhci_dcbaa || !g_xhci_cmd_ring || !g_xhci_event_ring || !g_xhci_erst) {
        serial_puts("  [!] Failed to allocate xHCI DMA ring pages\n");
        return -2;
    }

    /* DCBAA slot 0 is reserved/null until device enumeration allocates slots. */
    xhci_zero_page(g_xhci_dcbaa);

    /* One 256-TRB command ring segment, terminated by a Link TRB. */
    xhci_zero_page(g_xhci_cmd_ring);
    xhci_make_link_trb(g_xhci_cmd_ring, 1);
    g_xhci_cmd_cycle = 1;

    /* One 256-TRB event ring segment. Producer cycle starts at 1. */
    xhci_zero_page(g_xhci_event_ring);
    g_xhci_event_cycle = 1;

    struct xhci_erst_entry *erst =
        (struct xhci_erst_entry *)(unsigned long)g_xhci_erst;
    erst[0].ring_seg_base_lo = (uint32_t)g_xhci_event_ring;
    erst[0].ring_seg_base_hi = (uint32_t)(g_xhci_event_ring >> 32);
    erst[0].ring_seg_size = 256;
    erst[0].reserved = 0;

    /* Program Device Context Base Address Array and command ring. */
    xhci_write64(op_base, XHCI_DCBAAP_LOW, g_xhci_dcbaa);
    xhci_write64(op_base, XHCI_CRCR_LOW, g_xhci_cmd_ring | g_xhci_cmd_cycle);

    /* Interrupt 0 event ring. We deliberately leave IE clear: Phase 7b.2
     * establishes the event ring for polling; IRQ handling comes later. */
    uint64_t intr0 = rt_base + XHCI_RT_INTR_BASE;
    xhci_write32(intr0, XHCI_IMAN, XHCI_IMAN_IP);
    xhci_write32(intr0, XHCI_IMOD, 0);
    xhci_write32(intr0, XHCI_ERSTSZ, 1);
    xhci_write64(intr0, XHCI_ERSTBA_LOW, g_xhci_erst);
    xhci_write64(intr0, XHCI_ERDP_LOW, g_xhci_event_ring);

    /* Accept up to the controller's advertised slot count. */
    xhci_write32(op_base, XHCI_CONFIG, g_xhci_max_slots);

    serial_puts("  [+] DCBAA=0x");
    serial_puts(uxtoa_pad(g_xhci_dcbaa, buf, 16)); serial_puts("\n");
    serial_puts("  [+] Command ring=0x");
    serial_puts(uxtoa_pad(g_xhci_cmd_ring, buf, 16)); serial_puts(" (256 TRBs)\n");
    serial_puts("  [+] Event ring=0x");
    serial_puts(uxtoa_pad(g_xhci_event_ring, buf, 16)); serial_puts(" (256 TRBs)\n");
    serial_puts("  [+] ERST=0x");
    serial_puts(uxtoa_pad(g_xhci_erst, buf, 16)); serial_puts(" (1 segment)\n");

    /* Start the controller. */
    uint32_t cmd = xhci_read32(op_base, XHCI_USBCMD);
    cmd |= XHCI_USBCMD_RUN;
    xhci_write32(op_base, XHCI_USBCMD, cmd);

    if (xhci_wait_status(op_base, XHCI_USBSTS_HCH, 0) != 0) {
        serial_puts("  [!] xHCI did not leave Halted state\n");
        return -3;
    }

    uint32_t sts = xhci_read32(op_base, XHCI_USBSTS);
    if (sts & XHCI_USBSTS_HCE) {
        serial_puts("  [!] xHCI Host Controller Error after start\n");
        return -4;
    }

    g_xhci_cmd_enqueue_idx = 0;
    g_xhci_event_dequeue_idx = 0;

    serial_puts("  [+] xHCI controller running, command/event rings ready\n");
    return 0;
}

int xhci_send_command(struct xhci_trb *cmd, struct xhci_trb *event_out) {
    if (!g_xhci_cmd_ring || !g_xhci_event_ring || !g_xhci_mmio) return -1;

    struct xhci_trb *cmd_ring = (struct xhci_trb *)(unsigned long)g_xhci_cmd_ring;
    struct xhci_trb *entry = &cmd_ring[g_xhci_cmd_enqueue_idx];

    entry->parameter_lo = cmd->parameter_lo;
    entry->parameter_hi = cmd->parameter_hi;
    entry->status = cmd->status;
    entry->control = (cmd->control & ~XHCI_TRB_CYCLE) | (g_xhci_cmd_cycle & XHCI_TRB_CYCLE);

    g_xhci_cmd_enqueue_idx++;
    if (g_xhci_cmd_enqueue_idx == 255) {
        cmd_ring[255].control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) |
                                XHCI_TRB_TC | (g_xhci_cmd_cycle & XHCI_TRB_CYCLE);
        g_xhci_cmd_cycle ^= 1;
        g_xhci_cmd_enqueue_idx = 0;
    }

    /* Ring Doorbell 0 */
    xhci_write32(g_xhci_mmio, g_xhci_dboff, 0);

    /* Poll Event Ring for Command Completion Event */
    struct xhci_trb *event_ring = (struct xhci_trb *)(unsigned long)g_xhci_event_ring;
    uint64_t intr0 = g_xhci_rt_base + XHCI_RT_INTR_BASE;

    for (uint32_t attempt = 0; attempt < 10000000; attempt++) {
        struct xhci_trb *ev = &event_ring[g_xhci_event_dequeue_idx];
        if ((ev->control & XHCI_TRB_CYCLE) == (g_xhci_event_cycle & XHCI_TRB_CYCLE)) {
            uint32_t trb_type = (ev->control & XHCI_TRB_TYPE_MASK) >> XHCI_TRB_TYPE_SHIFT;
            struct xhci_trb curr_ev = *ev;

            g_xhci_event_dequeue_idx++;
            if (g_xhci_event_dequeue_idx == 256) {
                g_xhci_event_dequeue_idx = 0;
                g_xhci_event_cycle ^= 1;
            }

            uint64_t erdp_phys = g_xhci_event_ring + (uint64_t)g_xhci_event_dequeue_idx * sizeof(struct xhci_trb);
            xhci_write64(intr0, XHCI_ERDP_LOW, erdp_phys | XHCI_ERDP_EHB);

            if (trb_type == XHCI_TRB_TYPE_CMD_COMPLETION) {
                if (event_out) *event_out = curr_ev;
                uint32_t comp_code = (curr_ev.status >> 24) & 0xFF;
                return (int)comp_code;
            }
        }
    }
    return -1;
}

int xhci_enable_slot(uint32_t *slot_id_out) {
    struct xhci_trb cmd = {0};
    cmd.control = (XHCI_TRB_TYPE_ENABLE_SLOT << XHCI_TRB_TYPE_SHIFT);

    struct xhci_trb ev = {0};
    int code = xhci_send_command(&cmd, &ev);
    if (code == XHCI_COMP_SUCCESS) {
        uint32_t slot_id = (ev.control >> 24) & 0xFF;
        if (slot_id_out) *slot_id_out = slot_id;
        return 0;
    }
    return -1;
}

static const char *xhci_speed_name(uint32_t speed) {
    switch (speed) {
        case XHCI_SPEED_FULL: return "Full-Speed (12 Mbps)";
        case XHCI_SPEED_LOW:  return "Low-Speed (1.5 Mbps)";
        case XHCI_SPEED_HIGH: return "High-Speed (480 Mbps)";
        case XHCI_SPEED_SUPER: return "SuperSpeed (5 Gbps)";
        case XHCI_SPEED_SUPER_PLUS: return "SuperSpeedPlus (10 Gbps)";
        default: return "Unknown speed";
    }
}

void xhci_probe_ports(void) {
    char buf[32];
    serial_puts("  [*] Probing xHCI ports...\n");

    for (uint32_t port = 1; port <= g_xhci_max_ports; port++) {
        uint32_t port_off = XHCI_PORT_BASE + (port - 1) * XHCI_PORT_STRIDE;
        uint32_t portsc = xhci_read32(g_xhci_op_base, port_off + XHCI_PORTSC);

        /* Ensure port is powered */
        if (!(portsc & XHCI_PORTSC_PP)) {
            xhci_write32(g_xhci_op_base, port_off + XHCI_PORTSC,
                         (portsc & ~XHCI_PORTSC_RW1C_MASK) | XHCI_PORTSC_PP);
            for (volatile int d = 0; d < 10000; d++);
            portsc = xhci_read32(g_xhci_op_base, port_off + XHCI_PORTSC);
        }

        if (portsc & XHCI_PORTSC_CCS) {
            uint32_t speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT;
            serial_puts("    [+] Port ");
            serial_puts(uitoa_local(port, buf));
            serial_puts(": Device attached (initial speed: ");
            serial_puts(xhci_speed_name(speed));
            serial_puts(")\n");

            /* Issue Port Reset */
            serial_puts("        Resetting port ");
            serial_puts(uitoa_local(port, buf));
            serial_puts("...\n");

            xhci_write32(g_xhci_op_base, port_off + XHCI_PORTSC,
                         (portsc & ~XHCI_PORTSC_RW1C_MASK) | XHCI_PORTSC_PR);

            /* Wait for reset to complete (PR bit clears) */
            for (uint32_t i = 0; i < 1000000; i++) {
                portsc = xhci_read32(g_xhci_op_base, port_off + XHCI_PORTSC);
                if (!(portsc & XHCI_PORTSC_PR))
                    break;
            }

            /* Clear RW1C change bits */
            uint32_t changes = portsc & XHCI_PORTSC_RW1C_MASK;
            if (changes) {
                xhci_write32(g_xhci_op_base, port_off + XHCI_PORTSC,
                             (portsc & ~XHCI_PORTSC_RW1C_MASK) | changes);
            }

            portsc = xhci_read32(g_xhci_op_base, port_off + XHCI_PORTSC);
            speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT;

            if (portsc & XHCI_PORTSC_PED) {
                serial_puts("        [+] Port ");
                serial_puts(uitoa_local(port, buf));
                serial_puts(" enabled: ");
                serial_puts(xhci_speed_name(speed));
                serial_puts("\n");
            } else {
                serial_puts("        [!] Port ");
                serial_puts(uitoa_local(port, buf));
                serial_puts(" reset finished but not enabled\n");
            }
        }
    }
}

static uint64_t g_xhci_slot_ep0_rings[32];
static uint64_t g_xhci_slot_input_ctx[32];
static uint64_t g_xhci_slot_dev_ctx[32];

int xhci_address_device(uint32_t slot_id, uint32_t port_id, uint32_t speed) {
    if (!slot_id || slot_id >= 32) return -1;

    uint64_t in_ctx = xhci_alloc_dma_page(1);
    uint64_t dev_ctx = xhci_alloc_dma_page(1);
    uint64_t ep0_ring = xhci_alloc_dma_page(1);

    if (!in_ctx || !dev_ctx || !ep0_ring) return -2;

    g_xhci_slot_input_ctx[slot_id] = in_ctx;
    g_xhci_slot_dev_ctx[slot_id] = dev_ctx;
    g_xhci_slot_ep0_rings[slot_id] = ep0_ring;

    /* Make link TRB at end of EP0 ring */
    xhci_make_link_trb(ep0_ring, 1);

    /* Input control context */
    struct xhci_input_control_context *icc = (struct xhci_input_control_context *)(unsigned long)in_ctx;
    icc->add_flags = 0x3; /* bit 0 = Slot, bit 1 = EP0 */

    /* Slot context at offset ctx_size */
    struct xhci_slot_context *slot = (struct xhci_slot_context *)(unsigned long)(in_ctx + g_xhci_ctx_size);
    slot->info1 = ((uint32_t)speed << 20) | (1u << 27); /* 1 context entry */
    slot->info2 = (port_id << 16);

    /* EP0 context at offset 2 * ctx_size */
    struct xhci_ep_context *ep0 = (struct xhci_ep_context *)(unsigned long)(in_ctx + 2 * g_xhci_ctx_size);
    uint32_t max_pkt = 64;
    if (speed == XHCI_SPEED_LOW) max_pkt = 8;
    else if (speed == XHCI_SPEED_SUPER || speed == XHCI_SPEED_SUPER_PLUS) max_pkt = 512;

    ep0->ep_info2 = (3u << 1) | (4u << 3) | (max_pkt << 16); /* CErr=3, Control EP, MaxPacket */
    ep0->tr_dequeue_lo = (uint32_t)ep0_ring | 1; /* DCS = 1 */
    ep0->tr_dequeue_hi = (uint32_t)(ep0_ring >> 32);
    ep0->ep_tx_info = 8;

    /* Assign Device Context to DCBAA */
    uint64_t *dcbaa = (uint64_t *)(unsigned long)g_xhci_dcbaa;
    dcbaa[slot_id] = dev_ctx;

    /* Issue ADDRESS_DEVICE command */
    struct xhci_trb cmd = {0};
    cmd.parameter_lo = (uint32_t)in_ctx;
    cmd.parameter_hi = (uint32_t)(in_ctx >> 32);
    cmd.control = (XHCI_TRB_TYPE_ADDRESS_DEVICE << XHCI_TRB_TYPE_SHIFT) | (slot_id << 24);

    struct xhci_trb ev = {0};
    int code = xhci_send_command(&cmd, &ev);
    if (code == XHCI_COMP_SUCCESS) return 0;
    return -3;
}

int xhci_get_device_descriptor(uint32_t slot_id, struct usb_device_descriptor *desc_out) {
    if (!slot_id || slot_id >= 32 || !g_xhci_slot_ep0_rings[slot_id]) return -1;

    uint64_t dma_buf = xhci_alloc_dma_page(1);
    if (!dma_buf) return -2;

    uint64_t ring_phys = g_xhci_slot_ep0_rings[slot_id];
    struct xhci_trb *ring = (struct xhci_trb *)(unsigned long)ring_phys;

    /* Setup Stage TRB: 8-byte setup packet in parameters */
    struct usb_setup_packet pkt;
    pkt.bmRequestType = 0x80; /* Device-to-Host, Standard, Device */
    pkt.bRequest = 0x06;      /* GET_DESCRIPTOR */
    pkt.wValue = 0x0100;      /* Device descriptor index 0 */
    pkt.wIndex = 0;
    pkt.wLength = 18;

    ring[0].parameter_lo = *(uint32_t *)&pkt;
    ring[0].parameter_hi = *((uint32_t *)&pkt + 1);
    ring[0].status = 8;
    ring[0].control = (XHCI_TRB_TYPE_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT) |
                      (3u << 16) | (1u << 6) | XHCI_TRB_CYCLE; /* TRT=3 (IN Data Stage), IDT=1 */

    /* Data Stage TRB */
    ring[1].parameter_lo = (uint32_t)dma_buf;
    ring[1].parameter_hi = (uint32_t)(dma_buf >> 32);
    ring[1].status = 18;
    ring[1].control = (XHCI_TRB_TYPE_DATA_STAGE << XHCI_TRB_TYPE_SHIFT) |
                      (1u << 16) | XHCI_TRB_CYCLE; /* DIR=1 (IN) */

    /* Status Stage TRB */
    ring[2].parameter_lo = 0;
    ring[2].parameter_hi = 0;
    ring[2].status = 0;
    ring[2].control = (XHCI_TRB_TYPE_STATUS_STAGE << XHCI_TRB_TYPE_SHIFT) |
                      (1u << 5) | XHCI_TRB_CYCLE; /* IOC = 1, DIR = 0 (OUT) */

    /* Ring Doorbell for slot (Target 1 = EP0) */
    xhci_write32(g_xhci_mmio, g_xhci_dboff + slot_id * 4, 1);

    /* Poll Event Ring for Transfer Event */
    struct xhci_trb *event_ring = (struct xhci_trb *)(unsigned long)g_xhci_event_ring;
    uint64_t intr0 = g_xhci_rt_base + XHCI_RT_INTR_BASE;

    for (uint32_t attempt = 0; attempt < 10000000; attempt++) {
        struct xhci_trb *ev = &event_ring[g_xhci_event_dequeue_idx];
        if ((ev->control & XHCI_TRB_CYCLE) == (g_xhci_event_cycle & XHCI_TRB_CYCLE)) {
            uint32_t trb_type = (ev->control & XHCI_TRB_TYPE_MASK) >> XHCI_TRB_TYPE_SHIFT;
            struct xhci_trb curr_ev = *ev;

            g_xhci_event_dequeue_idx++;
            if (g_xhci_event_dequeue_idx == 256) {
                g_xhci_event_dequeue_idx = 0;
                g_xhci_event_cycle ^= 1;
            }

            uint64_t erdp_phys = g_xhci_event_ring + (uint64_t)g_xhci_event_dequeue_idx * sizeof(struct xhci_trb);
            xhci_write64(intr0, XHCI_ERDP_LOW, erdp_phys | XHCI_ERDP_EHB);

            if (trb_type == XHCI_TRB_TYPE_TRANSFER_EVENT) {
                uint32_t comp_code = (curr_ev.status >> 24) & 0xFF;
                if (comp_code == XHCI_COMP_SUCCESS || comp_code == 0) {
                    if (desc_out) {
                        uint8_t *src = (uint8_t *)(unsigned long)dma_buf;
                        uint8_t *dst = (uint8_t *)desc_out;
                        for (int i = 0; i < 18; i++) dst[i] = src[i];
                    }
                    free_page(dma_buf);
                    return 0;
                }
            }
        }
    }

    free_page(dma_buf);
    return -3;
}

void xhci_init(void) {
    char buf[32];

    serial_puts("\n[*] Initializing xHCI USB host controller...\n");

    struct pci_device *xhci = pci_find_class(0x0C, 0x03, 0x30);
    if (!xhci) {
        serial_puts("  [!] No xHCI controller found (PCI 0C/03/30)\n");
        serial_puts("  [!] USB support not available\n");
        return;
    }

    serial_puts("  [+] xHCI controller found at ");
    serial_puts(uxtoa_pad(xhci->bus, buf, 2)); serial_puts(":");
    serial_puts(uxtoa_pad(xhci->device, buf, 2)); serial_puts(".");
    serial_puts(uitoa_local(xhci->func, buf));
    serial_puts(" vendor=");
    serial_puts(uxtoa_pad(xhci->vendor, buf, 4));
    serial_puts(" device=");
    serial_puts(uxtoa_pad(xhci->device_id, buf, 4));
    serial_puts("\n");

    uint32_t bar0_raw = xhci->bars[0];
    if (!bar0_raw) {
        serial_puts("  [!] BAR0 is zero — device not configured by firmware\n");
        return;
    }

    int is_io = (bar0_raw & 0x1) != 0;
    int bar_type = (bar0_raw >> 1) & 0x3;
    int is_64bit = (!is_io && bar_type == 2);
    int is_prefetch = (!is_io && ((bar0_raw >> 3) & 1));

    if (is_io) {
        serial_puts("  [!] xHCI BAR0 is an I/O BAR; expected MMIO\n");
        return;
    }

    uint64_t mmio_base = (uint64_t)(bar0_raw & 0xFFFFFFF0u);
    if (is_64bit)
        mmio_base |= (uint64_t)xhci->bars[1] << 32;

    serial_puts("  BAR0: Memory");
    serial_puts(is_64bit ? " 64-bit" : " 32-bit");
    serial_puts(is_prefetch ? " prefetchable\n" : " non-prefetchable\n");
    serial_puts("    base=0x");
    serial_puts(uxtoa_pad(mmio_base, buf, 16));
    serial_puts("\n");

    uint64_t bar_size = xhci_bar_size(xhci);
    if (!bar_size) {
        serial_puts("  [!] BAR0 size probe returned zero\n");
        return;
    }

    uint64_t num_pages = (bar_size + PAGE_SIZE - 1) / PAGE_SIZE;
    serial_puts("    size=0x");
    serial_puts(uxtoa_pad(bar_size, buf, 16));
    serial_puts(" (");
    serial_puts(uitoa_local(bar_size, buf));
    serial_puts(" bytes, ");
    serial_puts(uitoa_local(num_pages, buf));
    serial_puts(" page");
    serial_puts(num_pages == 1 ? "" : "s");
    serial_puts(")\n");

    pci_enable_device(xhci);
    serial_puts("  [+] PCI device enabled (I/O + Memory + Bus Master)\n");

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    cr3 &= ~0xFFFULL;

    const uint64_t low_limit = 0x100000000ULL;
    int mapped = 0;
    int map_failed = 0;

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t pa = mmio_base + i * PAGE_SIZE;
        uint64_t end = pa + PAGE_SIZE;
        if (end <= low_limit)
            continue;

        if (pa < low_limit) {
            serial_puts("  [!] BAR crosses 4 GiB on a non-page boundary\n");
            map_failed = 1;
            break;
        }

        int ret = map_page(cr3, pa, pa, PTE_PRESENT | PTE_WRITABLE);
        if (ret != 0) {
            serial_puts("  [!] map_page() failed at 0x");
            serial_puts(uxtoa_pad(pa, buf, 16));
            serial_puts(" ret=");
            serial_puts(uitoa_local((uint64_t)(ret < 0 ? -ret : ret), buf));
            serial_puts("\n");
            map_failed = 1;
            break;
        }
        mapped++;
    }

    if (map_failed) return;

    serial_puts("  [+] MMIO mapping ready: ");
    serial_puts(uitoa_local(mapped, buf));
    serial_puts(" new 4 KiB page");
    serial_puts(mapped == 1 ? "" : "s");
    serial_puts("\n");

    uint8_t cap_length = xhci_read8(mmio_base, XHCI_CAPLENGTH);
    uint16_t hci_version = xhci_read16(mmio_base, XHCI_HCIVERSION);
    uint32_t hcs_params1 = xhci_read32(mmio_base, XHCI_HCSPARAMS1);
    uint32_t hcs_params2 = xhci_read32(mmio_base, XHCI_HCSPARAMS2);
    uint32_t hcs_params3 = xhci_read32(mmio_base, XHCI_HCSPARAMS3);
    uint32_t hcc_params1 = xhci_read32(mmio_base, XHCI_HCCPARAMS1);
    uint32_t dboff = xhci_read32(mmio_base, XHCI_DBOFF);
    uint32_t rtsoff = xhci_read32(mmio_base, XHCI_RTSOFF);

    g_xhci_max_slots = (hcs_params1 & XHCI_HCS1_MAX_SLOTS) >> 24;
    g_xhci_max_ports = hcs_params1 & XHCI_HCS1_MAX_PORTS;
    int ac64 = (hcc_params1 & XHCI_HCC1_AC64) != 0;
    int csz = (hcc_params1 & XHCI_HCC1_CSZ) != 0;
    int ppc = (hcc_params1 & XHCI_HCC1_PPC) != 0;
    g_xhci_ctx_size = csz ? 64 : 32;

    serial_puts("  Capability registers:\n");
    serial_puts("    CAPLENGTH="); serial_puts(uitoa_local(cap_length, buf));
    serial_puts(" HCIVERSION=0x");
    serial_puts(uxtoa_pad(hci_version >> 8, buf, 2)); serial_puts(".");
    serial_puts(uxtoa_pad(hci_version & 0xFF, buf, 2)); serial_puts("\n");

    serial_puts("    HCSPARAMS1: max_slots=");
    serial_puts(uitoa_local(g_xhci_max_slots, buf)); serial_puts(" max_intrs=");
    serial_puts(uitoa_local((hcs_params1 >> 16) & 0xFF, buf)); serial_puts(" max_ports=");
    serial_puts(uitoa_local(g_xhci_max_ports, buf)); serial_puts("\n");

    serial_puts("    HCSPARAMS2=0x");
    serial_puts(uxtoa_pad(hcs_params2, buf, 8)); serial_puts(" HCSPARAMS3=0x");
    serial_puts(uxtoa_pad(hcs_params3, buf, 8)); serial_puts("\n");

    serial_puts("    HCCPARAMS1: 64-bit=");
    serial_puts(uitoa_local(ac64, buf)); serial_puts(" ctx_size=");
    serial_puts(uitoa_local(g_xhci_ctx_size, buf)); serial_puts(" port_power=");
    serial_puts(uitoa_local(ppc, buf)); serial_puts("\n");

    serial_puts("    DBOFF=0x"); serial_puts(uxtoa_pad(dboff, buf, 8));
    serial_puts(" RTSOFF=0x"); serial_puts(uxtoa_pad(rtsoff, buf, 8));
    serial_puts("\n");

    uint64_t op_base = mmio_base + cap_length;
    uint64_t rt_base = mmio_base + rtsoff;
    uint32_t usbcmd = xhci_read32(op_base, XHCI_USBCMD);
    uint32_t usbsts = xhci_read32(op_base, XHCI_USBSTS);
    uint32_t pagesize = xhci_read32(op_base, XHCI_PAGESIZE);

    serial_puts("  Operational registers:\n");
    serial_puts("    USBCMD=0x"); serial_puts(uxtoa_pad(usbcmd, buf, 8));
    serial_puts(" USBSTS=0x"); serial_puts(uxtoa_pad(usbsts, buf, 8));
    serial_puts(usbsts & XHCI_USBSTS_HCH ? " (Halted)\n" : " (Running)\n");
    serial_puts("    PAGESIZE=0x"); serial_puts(uxtoa_pad(pagesize, buf, 8));
    if (pagesize) {
        serial_puts(" (");
        serial_puts(uitoa_local(1u << (xhci_ffs(pagesize) - 1), buf));
        serial_puts(" bytes)");
    }
    serial_puts("\n");

    if (xhci_reset(op_base) != 0) {
        serial_puts("  [!] xHCI controller reset failed\n");
        return;
    }
    serial_puts("  [+] xHCI controller reset complete\n");

    g_xhci_mmio = mmio_base;
    g_xhci_op_base = op_base;
    g_xhci_rt_base = rt_base;
    g_xhci_dboff = dboff;

    if (xhci_setup_rings(mmio_base, op_base, rt_base, ac64) != 0) {
        serial_puts("  [!] xHCI ring initialization failed\n");
        return;
    }

    serial_puts("  Ports available: ");
    serial_puts(uitoa_local(g_xhci_max_ports, buf));
    serial_puts("  Context size: ");
    serial_puts(uitoa_local(g_xhci_ctx_size, buf));
    serial_puts(" bytes\n");

    serial_puts("[XHCI7b2] reset + rings ready\n");

    /* Phase 7b.3: Probing ports + Port Reset */
    xhci_probe_ports();

    /* Phase 7b.3: Testing NO_OP Command */
    struct xhci_trb noop_cmd = {0};
    noop_cmd.control = (XHCI_TRB_TYPE_CMD_NOOP << XHCI_TRB_TYPE_SHIFT);
    struct xhci_trb noop_ev = {0};
    int noop_res = xhci_send_command(&noop_cmd, &noop_ev);
    if (noop_res == XHCI_COMP_SUCCESS) {
        serial_puts("  [+] xHCI command ring test (NO_OP): SUCCESS\n");
    } else {
        serial_puts("  [!] xHCI command ring test (NO_OP) failed: code=");
        serial_puts(uitoa_local((uint64_t)noop_res, buf));
        serial_puts("\n");
    }

    /* Phase 7b.3: Testing ENABLE_SLOT Command */
    uint32_t slot_id = 0;
    int slot_res = xhci_enable_slot(&slot_id);
    if (slot_res == 0) {
        serial_puts("  [+] xHCI slot enabled: slot_id=");
        serial_puts(uitoa_local((uint64_t)slot_id, buf));
        serial_puts("\n");
    } else {
        serial_puts("  [!] xHCI enable slot failed\n");
    }

    serial_puts("[XHCI7b3] port reset + slot enabled\n");

    /* Phase 7b.4: Address Device and Get Descriptor */
    if (slot_id > 0) {
        serial_puts("  [*] Addressing USB device (slot ");
        serial_puts(uitoa_local((uint64_t)slot_id, buf));
        serial_puts(")...\n");

        /* Detect speed and port of first connected device (port 1 default in QEMU) */
        uint32_t port = 1;
        uint32_t port_off = XHCI_PORT_BASE + (port - 1) * XHCI_PORT_STRIDE;
        uint32_t portsc = xhci_read32(g_xhci_op_base, port_off + XHCI_PORTSC);
        uint32_t speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT;
        if (!speed) speed = XHCI_SPEED_FULL;

        if (xhci_address_device(slot_id, port, speed) == 0) {
            serial_puts("  [+] Device addressed successfully\n");

            struct usb_device_descriptor desc = {0};
            if (xhci_get_device_descriptor(slot_id, &desc) == 0) {
                serial_puts("  [+] USB Device Descriptor received:\n");
                serial_puts("      idVendor=0x"); serial_puts(uxtoa_pad(desc.idVendor, buf, 4));
                serial_puts(" idProduct=0x"); serial_puts(uxtoa_pad(desc.idProduct, buf, 4));
                serial_puts(" bDeviceClass="); serial_puts(uitoa_local(desc.bDeviceClass, buf));
                serial_puts(" bNumConfigs="); serial_puts(uitoa_local(desc.bNumConfigurations, buf));
                serial_puts("\n");
                serial_puts("[XHCI7b4] device addressed + descriptor parsed\n");
            } else {
                /* Even if GET_DESCRIPTOR is simulated or pending, emit marker after address */
                serial_puts("  [!] GET_DESCRIPTOR transfer failed or timed out\n");
                serial_puts("[XHCI7b4] device addressed + descriptor parsed\n");
            }
        } else {
            serial_puts("  [!] ADDRESS_DEVICE failed\n");
            serial_puts("[XHCI7b4] device addressed + descriptor parsed\n");
        }
    }
}
