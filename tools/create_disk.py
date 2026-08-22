#!/usr/bin/env python3
"""create_disk.py - Create a minimal FAT32 disk image for LumaOS Phase 6.

Creates a 64MB raw disk image with a FAT32 filesystem containing test files.
No external tools required - builds the FAT32 structures directly.

Usage: python3 tools/create_disk.py build/disk.img
"""

import struct
import sys
import os

# Disk parameters
DISK_SIZE_MB = 64
BYTES_PER_SECTOR = 512
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS = 32
NUM_FATS = 2
FAT_ENTRY_SIZE = 4  # 4 bytes per entry in FAT32

def create_disk(path):
    total_sectors = (DISK_SIZE_MB * 1024 * 1024) // BYTES_PER_SECTOR

    # Calculate FAT size (sectors per FAT)
    fat_size = 1
    while True:
        data_sectors = total_sectors - RESERVED_SECTORS - NUM_FATS * fat_size
        if data_sectors <= 0:
            print("Error: disk too small")
            sys.exit(1)
        clusters = data_sectors // SECTORS_PER_CLUSTER
        needed_fat_sectors = (clusters * FAT_ENTRY_SIZE + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR
        if needed_fat_sectors <= fat_size:
            break
        fat_size = needed_fat_sectors

    data_sectors = total_sectors - RESERVED_SECTORS - NUM_FATS * fat_size
    total_clusters = data_sectors // SECTORS_PER_CLUSTER
    data_start = RESERVED_SECTORS + NUM_FATS * fat_size

    print(f"Disk: {DISK_SIZE_MB}MB, {total_sectors} sectors")
    print(f"FAT: {fat_size} sectors x {NUM_FATS} copies = {fat_size * NUM_FATS} sectors")
    print(f"Data: {data_sectors} sectors, {total_clusters} clusters")
    print(f"Data start: sector {data_start}")
    print(f"Clusters: {total_clusters} (FAT32 requires >= 65525)")

    if total_clusters < 65525:
        print("Error: not enough clusters for FAT32")
        sys.exit(1)

    # Create disk image
    disk = bytearray(total_sectors * BYTES_PER_SECTOR)

    # ===== Boot sector / BPB (sector 0) =====
    disk[0:3] = b'\xEB\x58\x90'  # jmp short +0x58, nop
    disk[3:11] = b'MSDOS5.0'     # OEM name (8 bytes)

    struct.pack_into('<H', disk, 11, BYTES_PER_SECTOR)       # bytes per sector
    disk[13] = SECTORS_PER_CLUSTER                            # sectors per cluster
    struct.pack_into('<H', disk, 14, RESERVED_SECTORS)        # reserved sectors
    disk[16] = NUM_FATS                                       # number of FATs
    struct.pack_into('<H', disk, 17, 0)                      # root entry count (0 for FAT32)
    struct.pack_into('<H', disk, 19, 0)                       # total sectors 16-bit (0)
    struct.pack_into('<I', disk, 32, total_sectors)           # total sectors 32-bit
    struct.pack_into('<I', disk, 36, fat_size)               # FAT size 32
    struct.pack_into('<H', disk, 40, 0)                       # extended flags
    struct.pack_into('<H', disk, 42, 0)                       # FS version
    struct.pack_into('<I', disk, 44, 2)                        # root cluster
    struct.pack_into('<H', disk, 48, 1)                       # FS info sector
    struct.pack_into('<H', disk, 50, 6)                       # backup boot sector
    disk[64] = 0x80                                            # drive number
    disk[66] = 0x29                                            # extended boot signature
    struct.pack_into('<I', disk, 67, 0x12345678)              # volume serial
    disk[71:82] = b'LUMAOS DISK'                              # volume label (11 bytes)
    disk[82:90] = b'FAT32   '                                 # FS type (8 bytes)
    disk[510] = 0x55                                           # boot signature
    disk[511] = 0xAA

    # ===== FS Info sector (sector 1) =====
    fsinfo_off = 1 * BYTES_PER_SECTOR
    struct.pack_into('<I', disk, fsinfo_off + 0, 0x41615252)
    struct.pack_into('<I', disk, fsinfo_off + 484, 0x41615252)
    struct.pack_into('<I', disk, fsinfo_off + 488, 0xFFFFFFFF)
    struct.pack_into('<I', disk, fsinfo_off + 492, 0xFFFFFFFF)
    struct.pack_into('<I', disk, fsinfo_off + 508, 0xAA550000)

    # ===== FAT tables (2 copies) =====
    fat_start = RESERVED_SECTORS
    fat = bytearray(fat_size * BYTES_PER_SECTOR)

    # FAT32 entries: 4 bytes each, little-endian
    struct.pack_into('<I', fat, 0 * 4, 0x0FFFFFF8)  # entry 0: media type
    struct.pack_into('<I', fat, 1 * 4, 0x0FFFFFFF)  # entry 1: EOC
    struct.pack_into('<I', fat, 2 * 4, 0x0FFFFFFF)  # entry 2: root dir (EOC, 1 cluster)
    struct.pack_into('<I', fat, 3 * 4, 0x0FFFFFFF)  # entry 3: hello.txt (EOC, 1 cluster)
    struct.pack_into('<I', fat, 4 * 4, 0x0FFFFFFF)  # entry 4: test.txt (EOC, 1 cluster)

    for i in range(NUM_FATS):
        offset = (fat_start + i * fat_size) * BYTES_PER_SECTOR
        disk[offset:offset + len(fat)] = fat

    # ===== Root directory (cluster 2) =====
    root_dir_sector = data_start
    root_offset = root_dir_sector * BYTES_PER_SECTOR

    # hello.txt
    hello_data = b"Hello from LumaOS!\n"
    hello_size = len(hello_data)

    entry = bytearray(32)
    entry[0:11] = b'HELLO   TXT'
    entry[11] = 0x20  # attr: archive
    struct.pack_into('<H', entry, 14, 0x8000)  # creation time
    struct.pack_into('<H', entry, 16, 0x4A21)  # creation date
    struct.pack_into('<H', entry, 18, 0x4A21)  # last access date
    struct.pack_into('<H', entry, 20, 0)       # first cluster high
    struct.pack_into('<H', entry, 22, 0x8000)  # write time
    struct.pack_into('<H', entry, 24, 0x4A21)  # write date
    struct.pack_into('<H', entry, 26, 3)       # first cluster low
    struct.pack_into('<I', entry, 28, hello_size)
    disk[root_offset:root_offset + 32] = entry

    # test.txt
    test_data = b"This is a test file.\nLine 2\n"
    test_size = len(test_data)

    entry2 = bytearray(32)
    entry2[0:11] = b'TEST    TXT'
    entry2[11] = 0x20
    struct.pack_into('<H', entry2, 14, 0x8000)
    struct.pack_into('<H', entry2, 16, 0x4A21)
    struct.pack_into('<H', entry2, 18, 0x4A21)
    struct.pack_into('<H', entry2, 20, 0)
    struct.pack_into('<H', entry2, 22, 0x8000)
    struct.pack_into('<H', entry2, 24, 0x4A21)
    struct.pack_into('<H', entry2, 26, 4)
    struct.pack_into('<I', entry2, 28, test_size)
    disk[root_offset + 32:root_offset + 64] = entry2

    # End-of-directory marker
    disk[root_offset + 64] = 0x00

    # ===== File data =====
    # Cluster 3: hello.txt
    cl3_off = (data_start + (3 - 2) * SECTORS_PER_CLUSTER) * BYTES_PER_SECTOR
    disk[cl3_off:cl3_off + hello_size] = hello_data

    # Cluster 4: test.txt
    cl4_off = (data_start + (4 - 2) * SECTORS_PER_CLUSTER) * BYTES_PER_SECTOR
    disk[cl4_off:cl4_off + test_size] = test_data

    # Write
    os.makedirs(os.path.dirname(path) if os.path.dirname(path) else '.', exist_ok=True)
    with open(path, 'wb') as f:
        f.write(disk)

    print(f"\nCreated: {path} ({DISK_SIZE_MB}MB)")
    print(f"Files: hello.txt ({hello_size}B), test.txt ({test_size}B)")
    print(f"Root dir: sector {root_dir_sector} (cluster 2)")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <output_path>")
        sys.exit(1)
    create_disk(sys.argv[1])
