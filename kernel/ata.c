/* ata.c - ATA/IDE PIO driver (Phase 6)
 *
 * Primary IDE controller, slave drive.
 * LBA28 PIO mode for sector reads.
 * No DMA, no interrupts — pure polled PIO.
 */
#include "ata.h"
#include "cpu.h"  /* serial_puts, inb, outb, inw, outw, uitoa, uxtoa */

static uint64_t disk_sectors = 0;
static int ata_present = 0;

/* Wait for BSY to clear, return status register or 0xFF on timeout. */
static uint8_t ata_wait_bsy(void) {
    uint8_t status;
    int timeout = 1000000;
    do {
        status = inb(ATA_STATUS);
        if (--timeout == 0) return 0xFF;
    } while (status & ATA_STAT_BSY);
    return status;
}

/* Wait for DRQ to set (data ready). Returns 0 on success, -1 on error. */
static int ata_wait_drq(void) {
    uint8_t status;
    int timeout = 1000000;
    do {
        status = inb(ATA_STATUS);
        if (status & ATA_STAT_ERR) return -1;
        if (--timeout == 0) return -1;
    } while (!(status & ATA_STAT_DRQ));
    return 0;
}

/* 400ns delay — read status 4 times (bus settle after drive select). */
static void ata_delay(void) {
    for (int i = 0; i < 4; i++)
        (void)inb(ATA_STATUS);
}

int ata_init(void) {
    /* Disable interrupts on primary IDE controller (polled mode) */
    outb(ATA_CONTROL, 0x02);

    /* Select slave drive on primary controller */
    outb(ATA_DRIVE, ATA_SLAVE | 0x40);  /* slave + LBA mode */
    ata_delay();

    /* Check if drive responds — 0xFF means no drive */
    uint8_t status = inb(ATA_STATUS);
    if (status == 0xFF || status == 0) {
        serial_puts("[ATA] No slave drive detected (status=0x");
        char buf[8];
        serial_puts(uxtoa(status, buf));
        serial_puts(")\n");
        return -1;
    }

    /* Wait for BSY to clear */
    status = ata_wait_bsy();
    if (status == 0xFF) {
        serial_puts("[ATA] BSY timeout on IDENTIFY\n");
        return -1;
    }

    /* Send IDENTIFY command */
    outb(ATA_COUNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    /* Wait for BSY to clear after IDENTIFY */
    status = ata_wait_bsy();
    if (status == 0xFF) {
        serial_puts("[ATA] IDENTIFY: BSY timeout\n");
        return -1;
    }

    /* Check for ATA/ATAPI signature */
    uint8_t cl = inb(ATA_LBA_MID);
    uint8_t ch = inb(ATA_LBA_HI);
    if (cl != 0 || ch != 0) {
        serial_puts("[ATA] Not an ATA device (sig=0x");
        char buf[8];
        serial_puts(uxtoa((cl << 8) | ch, buf));
        serial_puts(")\n");
        return -1;
    }

    if (ata_wait_drq() != 0) {
        serial_puts("[ATA] IDENTIFY: DRQ timeout\n");
        return -1;
    }

    /* Read 256 words (512 bytes) of identify data */
    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(ATA_DATA);

    /* Sector count from IDENTIFY words 60-61 (LBA28 max) */
    disk_sectors = ((uint32_t)id[61] << 16) | id[60];

    if (disk_sectors == 0) {
        serial_puts("[ATA] IDENTIFY: sector count = 0\n");
        return -1;
    }

    ata_present = 1;

    char buf[32];
    serial_puts("[ATA] Primary slave detected: ");
    serial_puts(uitoa(disk_sectors, buf));
    serial_puts(" sectors (");
    serial_puts(uitoa(disk_sectors / 2048, buf));
    serial_puts(" MB)\n");

    return 0;
}

int ata_read_sector(uint64_t lba, void *buf) {
    if (!ata_present) return -1;
    if (lba >= disk_sectors) return -1;

    /* Wait for BSY */
    uint8_t status = ata_wait_bsy();
    if (status == 0xFF) return -1;

    /* Set up LBA28 read: slave + LBA mode + bits 24-27 */
    outb(ATA_DRIVE, ATA_SLAVE | ((lba >> 24) & 0x0F) | 0x40);
    ata_delay();

    outb(ATA_COUNT, 1);
    outb(ATA_LBA_LO, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HI, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_READ_PIO);

    /* Wait for data ready */
    status = ata_wait_bsy();
    if (status & ATA_STAT_ERR) return -1;
    if (ata_wait_drq() != 0) return -1;

    /* Read 256 words (512 bytes) */
    uint16_t *p = (uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        p[i] = inw(ATA_DATA);

    return 0;
}

uint64_t ata_get_sector_count(void) {
    return disk_sectors;
}
