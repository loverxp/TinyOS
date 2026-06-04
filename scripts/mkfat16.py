#!/usr/bin/env python3
"""Create a 16MB FAT16 disk image with sample files for TinyOS."""

import struct
import sys
import os

SECTOR_SIZE = 512
TOTAL_SECTORS = 32768  # 16 MB
BYTES_PER_CLUSTER = 2048  # 4 sectors per cluster
SECTORS_PER_CLUSTER = BYTES_PER_CLUSTER // SECTOR_SIZE
RESERVED_SECTORS = 1
NUM_FATS = 2
ROOT_ENTRY_COUNT = 512
FAT_SIZE = 128  # sectors per FAT (enough for 16MB)

# Layout
FAT_START = RESERVED_SECTORS
ROOT_DIR_START = FAT_START + NUM_FATS * FAT_SIZE
ROOT_DIR_SECTORS = (ROOT_ENTRY_COUNT * 32 + SECTOR_SIZE - 1) // SECTOR_SIZE
DATA_START = ROOT_DIR_START + ROOT_DIR_SECTORS

def make_dos_name(name):
    """Convert 'hello.c' to 'HELLO   C  ' (11 bytes)."""
    parts = name.upper().split('.', 1)
    base = parts[0][:8].ljust(8)
    ext = (parts[1][:3] if len(parts) > 1 else '').ljust(3)
    return (base + ext).encode('ascii')

def make_dir_entry(name, attr, cluster, size):
    """Build a 32-byte directory entry."""
    dos_name = make_dos_name(name)
    return struct.pack('<11sB3sHHHHI',
        dos_name,       # name[11]
        attr,           # attributes
        b'\x00' * 3,    # reserved
        0,              # creation time
        0,              # creation date
        0,              # last access date
        0,              # first cluster high (FAT32 only)
        # skip 2 bytes (last write time/date handled below)
    ) + b'\x00' * 4 + struct.pack('<HI', 0, cluster, size)

def make_dir_entry_simple(name, attr, cluster, size):
    """Build a 32-byte directory entry (simplified)."""
    dos_name = make_dos_name(name)
    entry = bytearray(32)
    entry[0:11] = dos_name
    entry[11] = attr
    # first_cluster (offset 26, 2 bytes)
    struct.pack_into('<H', entry, 26, cluster)
    # file_size (offset 28, 4 bytes)
    struct.pack_into('<I', entry, 28, size)
    return bytes(entry)

