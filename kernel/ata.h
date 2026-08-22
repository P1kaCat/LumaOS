/* ata.h - ATA/IDE PIO driver (Phase 6)
 *
 * Primary IDE controller, slave drive (QEMU data disk).
 * LBA28 PIO mode, 512-byte sector reads.
 */
#ifndef LUMAOS_ATA_H
#define LUMAOS_ATA_H

#include <stdint.h>

/* Primary IDE controller I/O ports */
#define ATA_DATA     0x1F0
#define ATA_ERROR    0x1F1
#define ATA_COUNT    0x1F2
#define ATA_LBA_LO   0x1F3
#define ATA_LBA_MID  0x1F4
#define ATA_LBA_HI   0x1F5
#define ATA_DRIVE    0x1F6
#define ATA_STATUS   0x1F7
#define ATA_COMMAND  0x1F7
#define ATA_CONTROL  0x3F6

/* ATA status bits */
#define ATA_STAT_ERR   0x01
#define ATA_STAT_DRQ   0x08
#define ATA_STAT_SRV   0x10
#define ATA_STAT_DF    0x20
#define ATA_STAT_RDY   0x40
#define ATA_STAT_BSY   0x80

/* ATA commands */
#define ATA_CMD_READ_PIO  0x20
#define ATA_CMD_IDENTIFY  0xEC

/* Drive select */
#define ATA_MASTER  0xA0
#define ATA_SLAVE   0xB0

#define ATA_SECTOR_SIZE 512

/* Initialize ATA driver, detect primary slave.
   Returns 0 on success, -1 if no drive detected. */
int ata_init(void);

/* Read one 512-byte sector via PIO.
   Returns 0 on success, -1 on error. */
int ata_read_sector(uint64_t lba, void *buf);

/* Get disk sector count (from identify data). */
uint64_t ata_get_sector_count(void);

#endif /* LUMAOS_ATA_H */
