#include "../include/fat16.h"
#include "../include/ata.h"
#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/rtc.h"

static fat16_bpb_t bpb;
static int fat16_ready = 0;

/* Sector-sized scratch buffer */
static uint8_t sector_buf[512] __attribute__((aligned(4)));

/* Read a single sector into sector_buf */
static int read_sector(uint32_t lba) {
    return ata_read_sectors(lba, 1, sector_buf);
}

/* Convert an 8.3 DOS filename to a human-readable string.
 * Input:  "HELLO   C  " (11 bytes, space-padded)
 * Output: "HELLO.C" (null-terminated) */
static void fat16_name_to_str(const char* name11, char* out) {
    int i = 0;
    /* Copy base name (up to 8 chars, trim trailing spaces) */
    int base_len = 8;
    while (base_len > 0 && name11[base_len - 1] == ' ') base_len--;
    for (int j = 0; j < base_len; j++) {
        out[i++] = name11[j];
    }
    /* Copy extension if present */
    int ext_len = 3;
    while (ext_len > 0 && name11[8 + ext_len - 1] == ' ') ext_len--;
    if (ext_len > 0) {
        out[i++] = '.';
        for (int j = 0; j < ext_len; j++) {
            out[i++] = name11[8 + j];
        }
    }
    out[i] = '\0';
}

/* Convert user filename to 8.3 DOS format for comparison.
 * Input: "hello.c" -> Output: "HELLO   C  " */
static void str_to_fat16_name(const char* name, char* out11) {
    memset(out11, ' ', 11);
    int i = 0;
    /* Base name */
    while (*name && *name != '.' && i < 8) {
        char c = *name;
        if (c >= 'a' && c <= 'z') c -= 32;  /* uppercase */
        out11[i++] = c;
        name++;
    }
    /* Extension */
    if (*name == '.') {
        name++;
        i = 8;
        while (*name && i < 11) {
            char c = *name;
            if (c >= 'a' && c <= 'z') c -= 32;
            out11[i++] = c;
            name++;
        }
    }
}

/* Get the LBA of a given cluster number */
static uint32_t cluster_to_lba(uint16_t cluster) {
    return bpb.data_start + (uint32_t)(cluster - 2) * bpb.sectors_per_cluster;
}

/* Read next cluster from FAT table */
static uint16_t fat16_next_cluster(uint16_t cluster) {
    /* Each FAT entry is 2 bytes (16 bits) for FAT16 */
    uint32_t fat_offset = (uint32_t)cluster * 2;
    uint32_t fat_sector = bpb.fat_start + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    if (ata_read_sectors(fat_sector, 1, sector_buf) < 0)
        return 0xFFFF;

    uint16_t* fat = (uint16_t*)sector_buf;
    return fat[entry_offset / 2];
}

int fat16_init(void) {
    if (!ata_disk_present()) {
        printf("[FAT16] No disk\n");
        return -1;
    }

    /* Read boot sector (sector 0) */
    if (read_sector(0) < 0) {
        printf("[FAT16] Cannot read boot sector\n");
        return -1;
    }

    /* Parse BPB from boot sector */
    uint8_t* bsb = sector_buf;  /* Boot Sector Bytes */
    bpb.bytes_per_sector   = *(uint16_t*)(bsb + 11);
    bpb.sectors_per_cluster = *(uint8_t*)(bsb + 13);
    bpb.reserved_sectors   = *(uint16_t*)(bsb + 14);
    bpb.num_fats           = *(uint8_t*)(bsb + 16);
    bpb.root_entry_count   = *(uint16_t*)(bsb + 17);
    bpb.total_sectors_16   = *(uint16_t*)(bsb + 19);
    bpb.media_type         = *(uint8_t*)(bsb + 21);
    bpb.fat_size_16        = *(uint16_t*)(bsb + 22);
    bpb.sectors_per_track  = *(uint16_t*)(bsb + 24);
    bpb.num_heads          = *(uint16_t*)(bsb + 26);
    bpb.total_sectors_32   = *(uint32_t*)(bsb + 32);

    /* Validate */
    if (bpb.bytes_per_sector != 512) {
        printf("[FAT16] Unsupported sector size: %u\n", bpb.bytes_per_sector);
        return -1;
    }
    if (bpb.sectors_per_cluster == 0) {
        printf("[FAT16] Invalid sectors per cluster\n");
        return -1;
    }

    /* Compute derived values */
    uint32_t total_sectors = bpb.total_sectors_16 ?
                             bpb.total_sectors_16 : bpb.total_sectors_32;

    bpb.root_dir_sectors = ((uint32_t)bpb.root_entry_count * 32 + 511) / 512;
    bpb.fat_start = bpb.reserved_sectors;
    bpb.root_dir_start = bpb.fat_start + (uint32_t)bpb.num_fats * bpb.fat_size_16;
    bpb.data_start = bpb.root_dir_start + bpb.root_dir_sectors;
    bpb.data_sectors = total_sectors - bpb.data_start;
    bpb.total_clusters = bpb.data_sectors / bpb.sectors_per_cluster;
    bpb.total_size = total_sectors * bpb.bytes_per_sector;

    /* FAT16 has 4085-65525 clusters */
    if (bpb.total_clusters < 4085 || bpb.total_clusters > 65525) {
        printf("[FAT16] Cluster count %u not in FAT16 range\n", bpb.total_clusters);
        return -1;
    }

    fat16_ready = 1;

    serial_printf("[FAT16] Init OK: %u sectors, %u clusters, %u bytes/cluster, %u KB\n",
                  total_sectors, bpb.total_clusters,
                  bpb.sectors_per_cluster * 512,
                  bpb.total_size / 1024);
    serial_printf("[FAT16] FAT at LBA %u, root dir at LBA %u, data at LBA %u\n",
                  bpb.fat_start, bpb.root_dir_start, bpb.data_start);

    return 0;
}

