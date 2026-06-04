#include "../include/fat16.h"
#include "../include/ata.h"
#include "../include/stdio.h"
#include "../include/string.h"

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

int fat16_find(const char* name, fat16_entry_t* out) {
    if (!fat16_ready) return -1;

    char fat_name[11];
    str_to_fat16_name(name, fat_name);

    /* Scan root directory */
    uint32_t entries_per_sector = 512 / 32;  /* 16 entries per sector */
    uint32_t total_dir_sectors = bpb.root_dir_sectors;

    for (uint32_t s = 0; s < total_dir_sectors; s++) {
        if (ata_read_sectors(bpb.root_dir_start + s, 1, sector_buf) < 0)
            return -1;

        fat16_entry_t* entries = (fat16_entry_t*)sector_buf;
        for (uint32_t e = 0; e < entries_per_sector; e++) {
            /* End of directory */
            if (entries[e].name[0] == 0x00)
                return -1;
            /* Deleted entry */
            if ((uint8_t)entries[e].name[0] == 0xE5)
                continue;
            /* Skip LFN entries */
            if (entries[e].attr == FAT16_ATTR_LFN)
                continue;

            if (memcmp(entries[e].name, fat_name, 11) == 0) {
                memcpy(out, &entries[e], sizeof(fat16_entry_t));
                return 0;
            }
        }
    }

    return -1;  /* Not found */
}

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
        if (cluster >= 0xFFF8) return bytes_read;  /* End of chain */
    }

    /* Read from offset within first cluster */
    uint32_t cluster_offset = offset % cluster_size;

    while (bytes_read < size && cluster < 0xFFF8) {
        /* Read the current cluster */
        uint32_t lba = cluster_to_lba(cluster);
        uint32_t to_read = cluster_size - cluster_offset;
        if (to_read > size - bytes_read)
            to_read = size - bytes_read;

        /* Read sector by sector */
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

        cluster_offset = 0;  /* Subsequent clusters start at offset 0 */
        cluster = fat16_next_cluster(cluster);
    }

    return bytes_read;
}

int fat16_list(void) {
    if (!fat16_ready) {
        printf("FAT16 not initialized\n");
        return -1;
    }

    printf("Directory listing:\n");
    printf("%-14s %10s  %s\n", "Name", "Size", "Type");
    printf("--------------------------------------\n");

    uint32_t entries_per_sector = 512 / 32;
    int count = 0;

    for (uint32_t s = 0; s < bpb.root_dir_sectors; s++) {
        if (ata_read_sectors(bpb.root_dir_start + s, 1, sector_buf) < 0)
            return -1;

        fat16_entry_t* entries = (fat16_entry_t*)sector_buf;
        for (uint32_t e = 0; e < entries_per_sector; e++) {
            if (entries[e].name[0] == 0x00) goto done;
            if ((uint8_t)entries[e].name[0] == 0xE5) continue;
            if (entries[e].attr == FAT16_ATTR_LFN) continue;

            char display_name[14];
            fat16_name_to_str(entries[e].name, display_name);

            const char* type = "FILE";
            if (entries[e].attr & FAT16_ATTR_DIRECTORY) type = "DIR";
            if (entries[e].attr & FAT16_ATTR_VOLUME_ID) type = "VOL";

            printf("%-14s %10u  %s\n", display_name, entries[e].file_size, type);
            count++;
        }
    }

done:
    printf("\n%d entries\n", count);
    return count;
}