def write_file_to_clusters(img, data, start_cluster, fat):
    """Write file data to clusters and update FAT chain."""
    cluster = start_cluster
    offset = 0
    prev_cluster = None

    while offset < len(data):
        lba = DATA_START + (cluster - 2) * SECTORS_PER_CLUSTER
        chunk = data[offset:offset + BYTES_PER_CLUSTER]
        # Pad to full cluster
        if len(chunk) < BYTES_PER_CLUSTER:
            chunk = chunk + b'\x00' * (BYTES_PER_CLUSTER - len(chunk))
        # Write cluster sectors
        for s in range(SECTORS_PER_CLUSTER):
            sector_offset = (lba + s) * SECTOR_SIZE
            img[sector_offset:sector_offset + SECTOR_SIZE] = chunk[s*SECTOR_SIZE:(s+1)*SECTOR_SIZE]

        next_offset = offset + BYTES_PER_CLUSTER
        if next_offset >= len(data):
            # Last cluster: mark end of chain
            fat[cluster] = 0xFFFF
        else:
            # Find next free cluster
            next_cluster = start_cluster + (next_offset // BYTES_PER_CLUSTER)
            fat[cluster] = next_cluster
        prev_cluster = cluster
        offset += BYTES_PER_CLUSTER
        cluster = fat.get(prev_cluster, 0xFFFF)
        if cluster == 0xFFFF:
            break

def create_image(output_path):
    # Create empty image
    img = bytearray(TOTAL_SECTORS * SECTOR_SIZE)

    # Build FAT (as a list indexed by cluster number)
    fat = {0: 0xFFF8, 1: 0xFFFF}  # Reserved clusters

    # Sample files to include
    files = [
        ("readme.txt", b"Welcome to TinyOS!\r\nThis is a FAT16 disk image.\r\n"),
        ("hello.c", b'#include <stdio.h>\r\nint main(void) {\r\n    printf("Hello from disk!\\n");\r\n    return 0;\r\n}\r\n'),
        ("config.txt", b"# TinyOS Configuration\r\nshell.prompt=TinyOS> \r\ntimer.hz=50\r\n"),
        ("test.txt", b"This is a test file on the FAT16 disk.\r\n" * 20),
    ]

    # Allocate clusters for each file
    next_cluster = 2
    file_entries = []
    for name, data in files:
        start_cluster = next_cluster
        num_clusters = (len(data) + BYTES_PER_CLUSTER - 1) // BYTES_PER_CLUSTER
        if num_clusters == 0:
            num_clusters = 1

        # Build FAT chain
        for i in range(num_clusters):
            c = start_cluster + i
            if i < num_clusters - 1:
                fat[c] = start_cluster + i + 1
            else:
                fat[c] = 0xFFFF

        # Write file data to clusters
        write_file_to_clusters(img, data, start_cluster, fat)

        file_entries.append((name, 0x20, start_cluster, len(data)))  # ARCHIVE attr
        next_cluster += num_clusters

    # Write FAT table (both copies)
    for copy in range(NUM_FATS):
        fat_offset = (FAT_START + copy * FAT_SIZE) * SECTOR_SIZE
        for cluster_num in range(FAT_SIZE * SECTOR_SIZE // 2):
            val = fat.get(cluster_num, 0x0000)
            struct.pack_into('<H', img, fat_offset + cluster_num * 2, val)

    # Write root directory
    root_offset = ROOT_DIR_START * SECTOR_SIZE
    entry_idx = 0

    # Volume label
    vol_entry = bytearray(32)
    vol_entry[0:11] = b'TINYOS     '
    vol_entry[11] = 0x08  # VOLUME_ID
    img[root_offset:root_offset + 32] = bytes(vol_entry)
    entry_idx += 1

    for name, attr, cluster, size in file_entries:
        entry = make_dir_entry_simple(name, attr, cluster, size)
        offset = root_offset + entry_idx * 32
        img[offset:offset + 32] = entry
        entry_idx += 1

    # Write boot sector (BPB)
    bpb = bytearray(SECTOR_SIZE)
    # Jump instruction
    bpb[0:3] = b'\xEB\x3C\x90'
    # OEM name
    bpb[3:11] = b'TINYOS  '
    # BPB
    struct.pack_into('<H', bpb, 11, SECTOR_SIZE)          # bytes per sector
    bpb[13] = SECTORS_PER_CLUSTER                           # sectors per cluster
    struct.pack_into('<H', bpb, 14, RESERVED_SECTORS)      # reserved sectors
    bpb[16] = NUM_FATS                                      # number of FATs
    struct.pack_into('<H', bpb, 17, ROOT_ENTRY_COUNT)      # root entry count
    struct.pack_into('<H', bpb, 19, TOTAL_SECTORS)         # total sectors (16-bit)
    bpb[21] = 0xF8                                         # media type (hard disk)
    struct.pack_into('<H', bpb, 22, FAT_SIZE)              # FAT size
    struct.pack_into('<H', bpb, 24, 63)                    # sectors per track
    struct.pack_into('<H', bpb, 26, 255)                   # number of heads
    struct.pack_into('<I', bpb, 28, 0)                     # hidden sectors
    struct.pack_into('<I', bpb, 32, 0)                     # total sectors (32-bit)
    bpb[36] = 0x80                                         # drive number
    bpb[38] = 0x29                                         # extended boot signature
    struct.pack_into('<I', bpb, 39, 0x12345678)            # volume serial
    bpb[43:54] = b'TINYOS DISK'                            # volume label
    bpb[54:62] = b'FAT16   '                               # filesystem type
    # Boot signature
    bpb[510] = 0x55
    bpb[511] = 0xAA

    img[0:SECTOR_SIZE] = bpb

    # Write image
    with open(output_path, 'wb') as f:
        f.write(img)

    total_size_kb = TOTAL_SECTORS * SECTOR_SIZE // 1024
    print(f"Created {output_path}: {total_size_kb} KB ({TOTAL_SECTORS} sectors)")
    print(f"  FAT16, {SECTORS_PER_CLUSTER} sectors/cluster, {len(files)} files")
    for name, _, cluster, size in file_entries:
        print(f"    {name:14s} {size:6d} bytes (cluster {cluster})")

if __name__ == '__main__':
    output = sys.argv[1] if len(sys.argv) > 1 else 'disk.img'
    create_image(output)
