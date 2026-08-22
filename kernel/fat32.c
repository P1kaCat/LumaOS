/* fat32.c - FAT32 read-only parser (Phase 6)
 *
 * Sector-based FAT access: reads FAT sectors on demand, no full FAT cache.
 * A one-sector cache avoids re-reading the same FAT sector.
 *
 * Root directory listing and file lookup by 8.3 short name.
 * No LFN support. No write. No subdirectory traversal (API designed for it).
 */
#include "fat32.h"
#include "ata.h"
#include "cpu.h"  /* serial_puts */

/* Local number-to-string helper (kernel has no shared libc) */
static char *uitoa(uint64_t n, char *buf) {
    if (!n) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[32]; int i=0; while (n) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while (i) buf[j++]=tmp[--i]; buf[j]=0; return buf;
}

/* ===== FAT32 filesystem state ===== */
static struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint32_t fat_size;          /* sectors per FAT */
    uint32_t root_cluster;
    uint32_t data_start;        /* first data sector */
    uint32_t cluster_size;      /* bytes per cluster */
    int      initialized;

    /* FAT sector cache — avoids re-reading the same sector */
    uint32_t cached_fat_sector;
    uint8_t  fat_sector_buf[512];
    int      fat_sector_valid;
} fs;

/* ===== Internal helpers ===== */

/* Read a FAT sector into the cache. */
static int fat_cache_sector(uint32_t fat_sector_idx) {
    if (fs.fat_sector_valid && fs.cached_fat_sector == fat_sector_idx)
        return 0;  /* cache hit */

    uint32_t lba = fs.reserved_sectors + fat_sector_idx;
    if (ata_read_sector(lba, fs.fat_sector_buf) != 0)
        return -1;

    fs.cached_fat_sector = fat_sector_idx;
    fs.fat_sector_valid = 1;
    return 0;
}

/* Get the FAT entry for a cluster (4 bytes in FAT32). */
static uint32_t fat_get_entry(uint32_t cluster) {
    uint32_t byte_offset = cluster * 4;
    uint32_t sector_idx = byte_offset / fs.bytes_per_sector;
    uint32_t offset_in_sector = byte_offset % fs.bytes_per_sector;

    if (fat_cache_sector(sector_idx) != 0)
        return FAT32_CLUSTER_EOC;  /* treat as end-of-chain on error */

    uint32_t entry;
    /* Read 4 bytes from the sector buffer at the offset */
    uint8_t *p = fs.fat_sector_buf + offset_in_sector;
    entry = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
            ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return entry & 0x0FFFFFFF;
}

/* Convert cluster number to LBA sector. */
static uint32_t cluster_to_lba(uint32_t cluster) {
    return fs.data_start + (cluster - 2) * fs.sectors_per_cluster;
}

/* ===== Public API ===== */

int fat32_init(void) {
    uint8_t boot_sector[512];

    if (ata_read_sector(0, boot_sector) != 0) {
        serial_puts("[FAT32] Cannot read boot sector\n");
        return -1;
    }

    /* Parse BPB from boot sector */
    struct fat32_bpb *bpb = (struct fat32_bpb *)boot_sector;

    fs.bytes_per_sector = bpb->bytes_per_sector;
    fs.sectors_per_cluster = bpb->sectors_per_cluster;
    fs.reserved_sectors = bpb->reserved_sectors;
    fs.num_fats = bpb->num_fats;
    fs.fat_size = bpb->fat_size32;
    fs.root_cluster = bpb->root_cluster;

    /* Validate */
    if (fs.bytes_per_sector != 512) {
        serial_puts("[FAT32] Unsupported sector size: ");
        char buf[8];
        serial_puts(uitoa(fs.bytes_per_sector, buf));
        serial_puts("\n");
        return -1;
    }
    if (fs.sectors_per_cluster == 0 || fs.fat_size == 0) {
        serial_puts("[FAT32] Invalid BPB parameters\n");
        return -1;
    }

    /* Compute data start */
    fs.data_start = fs.reserved_sectors + (fs.num_fats * fs.fat_size);
    fs.cluster_size = fs.bytes_per_sector * fs.sectors_per_cluster;

    /* Initialize FAT cache */
    fs.fat_sector_valid = 0;
    fs.cached_fat_sector = 0xFFFFFFFF;

    fs.initialized = 1;

    char buf[32];
    serial_puts("[FAT32] BPB parsed: sector_size=");
    serial_puts(uitoa(fs.bytes_per_sector, buf));
    serial_puts(" cluster_size=");
    serial_puts(uitoa(fs.cluster_size, buf));
    serial_puts(" reserved=");
    serial_puts(uitoa(fs.reserved_sectors, buf));
    serial_puts(" fat_size=");
    serial_puts(uitoa(fs.fat_size, buf));
    serial_puts(" data_start=");
    serial_puts(uitoa(fs.data_start, buf));
    serial_puts(" root_cluster=");
    serial_puts(uitoa(fs.root_cluster, buf));
    serial_puts("\n");

    return 0;
}

