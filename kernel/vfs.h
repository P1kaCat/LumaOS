/* vfs.h - Virtual File System layer (Phase 6)
 *
 * Minimal read-only VFS sitting between syscalls and the FAT32 layer.
 * Per-process file descriptor table (global for now — will attach to
 * task struct when syscalls 8/9/10 are wired in).
 *
 * Architecture:
 *   Userland → Syscalls (8/9/10) → VFS → FAT32 → ATA/IDE → Disk
 *
 * Limitations (intentional for Phase 6 read-only):
 *   - No file creation, deletion, or writing
 *   - No subdirectory traversal (root dir only)
 *   - No long filename (LFN) support — 8.3 only
 *   - No permissions or timestamps
 */
#ifndef LUMAOS_VFS_H
#define LUMAOS_VFS_H

#include <stdint.h>

/* ===== Limits ===== */
#define VFS_MAX_OPEN 8   /* max simultaneously open files */

/* ===== Error codes ===== */
#define VFS_OK            0
#define VFS_ERR_NOT_FOUND (-1)  /* file not found in root directory */
#define VFS_ERR_NO_FD     (-2)  /* no free file descriptor available */
#define VFS_ERR_BAD_FD    (-3)  /* invalid file descriptor (out of range or not open) */
#define VFS_ERR_IO        (-4)  /* I/O error reading from disk */

/* ===== Open file structure ===== */
struct vfs_file {
    int      in_use;          /* 0 = free slot, 1 = open file */
    uint32_t start_cluster;   /* first cluster of file data */
    uint32_t cur_cluster;     /* current cluster in chain (for sequential reads) */
    uint32_t file_size;       /* total file size in bytes */
    uint32_t position;        /* current read offset (0..file_size) */
};

/* Initialize VFS subsystem (clears FD table). Call after fat32_init(). */
void vfs_init(void);

/* Open a file by path (e.g. "hello.txt" or "HELLO.TXT").
   Path is case-insensitive, 8.3 format only.
   Returns file descriptor (>= 0) on success, negative error on failure. */
int vfs_open(const char *path);

/* Close an open file descriptor.
   Returns 0 on success, negative error on failure. */
int vfs_close(int fd);

/* Read up to len bytes from an open file into buf.
   Returns bytes read (0 = EOF), negative error on failure.
   NOTE: When called from a syscall, the caller MUST validate that
   buf points to valid user-writable memory before calling this. */
int vfs_read(int fd, void *buf, uint32_t len);

#endif /* LUMAOS_VFS_H */
