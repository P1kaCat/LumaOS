/* fat32.h - FAT32 read-only parser (Phase 6)
 *
 * On-disk structures use __attribute__((packed)) to match
 * the FAT32 specification exactly. No compiler padding assumptions.
 *
 * Design: sector-based FAT access (no full FAT cache).
 * A small sector cache is used for the currently-read FAT sector.
 */
#ifndef LUMAOS_FAT32_H
#define LUMAOS_FAT32_H

#include <stdint.h>

/* ===== On-disk structures (packed, match FAT32 spec) ===== */

/* FAT32 Boot Sector / BPB (BIOS Parameter Block)
 * Fields at their exact byte offsets per the FAT spec. */
struct __attribute__((packed)) fat32_bpb {
    uint8_t  jmp_boot[3];          /* 0x00: jump instruction */
    char     oem_name[8];         /* 0x03: OEM name */
    uint16_t bytes_per_sector;    /* 0x0B: usually 512 */
    uint8_t  sectors_per_cluster; /* 0x0D: power of 2 */
    uint16_t reserved_sectors;    /* 0x0E: typically 32 */
    uint8_t  num_fats;            /* 0x10: typically 2 */
    uint16_t root_entry_count;    /* 0x11: 0 for FAT32 */
    uint16_t total_sectors16;     /* 0x13: 0 for FAT32 */
    uint8_t  media_type;          /* 0x15: 0xF8 for fixed disk */
    uint16_t fat_size16;          /* 0x17: 0 for FAT32 */
    uint16_t sectors_per_track;   /* 0x19: geometry */
    uint16_t num_heads;           /* 0x1B: geometry */
    uint32_t hidden_sectors;      /* 0x1D: partition offset */
    uint32_t total_sectors32;     /* 0x21: total sectors */

    /* FAT32 extended BPB fields */
    uint32_t fat_size32;          /* 0x25: sectors per FAT */
    uint16_t ext_flags;           /* 0x29: flags */
    uint16_t fs_version;          /* 0x2B: 0 */
    uint32_t root_cluster;        /* 0x2F: usually 2 */
    uint16_t fs_info_sector;      /* 0x33: usually 1 */
    uint16_t backup_boot_sector;  /* 0x35: usually 6 */
    uint8_t  reserved[12];        /* 0x37: reserved */
    uint8_t  drive_number;        /* 0x43: 0x80 */
    uint8_t  reserved1;           /* 0x44: 0 */
    uint8_t  boot_signature;      /* 0x45: 0x29 */
    uint32_t volume_id;           /* 0x46: serial number */
    char     volume_label[11];    /* 0x4A: volume label */
    char     fs_type[8];          /* 0x52: "FAT32   " */
};

/* FAT32 directory entry (32 bytes, packed) */
struct __attribute__((packed)) fat32_dir_entry {
    uint8_t  name[11];        /* 0x00: 8.3 name (not null-terminated) */
    uint8_t  attr;            /* 0x0B: file attributes */
    uint8_t  nt_reserved;     /* 0x0C: case info (NT) */
    uint8_t  creation_tenth;  /* 0x0D: creation time tenths */
    uint16_t creation_time;   /* 0x0E: creation time */
    uint16_t creation_date;   /* 0x10: creation date */
    uint16_t access_date;     /* 0x12: last access date */
    uint16_t cluster_hi;     /* 0x14: first cluster high */
    uint16_t write_time;     /* 0x16: write time */
    uint16_t write_date;     /* 0x18: write date */
    uint16_t cluster_lo;     /* 0x1A: first cluster low */
    uint32_t file_size;       /* 0x1E: file size in bytes */
};

/* File attribute bits */
#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0F  /* long file name */

/* FAT32 cluster chain markers */
#define FAT32_CLUSTER_FREE   0x00000000
#define FAT32_CLUSTER_EOC_MIN 0x0FFFFFF8
#define FAT32_CLUSTER_EOC    0x0FFFFFFF
#define FAT32_CLUSTER_BAD    0x0FFFFFF7

/* Maximum path components we support */
#define FAT32_MAX_NAME 13    /* 8.3 + null */

/* FAT32 state — initialized by fat32_init() */

/* Initialize FAT32: read BPB from sector 0, compute layout.
   Returns 0 on success, -1 on error. */
int fat32_init(void);

/* Read a cluster (sectors_per_cluster sectors) into buf.
   Returns 0 on success, -1 on error. */
int fat32_read_cluster(uint32_t cluster, void *buf);

/* Get next cluster in chain. Returns cluster number or 0 if EOC/end. */
uint32_t fat32_next_cluster(uint32_t cluster);

/* List root directory entries.
   Calls callback for each non-empty entry.
   Returns number of entries found. */
typedef void (*fat32_dir_callback)(const struct fat32_dir_entry *entry, void *ctx);
int fat32_list_root(fat32_dir_callback cb, void *ctx);

/* Look up a file by 8.3 name (e.g. "HELLO   TXT") in the root directory.
   Returns 0 on success, fills *out. -1 if not found. */
int fat32_lookup(const char *name, struct fat32_dir_entry *out);

/* Get disk layout info */
uint32_t fat32_get_data_start(void);
uint32_t fat32_get_cluster_size(void);
uint32_t fat32_get_root_cluster(void);

#endif /* LUMAOS_FAT32_H */