const fat16_bpb_t* fat16_get_bpb(void) {
    return fat16_ready ? &bpb : NULL;
}

/* ── Write helpers ──────────────────────────────────────────────── */

/* Read file data from a directory entry */
uint32_t fat16_read(const fat16_entry_t* entry, uint32_t offset,
                    void* buffer, uint32_t size) {
    if (!fat16_ready || !entry || size == 0) return 0;

    /* Clamp to file size */
    if (offset >= entry->file_size) return 0;
    if (offset + size > entry->file_size)
        size = entry->file_size - offset;

    uint8_t* dst = (uint8_t*)buffer;
    uint32_t cluster_size = (uint32_t)bpb.sectors_per_cluster * 512;
    uint16_t cluster = entry->first_cluster;
    uint32_t bytes_read = 0;

    /* Skip clusters before the offset */
    uint32_t skip_clusters = offset / cluster_size;
    for (uint32_t i = 0; i < skip_clusters; i++) {
        cluster = fat16_next_cluster(cluster);
        if (cluster >= 0xFFF8) return bytes_read;
    }

    /* Read from offset within first cluster */
    uint32_t cluster_offset = offset % cluster_size;

    while (bytes_read < size && cluster < 0xFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        uint32_t to_read = cluster_size - cluster_offset;
        if (to_read > size - bytes_read)
            to_read = size - bytes_read;

        uint32_t sec_in_cluster = cluster_offset / 512;
        uint32_t off_in_sector = cluster_offset % 512;

        uint32_t remaining = to_read;
        while (remaining > 0) {
            if (ata_read_sectors(lba + sec_in_cluster, 1, sector_buf) < 0)
                return bytes_read;

            uint32_t copy_len = 512 - off_in_sector;
            if (copy_len > remaining) copy_len = remaining;

            memcpy(dst + bytes_read, sector_buf + off_in_sector, copy_len);
            bytes_read += copy_len;
            remaining -= copy_len;
            sec_in_cluster++;
            off_in_sector = 0;
        }

        cluster_offset = 0;
        cluster = fat16_next_cluster(cluster);
    }

    return bytes_read;
}

/* Write a 2-byte FAT entry for the given cluster */
static int fat16_set_fat_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = (uint32_t)cluster * 2;
    uint32_t fat_sector = bpb.fat_start + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    if (ata_read_sectors(fat_sector, 1, sector_buf) < 0) return -1;
    uint16_t* fat = (uint16_t*)sector_buf;
    fat[entry_offset / 2] = value;
    if (ata_write_sectors(fat_sector, 1, sector_buf) < 0) return -1;

    /* Write to all FAT copies */
    for (uint8_t f = 1; f < bpb.num_fats; f++) {
        uint32_t copy_sector = fat_sector + (uint32_t)f * bpb.fat_size_16;
        if (ata_write_sectors(copy_sector, 1, sector_buf) < 0) return -1;
    }
    return 0;
}

/* Read a FAT entry (already have fat16_next_cluster, reuse it) */
static uint16_t fat16_get_fat_entry(uint16_t cluster) {
    return fat16_next_cluster(cluster);
}

/* Find and allocate a free cluster. Returns 0 on failure. */
static uint16_t fat16_alloc_cluster(void) {
    /* Clusters 2..total_clusters+1 are valid */
    for (uint32_t c = 2; c < bpb.total_clusters + 2; c++) {
        uint16_t val = fat16_get_fat_entry((uint16_t)c);
        if (val == 0x0000) {
            /* Mark as end-of-chain */
            if (fat16_set_fat_entry((uint16_t)c, 0xFFFF) < 0) return 0;
            return (uint16_t)c;
        }
    }
    return 0;
}