int fat32_read_cluster(uint32_t cluster, void *buf) {
    if (!fs.initialized) return -1;
    if (cluster < 2) return -1;

    uint32_t lba = cluster_to_lba(cluster);
    uint8_t *p = (uint8_t *)buf;

    for (uint32_t i = 0; i < fs.sectors_per_cluster; i++) {
        if (ata_read_sector(lba + i, p + i * 512) != 0)
            return -1;
    }
    return 0;
}

uint32_t fat32_next_cluster(uint32_t cluster) {
    if (!fs.initialized) return 0;
    uint32_t entry = fat_get_entry(cluster);
    if (entry >= FAT32_CLUSTER_EOC_MIN) return 0;  /* end of chain */
    return entry;
}

int fat32_list_root(fat32_dir_callback cb, void *ctx) {
    if (!fs.initialized) return -1;

    uint8_t cluster_buf[512];  /* max 1 sector per cluster for our disk */
    uint32_t cluster = fs.root_cluster;
    int count = 0;

    do {
        if (fat32_read_cluster(cluster, cluster_buf) != 0) {
            serial_puts("[FAT32] list_root: read error\n");
            return -1;
        }

        struct fat32_dir_entry *entries = (struct fat32_dir_entry *)cluster_buf;
        int entries_per_cluster = fs.cluster_size / sizeof(struct fat32_dir_entry);

        for (int i = 0; i < entries_per_cluster; i++) {
            /* End of directory */
            if (entries[i].name[0] == 0x00) return count;

            /* Skip deleted entries */
            if (entries[i].name[0] == 0xE5) continue;

            /* Skip LFN entries */
            if ((entries[i].attr & FAT32_ATTR_LFN) == FAT32_ATTR_LFN) continue;

            /* Skip volume label */
            if (entries[i].attr & FAT32_ATTR_VOLUME_ID) continue;

            cb(&entries[i], ctx);
            count++;
        }

        cluster = fat32_next_cluster(cluster);
    } while (cluster != 0);

    return count;
}

/* Compare 8.3 name: user gives "HELLO   TXT", on-disk is 11 bytes.
   Returns 0 if match. */
static int name_match(const uint8_t disk_name[11], const char *user_name) {
    for (int i = 0; i < 11; i++) {
        char d = (char)disk_name[i];
        char u = user_name[i];
        /* Case-insensitive for letters */
        if (d >= 'a' && d <= 'z') d -= 32;
        if (u >= 'a' && u <= 'z') u -= 32;
        if (d != u) return -1;
    }
    return 0;
}

int fat32_lookup(const char *name, struct fat32_dir_entry *out) {
    if (!fs.initialized) return -1;

    uint8_t cluster_buf[512];
    uint32_t cluster = fs.root_cluster;

    do {
        if (fat32_read_cluster(cluster, cluster_buf) != 0) return -1;

        struct fat32_dir_entry *entries = (struct fat32_dir_entry *)cluster_buf;
        int entries_per_cluster = fs.cluster_size / sizeof(struct fat32_dir_entry);

        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) return -1;  /* end of dir, not found */
            if (entries[i].name[0] == 0xE5) continue;   /* deleted */
            if ((entries[i].attr & FAT32_ATTR_LFN) == FAT32_ATTR_LFN) continue;

            if (name_match(entries[i].name, name) == 0) {
                if (out) *out = entries[i];
                return 0;
            }
        }

        cluster = fat32_next_cluster(cluster);
    } while (cluster != 0);

    return -1;  /* not found */
}

uint32_t fat32_get_data_start(void) {
    return fs.data_start;
}

uint32_t fat32_get_cluster_size(void) {
    return fs.cluster_size;
}

uint32_t fat32_get_root_cluster(void) {
    return fs.root_cluster;
}
