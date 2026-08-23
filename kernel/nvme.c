/* nvme.c — NVMe PCIe SSD Controller Driver
 *
 * Phase 7d: NVMe controller initialization, Admin Queue setup and IDENTIFY.
 */
#include "nvme.h"
#include "cpu.h"
#include "mem.h"

static uint64_t g_nvme_bar0;
static uint32_t g_nvme_dstrd;
static uint64_t g_nvme_asq;
static uint64_t g_nvme_acq;
static uint16_t g_nvme_sq_tail;
static uint16_t g_nvme_cq_head;
static uint8_t  g_nvme_cq_phase = 1;

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

static uint32_t nvme_read32(uint64_t base, uint32_t off) {
    return *(volatile uint32_t *)(unsigned long)(base + off);
}

static uint64_t nvme_read64(uint64_t base, uint32_t off) {
    uint32_t lo = nvme_read32(base, off);
    uint32_t hi = nvme_read32(base, off + 4);
    return ((uint64_t)hi << 32) | lo;
}

static void nvme_write32(uint64_t base, uint32_t off, uint32_t val) {
    *(volatile uint32_t *)(unsigned long)(base + off) = val;
}

static void nvme_write64(uint64_t base, uint32_t off, uint64_t val) {
    nvme_write32(base, off, (uint32_t)val);
    nvme_write32(base, off + 4, (uint32_t)(val >> 32));
}