/* Free an entire cluster chain starting from 'start' */
static int fat16_free_chain(uint16_t start) {
    uint16_t cluster = start;
    while (cluster >= 2 && cluster < 0xFFF8) {
        uint16_t next = fat16_get_fat_entry(cluster);
        fat16_set_fat_entry(cluster, 0x0000);
        cluster = next;
    }
    return 0;
}

/* Build DOS date/time from RTC. Returns packed (date<<16 | time). */
static void fat16_make_datetime(uint16_t* out_time, uint16_t* out_date) {
    rtc_time_t tm;
    rtc_read_time(&tm);
    /* DOS time: (hour<<11) | (minute<<5) | (second/2) */
    *out_time = ((uint16_t)tm.hour << 11) |
                ((uint16_t)tm.minute << 5) |
                ((uint16_t)(tm.second / 2));
    /* DOS date: ((year-1980)<<9) | (month<<5) | day */
    *out_date = ((uint16_t)(tm.year - 1980) << 9) |
                ((uint16_t)tm.month << 5) |
                (uint16_t)tm.day;
}

/* ── Subdirectory Support ─────────────────────────────────────────── */

/* Read a directory entry from any directory.
 * dir_cluster=0 means root directory (fixed sectors).
 * For subdirectories, follows the cluster chain.
 * 'entry_index' is the 0-based entry number within the directory.
 * Returns 0 on success, -1 if entry doesn't exist.
 */
static int read_dir_entry(uint16_t dir_cluster, uint32_t entry_index,
                           fat16_entry_t* out) {
    uint32_t entries_per_sector = 512 / 32;
    uint32_t cluster_size = (uint32_t)bpb.sectors_per_cluster * 512;
    uint32_t entries_per_cluster = cluster_size / 32;

    if (dir_cluster == 0) {
        /* Root directory: contiguous sectors, fixed size */
        uint32_t ent_per_sector = 512 / 32;
        uint32_t sector_idx = entry_index / ent_per_sector;
        uint32_t ent_in_sector = entry_index % ent_per_sector;

        if (sector_idx >= bpb.root_dir_sectors) return -1;

        if (ata_read_sectors(bpb.root_dir_start + sector_idx, 1, sector_buf) < 0)
            return -1;

        fat16_entry_t* entries = (fat16_entry_t*)sector_buf;
        memcpy(out, &entries[ent_in_sector], sizeof(fat16_entry_t));
        return 0;
    } else {
        /* Subdirectory: follow cluster chain */
        uint16_t cluster = dir_cluster;
        while (cluster >= 2 && cluster < 0xFFF8) {
            /* Check if entry is in this cluster */
            if (entry_index < entries_per_cluster) {
                uint32_t lba = cluster_to_lba(cluster);
                uint32_t ent_in_cluster = entry_index;
                uint32_t sector_idx = (ent_in_cluster * 32) / 512;
                uint32_t ent_in_sector = ent_in_cluster % (512 / 32);

                if (ata_read_sectors(lba + sector_idx, 1, sector_buf) < 0)
                    return -1;

                fat16_entry_t* entries = (fat16_entry_t*)sector_buf;
                memcpy(out, &entries[ent_in_sector], sizeof(fat16_entry_t));
                return 0;
            }
            entry_index -= entries_per_cluster;
            cluster = fat16_next_cluster(cluster);
        }
        return -1;
    }
}

/* Find a directory entry by name within a specific directory.
 * dir_cluster=0 means root directory.
 * Returns 0 on success, -1 if not found.
 */
static int fat16_find_entry_in_dir(uint16_t dir_cluster, const char* name,
                                    fat16_entry_t* out) {
    char fat_name[11];
    str_to_fat16_name(name, fat_name);

    uint32_t max_entries;

    if (dir_cluster == 0) {
        max_entries = (bpb.root_dir_sectors * 512) / 32;
    } else {
        /* Subdirectories can grow; scan until end-of-chain or empty entry.
         * We'll scan entry by entry via read_dir_entry. */
        max_entries = 0xFFFFFF; /* effectively unlimited */
    }

    for (uint32_t i = 0; i < max_entries; i++) {
        if (read_dir_entry(dir_cluster, i, out) < 0) break;

        /* End of directory */
        if (out->name[0] == 0x00) break;
        /* Deleted entry */
        if ((uint8_t)out->name[0] == 0xE5) continue;
        /* Skip LFN */
        if (out->attr == FAT16_ATTR_LFN) continue;

        if (memcmp(out->name, fat_name, 11) == 0) {
            return 0;
        }

        if (dir_cluster != 0 && i >= 65534) break; /* safety limit */
    }

    return -1; /* Not found */
}

/* Traverse a path string and resolve to parent directory + filename.
 * Input:  "DIR1/SUBDIR/FILE.TXT"
 * Output: parent_cluster = cluster of "DIR1/SUBDIR" (0 for root),
 *         out_name = "FILE.TXT"
 * Returns 0 on success, -1 if a directory in the path doesn't exist.
 */
