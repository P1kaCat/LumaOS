/* vfs.c - Virtual File System layer (Phase 6)
 *
 * Minimal read-only VFS: open, read, close.
 * Sits on top of the FAT32 layer — converts user paths to 8.3 names,
 * manages a file descriptor table, and reads file data via cluster
 * chain walks.
 *
 * No writes, no subdirectories, no LFN. Read-only root directory only.
 */
#include "vfs.h"
#include "fat32.h"
#include "cpu.h"   /* serial_puts */
#include <stdint.h>

/* ===== Global file descriptor table =====
 * For now this is a single global table (kernel-space testing).
 * When syscalls 8/9/10 are added, this will become per-process
 * (either in the task struct or indexed by PID).
 */
static struct vfs_file fd_table[VFS_MAX_OPEN];

/* Cluster read buffer — our disk uses 1 sector/cluster = 512 bytes.
 * If cluster_size ever differs, this must be enlarged. */
static uint8_t vfs_cluster_buf[512];

/* ===== Internal: convert "hello.txt" → "HELLO   TXT" (11 chars) ===== */
static void path_to_83(const char *path, char out[11]) {
    /* Fill with spaces (FAT32 8.3 padding) */
    for (int i = 0; i < 11; i++) out[i] = ' ';

    int out_idx = 0;
    int in_ext = 0;

    for (int i = 0; path[i]; i++) {
        char c = path[i];
        if (c == '.') {
            in_ext = 1;
            out_idx = 8;       /* extension starts at position 8 */
            continue;
        }
        /* Uppercase for case-insensitive match */
        if (c >= 'a' && c <= 'z') c -= 32;
        if (!in_ext) {
            if (out_idx < 8)  out[out_idx++] = c;
        } else {
            if (out_idx < 11) out[out_idx++] = c;
        }
    }
}

/* ===== Public API ===== */

void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_OPEN; i++) {
        fd_table[i].in_use = 0;
        fd_table[i].start_cluster = 0;
        fd_table[i].cur_cluster = 0;
        fd_table[i].file_size = 0;
        fd_table[i].position = 0;
    }
    serial_puts("[VFS] File descriptor table initialized (");
    char buf[8];
    /* small uitoa inline */
    {
        uint64_t n = VFS_MAX_OPEN;
        char tmp[8]; int i = 0;
        while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
        int j = 0; while (i) buf[j++] = tmp[--i]; buf[j] = 0;
    }
    serial_puts(buf);
    serial_puts(" slots)\n");
}

int vfs_open(const char *path) {
    if (!path) return VFS_ERR_NOT_FOUND;

    /* Convert path to FAT32 8.3 format */
    char name83[11];
    path_to_83(path, name83);

    /* Look up file in root directory */
    struct fat32_dir_entry entry;
    if (fat32_lookup(name83, &entry) != 0) {
        return VFS_ERR_NOT_FOUND;
    }

    /* Skip directories — read-only file access only */
    if (entry.attr & FAT32_ATTR_DIRECTORY) {
        return VFS_ERR_NOT_FOUND;
    }

    /* Find a free file descriptor */
    for (int fd = 0; fd < VFS_MAX_OPEN; fd++) {
        if (!fd_table[fd].in_use) {
            fd_table[fd].in_use = 1;
            fd_table[fd].start_cluster =
                ((uint32_t)entry.cluster_hi << 16) | entry.cluster_lo;
            fd_table[fd].cur_cluster = fd_table[fd].start_cluster;
            fd_table[fd].file_size = entry.file_size;
            fd_table[fd].position = 0;
            return fd;
        }
    }

    return VFS_ERR_NO_FD;  /* table full */
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN || !fd_table[fd].in_use)
        return VFS_ERR_BAD_FD;

    fd_table[fd].in_use = 0;
    return VFS_OK;
}

int vfs_read(int fd, void *buf, uint32_t len) {
    if (fd < 0 || fd >= VFS_MAX_OPEN || !fd_table[fd].in_use)
        return VFS_ERR_BAD_FD;

    struct vfs_file *f = &fd_table[fd];

    /* EOF: position at or past end of file */
    if (f->position >= f->file_size)
        return 0;  /* EOF — 0 bytes read */

    /* Limit read to remaining file data */
    uint32_t remaining = f->file_size - f->position;
    if (len > remaining) len = remaining;

    uint32_t cluster_size = fat32_get_cluster_size();
    uint8_t *p = (uint8_t *)buf;
    uint32_t bytes_read = 0;

    /* Compute starting cluster and offset within cluster */
    uint32_t clusters_to_skip = f->position / cluster_size;
    uint32_t offset_in_cluster = f->position % cluster_size;

    /* Walk cluster chain to the right cluster */
    uint32_t cluster = f->start_cluster;
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        cluster = fat32_next_cluster(cluster);
        if (cluster == 0) return bytes_read;  /* unexpected end of chain */
    }

    /* Read data, possibly spanning multiple clusters */
    while (bytes_read < len) {
        if (fat32_read_cluster(cluster, vfs_cluster_buf) != 0)
            return (bytes_read > 0) ? bytes_read : VFS_ERR_IO;

        uint32_t chunk = cluster_size - offset_in_cluster;
        if (chunk > len - bytes_read) chunk = len - bytes_read;

        for (uint32_t i = 0; i < chunk; i++)
            p[bytes_read + i] = vfs_cluster_buf[offset_in_cluster + i];

        bytes_read += chunk;
        offset_in_cluster = 0;

        if (bytes_read < len) {
            cluster = fat32_next_cluster(cluster);
            if (cluster == 0) break;  /* end of chain — stop reading */
        }
    }

    f->position += bytes_read;
    return bytes_read;
}