void nvme_init(void) {
    char buf[32];
    serial_puts("\n[*] Initializing NVMe PCIe controller...\n");

    struct pci_device *nvme_dev = pci_find_class(0x01, 0x08, 0x02);
    if (!nvme_dev) {
        serial_puts("  [!] No NVMe controller found (PCI 01/08/02)\n");
        return;
    }

    serial_puts("  [+] NVMe controller found at ");
    serial_puts(uxtoa_pad(nvme_dev->bus, buf, 2)); serial_puts(":");
    serial_puts(uxtoa_pad(nvme_dev->device, buf, 2)); serial_puts(".");
    serial_puts(uitoa_local(nvme_dev->func, buf));
    serial_puts(" vendor=");
    serial_puts(uxtoa_pad(nvme_dev->vendor, buf, 4));
    serial_puts(" device=");
    serial_puts(uxtoa_pad(nvme_dev->device_id, buf, 4));
    serial_puts("\n");

    uint32_t bar0_raw = nvme_dev->bars[0];
    if (!bar0_raw) {
        serial_puts("  [!] NVMe BAR0 is zero\n");
        return;
    }

    int is_64 = ((bar0_raw >> 1) & 3) == 2;
    uint64_t bar0 = (uint64_t)(bar0_raw & 0xFFFFFFF0u);
    if (is_64) bar0 |= (uint64_t)nvme_dev->bars[1] << 32;

    g_nvme_bar0 = bar0;
    pci_enable_device(nvme_dev);

    /* Map MMIO pages if above 4GB */
    if (bar0 >= 0x100000000ULL) {
        uint64_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        cr3 &= ~0xFFFULL;
        for (uint64_t i = 0; i < 4; i++) {
            map_page(cr3, bar0 + i * PAGE_SIZE, bar0 + i * PAGE_SIZE, PTE_PRESENT | PTE_WRITABLE);
        }
    }

    serial_puts("  BAR0=0x");
    serial_puts(uxtoa_pad(bar0, buf, 16));
    serial_puts("\n");

    uint64_t cap = nvme_read64(bar0, NVME_REG_CAP);
    uint32_t vs = nvme_read32(bar0, NVME_REG_VS);
    g_nvme_dstrd = (uint32_t)((cap >> 32) & 0xF);

    serial_puts("  NVMe Version: ");
    serial_puts(uitoa_local((vs >> 16) & 0xFFFF, buf)); serial_puts(".");
    serial_puts(uitoa_local((vs >> 8) & 0xFF, buf)); serial_puts(".");
    serial_puts(uitoa_local(vs & 0xFF, buf));
    serial_puts(" (Doorbell stride: ");
    serial_puts(uitoa_local(g_nvme_dstrd, buf));
    serial_puts(")\n");

    /* Disable controller first (CC.EN = 0) */
    uint32_t cc = nvme_read32(bar0, NVME_REG_CC);
    if (cc & NVME_CC_EN) {
        nvme_write32(bar0, NVME_REG_CC, cc & ~NVME_CC_EN);
        for (uint32_t i = 0; i < 1000000; i++) {
            if (!(nvme_read32(bar0, NVME_REG_CSTS) & NVME_CSTS_RDY))
                break;
        }
    }

    /* Allocate Admin Submission Queue and Completion Queue */
    g_nvme_asq = alloc_page();
    g_nvme_acq = alloc_page();

    if (!g_nvme_asq || !g_nvme_acq) {
        serial_puts("  [!] Failed to allocate NVMe Admin Queue pages\n");
        return;
    }

    volatile uint8_t *asq_p = (volatile uint8_t *)(unsigned long)g_nvme_asq;
    volatile uint8_t *acq_p = (volatile uint8_t *)(unsigned long)g_nvme_acq;
    for (int i = 0; i < 4096; i++) { asq_p[i] = 0; acq_p[i] = 0; }

    g_nvme_sq_tail = 0;
    g_nvme_cq_head = 0;
    g_nvme_cq_phase = 1;

    /* AQA: 64 entries in ASQ and ACQ (size - 1 = 63 = 0x3F) */
    uint32_t aqa = (63u << 16) | 63u;
    nvme_write32(bar0, NVME_REG_AQA, aqa);
    nvme_write64(bar0, NVME_REG_ASQ, g_nvme_asq);
    nvme_write64(bar0, NVME_REG_ACQ, g_nvme_acq);

    /* Enable Controller */
    uint32_t new_cc = NVME_CC_EN | NVME_CC_CSS_NVM | NVME_CC_MPS_4K |
                      NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16;
    nvme_write32(bar0, NVME_REG_CC, new_cc);

    /* Wait for CSTS.RDY = 1 */
    int ready = 0;
    for (uint32_t i = 0; i < 2000000; i++) {
        if (nvme_read32(bar0, NVME_REG_CSTS) & NVME_CSTS_RDY) {
            ready = 1;
            break;
        }
    }

    if (!ready) {
        serial_puts("  [!] NVMe controller failed to become Ready\n");
        return;
    }

    serial_puts("  [+] NVMe Controller Ready (ASQ=0x");
    serial_puts(uxtoa_pad(g_nvme_asq, buf, 16));
    serial_puts(", ACQ=0x");
    serial_puts(uxtoa_pad(g_nvme_acq, buf, 16));
    serial_puts(")\n");

    /* Test Admin Command: IDENTIFY Controller */
    uint64_t id_buf = alloc_page();
    if (id_buf) {
        volatile uint8_t *id_p = (volatile uint8_t *)(unsigned long)id_buf;
        for (int i = 0; i < 4096; i++) id_p[i] = 0;

        struct nvme_sqe *sq = (struct nvme_sqe *)(unsigned long)g_nvme_asq;
        struct nvme_sqe *cmd = &sq[g_nvme_sq_tail];
        cmd->opcode = NVME_ADMIN_IDENTIFY;
        cmd->flags = 0;
        cmd->cid = 1;
        cmd->nsid = 0;
        cmd->prp1 = id_buf;
        cmd->cdw10 = 1; /* CNS 1 = Identify Controller */

        g_nvme_sq_tail = (g_nvme_sq_tail + 1) % 64;

        /* Ring Admin SQ Doorbell (offset 0x1000 + 0) */
        nvme_write32(bar0, 0x1000, g_nvme_sq_tail);

        /* Poll Admin CQ */
        struct nvme_cqe *cq = (struct nvme_cqe *)(unsigned long)g_nvme_acq;
        int completed = 0;
        for (uint32_t i = 0; i < 1000000; i++) {
            uint16_t status = cq[g_nvme_cq_head].status;
            if ((status & 1) == g_nvme_cq_phase) {
                completed = 1;
                break;
            }
        }

        if (completed) {
            struct nvme_id_ctrl *ctrl_info = (struct nvme_id_ctrl *)(unsigned long)id_buf;
            char model[41];
            for (int i = 0; i < 40; i++) model[i] = ctrl_info->mn[i];
            model[40] = 0;
            /* Trim trailing spaces */
            for (int i = 39; i >= 0 && model[i] == ' '; i--) model[i] = 0;

            char serial[21];
            for (int i = 0; i < 20; i++) serial[i] = ctrl_info->sn[i];
            serial[20] = 0;
            for (int i = 19; i >= 0 && serial[i] == ' '; i--) serial[i] = 0;

            serial_puts("  [+] NVMe Model: ");
            serial_puts(model);
            serial_puts(" (S/N: ");
            serial_puts(serial);
            serial_puts(")\n");

            g_nvme_cq_head = (g_nvme_cq_head + 1) % 64;
            if (g_nvme_cq_head == 0) g_nvme_cq_phase ^= 1;

            /* Ring Admin CQ Doorbell */
            uint32_t cq_db_off = 0x1000 + (1 << (2 + g_nvme_dstrd));
            nvme_write32(bar0, cq_db_off, g_nvme_cq_head);
        }
        free_page(id_buf);
    }

    serial_puts("[NVME7d] controller initialized + admin queue ready\n");
}