static int fat16_resolve_path(const char* path, uint16_t* parent_cluster,
                               char* out_name) {
    /* Skip leading slashes */
    while (*path == '/') path++;
    if (*path == '\0') return -1;

    uint16_t current_cluster = 0; /* start at root */
    char component[13];
    int comp_idx = 0;

    while (1) {
        /* Extract next path component */
        comp_idx = 0;
        while (*path && *path != '/' && comp_idx < 12) {
            component[comp_idx++] = *path++;
        }
        component[comp_idx] = '\0';
        if (comp_idx == 0) return -1;

        /* Skip slashes */
        while (*path == '/') path++;

        if (*path == '\0') {
            /* This is the final component (filename or last dir) */
            strcpy(out_name, component);
            *parent_cluster = current_cluster;
            return 0;
        }

        /* Intermediate component: must be a directory */
        fat16_entry_t entry;
        if (fat16_find_entry_in_dir(current_cluster, component, &entry) < 0) {
            return -1; /* directory not found */
        }
        if (!(entry.attr & FAT16_ATTR_DIRECTORY)) {
            return -1; /* not a directory */
        }
        current_cluster = entry.first_cluster;

        if (current_cluster == 0) {
            /* Entry is in root dir, but its cluster 0 means it IS root.
             * In FAT16, directory entries with cluster 0 and directory attr
             * should have a valid cluster. If cluster is 0, it's the root. */
            current_cluster = 0;
        }
    }
}

/* ── Modified fat16_find (supports subdirectories) ─── */

int fat16_find(const char* name, fat16_entry_t* out) {
    if (!fat16_ready) return -1;

    /* Check if name contains a path separator */
    const char* slash = name;
    while (*slash) {
        if (*slash == '/') {
            /* Has subdirectory path */
            uint16_t parent_cluster;
            char fname[13];
            if (fat16_resolve_path(name, &parent_cluster, fname) < 0)
                return -1;
            return fat16_find_entry_in_dir(parent_cluster, fname, out);
        }
        slash++;
    }

    /* Simple name: search root directory (original behavior) */
    char fat_name[11];
    str_to_fat16_name(name, fat_name);

    uint32_t entries_per_sector = 512 / 32;
    uint32_t total_dir_sectors = bpb.root_dir_sectors;

    for (uint32_t s = 0; s < total_dir_sectors; s++) {
        if (ata_read_sectors(bpb.root_dir_start + s, 1, sector_buf) < 0)
            return -1;

        fat16_entry_t* entries = (fat16_entry_t*)sector_buf;
        for (uint32_t e = 0; e < entries_per_sector; e++) {
            if (entries[e].name[0] == 0x00) return -1;
            if ((uint8_t)entries[e].name[0] == 0xE5) continue;
            if (entries[e].attr == FAT16_ATTR_LFN) continue;

            if (memcmp(entries[e].name, fat_name, 11) == 0) {
                memcpy(out, &entries[e], sizeof(fat16_entry_t));
                return 0;
            }
        }
    }

    return -1;  /* Not found */
}

/* ── Modified fat16_list (supports paths, uses fat16_list_dir) ── */

int fat16_list(void) {
    /* Default: list root directory */
    return fat16_list_dir("");
}

int fat16_list_dir(const char* path) {
    if (!fat16_ready) {
        printf("FAT16 not initialized\n");
        return -1;
    }

    uint16_t dir_cluster = 0;

    /* If path is non-empty, resolve it */
    if (path && *path && *path != '\0') {
        /* Remove trailing slash */
        char buf[256];
        strcpy(buf, path);
        int len = strlen(buf);
        while (len > 0 && buf[len-1] == '/') buf[--len] = '\0';

        /* Find the directory */
        fat16_entry_t entry;
        if (fat16_find(buf, &entry) < 0) {
            printf("Directory not found: %s\n", buf);
            return -1;
        }
        if (!(entry.attr & FAT16_ATTR_DIRECTORY)) {
            printf("Not a directory: %s\n", buf);
            return -1;
        }
        dir_cluster = entry.first_cluster;
    }

    /* Print header */
    printf("Directory listing");
    if (path && *path) printf(": %s", path);
    printf("\n");
    printf("%-14s %10s  %s\n", "Name", "Size", "Type");
    printf("--------------------------------------\n");

    int count = 0;
    uint32_t max_entries;
    if (dir_cluster == 0) {
        max_entries = (bpb.root_dir_sectors * 512) / 32;
    } else {
        max_entries = 65534;
    }

    for (uint32_t i = 0; i < max_entries; i++) {
        fat16_entry_t entry;
        if (read_dir_entry(dir_cluster, i, &entry) < 0) break;

        if (entry.name[0] == 0x00) break;
        if ((uint8_t)entry.name[0] == 0xE5) continue;
        if (entry.attr == FAT16_ATTR_LFN) continue;

        /* Skip . and .. entries */
        if (entry.name[0] == '.' && (entry.name[1] == ' ' || entry.name[1] == '.'))
            continue;

        char display_name[14];
        fat16_name_to_str(entry.name, display_name);

        const char* type = "FILE";
        if (entry.attr & FAT16_ATTR_DIRECTORY) type = "DIR";
        if (entry.attr & FAT16_ATTR_VOLUME_ID) type = "VOL";

        printf("%-14s %10u  %s\n", display_name, entry.file_size, type);
        count++;
    }

    printf("\n%d entries\n", count);
    return count;
}

/* ── Find free entry in a directory (for write/mkdir) ── */

/* Find a free directory entry slot in a directory.
 * dir_cluster=0 for root.
 * Returns the entry index and reads the sector into the provided buffer.
 * On success, *out_lba and *out_idx are set, and the sector is in dir_buf.
 */
static int fat16_find_free_entry(uint16_t dir_cluster,
                                  uint32_t* out_idx,
                                  uint32_t* out_lba,
                                  uint8_t* dir_buf) {
    uint32_t entries_per_sector = 512 / 32;

    if (dir_cluster == 0) {
        /* Root directory: scan fixed sectors */
        for (uint32_t s = 0; s < bpb.root_dir_sectors; s++) {
            uint32_t lba = bpb.root_dir_start + s;
            if (ata_read_sectors(lba, 1, dir_buf) < 0) return -1;

            fat16_entry_t* entries = (fat16_entry_t*)dir_buf;
            for (uint32_t e = 0; e < entries_per_sector; e++) {
                if (entries[e].name[0] == 0x00 || (uint8_t)entries[e].name[0] == 0xE5) {
                    *out_idx = e;
                    *out_lba = lba;
                    return 0;
                }
            }
        }
    } else {
        /* Subdirectory: follow cluster chain, extend if needed */
        uint16_t cluster = dir_cluster;
        while (1) {
            uint32_t lba_start = cluster_to_lba(cluster);
            for (uint32_t sec = 0; sec < bpb.sectors_per_cluster; sec++) {
                uint32_t lba = lba_start + sec;
                if (ata_read_sectors(lba, 1, dir_buf) < 0) return -1;

                fat16_entry_t* entries = (fat16_entry_t*)dir_buf;
                for (uint32_t e = 0; e < entries_per_sector; e++) {
                    if (entries[e].name[0] == 0x00 || (uint8_t)entries[e].name[0] == 0xE5) {
                        *out_idx = e;
                        *out_lba = lba;
                        return 0;
                    }
                }
            }

            /* Try next cluster */
            uint16_t next = fat16_next_cluster(cluster);
            if (next >= 0xFFF8) {
                /* Need to extend directory - allocate a new cluster */
                uint16_t new_cluster = fat16_alloc_cluster();
                if (new_cluster == 0) return -1;

                /* Link current cluster to new one */
                fat16_set_fat_entry(cluster, new_cluster);

                /* Zero-fill the new cluster */
                uint8_t zero_buf[512] __attribute__((aligned(4)));
                memset(zero_buf, 0, 512);
                uint32_t new_lba = cluster_to_lba(new_cluster);
                for (uint32_t sec = 0; sec < bpb.sectors_per_cluster; sec++) {
                    ata_write_sectors(new_lba + sec, 1, zero_buf);
                }

                /* First entry of new cluster is free */
                if (ata_read_sectors(new_lba, 1, dir_buf) < 0) return -1;
                *out_idx = 0;
                *out_lba = new_lba;
                return 0;
            }
            cluster = next;
        }
    }

    return -1; /* directory full */
}

/* ── fat16_write (modified to support subdirectories) ── */

int fat16_write(const char* name, const void* data, uint32_t size) {
    if (!fat16_ready) return -1;
    if (size == 0) return -1;

    uint16_t parent_cluster = 0;
    char fname[13];

    /* Check if path contains directory */
    const char* slash = name;
    while (*slash) {
        if (*slash == '/') {
            if (fat16_resolve_path(name, &parent_cluster, fname) < 0) {
                printf("Directory not found\n");
                return -1;
            }
            goto do_write;
        }
        slash++;
    }
    strcpy(fname, name);

do_write:
    char fat_name[11];
    str_to_fat16_name(fname, fat_name);

    /* Check if file already exists in the target directory; if so, delete it first */
    fat16_entry_t existing;
    if (fat16_find_entry_in_dir(parent_cluster, fname, &existing) == 0) {
        /* Delete existing file */
        if (existing.first_cluster >= 2) {
            fat16_free_chain(existing.first_cluster);
        }
        /* Mark entry as deleted - read sector and mark it */
        /* We'll just overwrite it below during find_free_entry */
    }

    /* Find a free directory entry */
    uint8_t dir_buf[512] __attribute__((aligned(4)));
    uint32_t dir_idx, dir_lba;
    if (fat16_find_free_entry(parent_cluster, &dir_idx, &dir_lba, dir_buf) < 0) {
        serial_printf("[FAT16] No free directory entry\n");
        return -1;
    }

    /* Allocate clusters for the data */
    uint32_t cluster_size = (uint32_t)bpb.sectors_per_cluster * 512;
    uint32_t clusters_needed = (size + cluster_size - 1) / cluster_size;

    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;

    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint16_t c = fat16_alloc_cluster();
        if (c == 0) {
            serial_printf("[FAT16] Out of disk space at cluster %u/%u\n",
                          i, clusters_needed);
            if (first_cluster) fat16_free_chain(first_cluster);
            return -1;
        }
        if (i == 0) {
            first_cluster = c;
        } else {
            fat16_set_fat_entry(prev_cluster, c);
        }
        prev_cluster = c;
    }

    /* Write data cluster by cluster */
    const uint8_t* src = (const uint8_t*)data;
    uint32_t remaining = size;
    uint16_t cluster = first_cluster;

    while (remaining > 0 && cluster >= 2 && cluster < 0xFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        uint8_t cluster_buf[512];
        for (uint32_t sec = 0; sec < bpb.sectors_per_cluster; sec++) {
            uint32_t to_write = remaining;
            if (to_write > 512) to_write = 512;

            if (to_write < 512) {
                memset(cluster_buf, 0, 512);
                memcpy(cluster_buf, src, to_write);
                if (ata_write_sectors(lba + sec, 1, cluster_buf) < 0) {
                    fat16_free_chain(first_cluster);
                    return -1;
                }
            } else {
                if (ata_write_sectors(lba + sec, 1, src) < 0) {
                    fat16_free_chain(first_cluster);
                    return -1;
                }
            }
            src += to_write;
            remaining -= to_write;
            if (remaining == 0) break;
        }
        cluster = fat16_get_fat_entry(cluster);
    }

    /* Write the directory entry */
    if (ata_read_sectors(dir_lba, 1, dir_buf) < 0) {
        fat16_free_chain(first_cluster);
        return -1;
    }

    fat16_entry_t* entries = (fat16_entry_t*)dir_buf;
    fat16_entry_t* new_entry = &entries[dir_idx];

    memset(new_entry, 0, sizeof(fat16_entry_t));
    memcpy(new_entry->name, fat_name, 11);
    new_entry->attr = FAT16_ATTR_ARCHIVE;
    new_entry->first_cluster = first_cluster;
    new_entry->file_size = size;

    uint16_t dos_time, dos_date;
    fat16_make_datetime(&dos_time, &dos_date);
    new_entry->crt_time = dos_time;
    new_entry->crt_date = dos_date;
    new_entry->wrt_time = dos_time;
    new_entry->wrt_date = dos_date;

    if (ata_write_sectors(dir_lba, 1, dir_buf) < 0) {
        fat16_free_chain(first_cluster);
        return -1;
    }

    serial_printf("[FAT16] Wrote '%s': %u bytes, %u clusters\n",
                  fname, size, clusters_needed);
    return 0;
}

/* ── fat16_delete (modified to support subdirectories) ── */

int fat16_delete(const char* name) {
    if (!fat16_ready) return -1;

    uint16_t parent_cluster = 0;
    char fname[13];

    /* Check if path contains directory */
    const char* slash = name;
    while (*slash) {
        if (*slash == '/') {
            if (fat16_resolve_path(name, &parent_cluster, fname) < 0)
                return -1;
            goto do_delete;
        }
        slash++;
    }
    strcpy(fname, name);

do_delete:
    char fat_name[11];
    str_to_fat16_name(fname, fat_name);

    uint32_t max_entries;
    if (parent_cluster == 0) {
        max_entries = (bpb.root_dir_sectors * 512) / 32;
    } else {
        max_entries = 65534;
    }

    for (uint32_t i = 0; i < max_entries; i++) {
        fat16_entry_t entry;
        if (read_dir_entry(parent_cluster, i, &entry) < 0) return -1;

        if (entry.name[0] == 0x00) return -1;
        if ((uint8_t)entry.name[0] == 0xE5) continue;
        if (entry.attr == FAT16_ATTR_LFN) continue;

        if (memcmp(entry.name, fat_name, 11) == 0) {
            /* Verify it's not a directory (use rmdir for directories) */
            if (entry.attr & FAT16_ATTR_DIRECTORY) {
                printf("Is a directory, use 'rmdir'\n");
                return -1;
            }

            /* Free cluster chain */
            if (entry.first_cluster >= 2) {
                fat16_free_chain(entry.first_cluster);
            }

            /* Read sector, mark entry as deleted, write back */
            uint32_t entries_per_sector = 512 / 32;
            uint32_t sector_idx;
            if (parent_cluster == 0) {
                sector_idx = bpb.root_dir_start + (i / entries_per_sector);
            } else {
                /* For subdirectories, need to find the right sector */
                uint32_t cluster_size_entries = ((uint32_t)bpb.sectors_per_cluster * 512) / 32;
                uint16_t cluster = parent_cluster;
                uint32_t remaining = i;
                while (cluster >= 2 && cluster < 0xFFF8) {
                    if (remaining < cluster_size_entries) {
                        uint32_t sec_in_cluster = (remaining * 32) / 512;
                        sector_idx = cluster_to_lba(cluster) + sec_in_cluster;
                        goto found_sector;
                    }
                    remaining -= cluster_size_entries;
                    cluster = fat16_next_cluster(cluster);
                }
                return -1;
            }
found_sector:
            if (ata_read_sectors(sector_idx, 1, sector_buf) < 0) return -1;
            fat16_entry_t* ents = (fat16_entry_t*)sector_buf;
            ents[i % (512 / 32)].name[0] = (char)0xE5;
            if (ata_write_sectors(sector_idx, 1, sector_buf) < 0) return -1;
            return 0;
        }
    }
    return -1;
}

/* ── fat16_mkdir ── */

int fat16_mkdir(const char* name) {
    if (!fat16_ready) return -1;

    uint16_t parent_cluster = 0;
    char dirname[13];

    const char* slash = name;
    while (*slash) {
        if (*slash == '/') {
            if (fat16_resolve_path(name, &parent_cluster, dirname) < 0) {
                printf("Parent directory not found\n");
                return -1;
            }
            goto do_mkdir;
        }
        slash++;
    }
    strcpy(dirname, name);

do_mkdir:
    /* Check if already exists */
    fat16_entry_t existing;
    if (fat16_find_entry_in_dir(parent_cluster, dirname, &existing) == 0) {
        printf("Already exists: %s\n", dirname);
        return -1;
    }

    char fat_name[11];
    str_to_fat16_name(dirname, fat_name);
    /* Make sure it looks like a directory name (all caps, no extension typically) */
    /* 8.3 name: directories conventionally have no extension */

    /* Allocate a cluster for the directory */
    uint16_t cluster = fat16_alloc_cluster();
    if (cluster == 0) {
        printf("Disk full\n");
        return -1;
    }

    /* Zero-fill the cluster */
    uint8_t zero_buf[512] __attribute__((aligned(4)));
    memset(zero_buf, 0, 512);
    uint32_t cluster_lba = cluster_to_lba(cluster);
    for (uint32_t sec = 0; sec < bpb.sectors_per_cluster; sec++) {
        ata_write_sectors(cluster_lba + sec, 1, zero_buf);
    }

    /* Create "." entry */
    fat16_entry_t dot_entry;
    memset(&dot_entry, 0, sizeof(dot_entry));
    memset(dot_entry.name, ' ', 11);
    dot_entry.name[0] = '.';
    dot_entry.attr = FAT16_ATTR_DIRECTORY;
    dot_entry.first_cluster = cluster;
    uint16_t dos_time, dos_date;
    fat16_make_datetime(&dos_time, &dos_date);
    dot_entry.crt_time = dos_time;
    dot_entry.crt_date = dos_date;
    dot_entry.wrt_time = dos_time;
    dot_entry.wrt_date = dos_date;

    /* Create ".." entry */
    fat16_entry_t dotdot_entry;
    memset(&dotdot_entry, 0, sizeof(dotdot_entry));
    memset(dotdot_entry.name, ' ', 11);
    dotdot_entry.name[0] = '.';
    dotdot_entry.name[1] = '.';
    dotdot_entry.attr = FAT16_ATTR_DIRECTORY;
    dotdot_entry.first_cluster = parent_cluster; /* 0 for root parent */
    dotdot_entry.crt_time = dos_time;
    dotdot_entry.crt_date = dos_date;
    dotdot_entry.wrt_time = dos_time;
    dotdot_entry.wrt_date = dos_date;

    /* Write "." and ".." to the first sector of the cluster */
    fat16_entry_t* dir_entries = (fat16_entry_t*)zero_buf;
    memcpy(&dir_entries[0], &dot_entry, sizeof(fat16_entry_t));
    memcpy(&dir_entries[1], &dotdot_entry, sizeof(fat16_entry_t));
    ata_write_sectors(cluster_lba, 1, zero_buf);

    /* Find a free entry in parent directory and add the directory entry */
    uint8_t dir_buf[512] __attribute__((aligned(4)));
    uint32_t dir_idx, dir_lba;
    if (fat16_find_free_entry(parent_cluster, &dir_idx, &dir_lba, dir_buf) < 0) {
        fat16_free_chain(cluster);
        printf("Parent directory full\n");
        return -1;
    }

    /* Write the directory entry */
    if (ata_read_sectors(dir_lba, 1, dir_buf) < 0) {
        fat16_free_chain(cluster);
        return -1;
    }

    fat16_entry_t* entries = (fat16_entry_t*)dir_buf;
    fat16_entry_t* new_entry = &entries[dir_idx];

    memset(new_entry, 0, sizeof(fat16_entry_t));
    memcpy(new_entry->name, fat_name, 11);
    new_entry->attr = FAT16_ATTR_DIRECTORY;
    new_entry->first_cluster = cluster;
    new_entry->file_size = 0; /* directories have size 0 in FAT16 */
    new_entry->crt_time = dos_time;
    new_entry->crt_date = dos_date;
    new_entry->wrt_time = dos_time;
    new_entry->wrt_date = dos_date;

    if (ata_write_sectors(dir_lba, 1, dir_buf) < 0) {
        fat16_free_chain(cluster);
        return -1;
    }

    serial_printf("[FAT16] Created directory '%s' cluster=%u\n", dirname, cluster);
    return 0;
}

/* ── fat16_rmdir ── */

int fat16_rmdir(const char* name) {
    if (!fat16_ready) return -1;

    uint16_t parent_cluster = 0;
    char dirname[13];

    const char* slash = name;
    while (*slash) {
        if (*slash == '/') {
            if (fat16_resolve_path(name, &parent_cluster, dirname) < 0) {
                printf("Path not found\n");
                return -1;
            }
            goto do_rmdir;
        }
        slash++;
    }
    strcpy(dirname, name);

do_rmdir:
    fat16_entry_t entry;
    if (fat16_find_entry_in_dir(parent_cluster, dirname, &entry) < 0) {
        printf("Not found: %s\n", dirname);
        return -1;
    }

    if (!(entry.attr & FAT16_ATTR_DIRECTORY)) {
        printf("Not a directory: %s\n", dirname);
        return -1;
    }

    uint16_t cluster = entry.first_cluster;

    /* Check if directory is empty (only . and ..) */
    /* Read the first sector of the directory */
    if (cluster >= 2) {
        uint32_t lba = cluster_to_lba(cluster);
        if (ata_read_sectors(lba, 1, sector_buf) < 0) return -1;

        fat16_entry_t* dir_entries = (fat16_entry_t*)sector_buf;
        /* Skip . and .. entries, check if any other valid entries exist */
        for (uint32_t i = 2; i < (512 / 32); i++) {
            if (dir_entries[i].name[0] == 0x00) break; /* end of dir - OK */
            if ((uint8_t)dir_entries[i].name[0] == 0xE5) continue; /* deleted - OK */
            if (dir_entries[i].attr == FAT16_ATTR_LFN) continue;
            printf("Directory not empty\n");
            return -1;
        }

        /* Free the cluster chain */
        fat16_free_chain(cluster);
    }

    /* Remove directory entry from parent */
    /* Find and mark the entry as deleted */
    char fat_name[11];
    str_to_fat16_name(dirname, fat_name);

    uint32_t max_entries = (parent_cluster == 0) ?
        (bpb.root_dir_sectors * 512) / 32 : 65534;

    for (uint32_t i = 0; i < max_entries; i++) {
        fat16_entry_t e;
        if (read_dir_entry(parent_cluster, i, &e) < 0) return -1;
        if (e.name[0] == 0x00) break;
        if ((uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr == FAT16_ATTR_LFN) continue;

        if (memcmp(e.name, fat_name, 11) == 0) {
            /* Found - delete it */
            uint32_t entries_per_sector = 512 / 32;
            uint32_t sector_idx;
            if (parent_cluster == 0) {
                sector_idx = bpb.root_dir_start + (i / entries_per_sector);
            } else {
                uint32_t cluster_size_entries = ((uint32_t)bpb.sectors_per_cluster * 512) / 32;
                uint16_t c = parent_cluster;
                uint32_t remaining = i;
                while (c >= 2 && c < 0xFFF8) {
                    if (remaining < cluster_size_entries) {
                        uint32_t sec = (remaining * 32) / 512;
                        sector_idx = cluster_to_lba(c) + sec;
                        goto rmdir_found_sector;
                    }
                    remaining -= cluster_size_entries;
                    c = fat16_next_cluster(c);
                }
                return -1;
            }
rmdir_found_sector:
            if (ata_read_sectors(sector_idx, 1, sector_buf) < 0) return -1;
            fat16_entry_t* ents = (fat16_entry_t*)sector_buf;
            ents[i % (512 / 32)].name[0] = (char)0xE5;
            if (ata_write_sectors(sector_idx, 1, sector_buf) < 0) return -1;
            serial_printf("[FAT16] Removed directory '%s'\n", dirname);
            return 0;
        }
    }

    return -1;
}
