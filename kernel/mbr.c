#include "../include/mbr.h"
#include "../include/ata.h"
#include "../include/stdio.h"
#include "../include/string.h"

static mbr_info_t global_mbr_info;

mbr_info_t* mbr_get_info(void) {
    return &global_mbr_info;
}

int mbr_is_fat16(uint8_t type) {
    return type == MBR_TYPE_FAT16_SMALL ||
           type == MBR_TYPE_FAT16_LARGE ||
           type == MBR_TYPE_FAT16_LBA;
}

static const char* mbr_type_name(uint8_t type) {
    switch (type) {
        case 0x00: return "Empty";
        case 0x01: return "FAT12";
        case MBR_TYPE_FAT16_SMALL: return "FAT16 <32M";
        case 0x05: return "Extended";
        case MBR_TYPE_FAT16_LARGE: return "FAT16 CHS";
        case 0x07: return "NTFS/exFAT";
        case 0x0B: return "FAT32 CHS";
        case 0x0C: return "FAT32 LBA";
        case MBR_TYPE_FAT16_LBA: return "FAT16 LBA";
        case 0x83: return "Linux";
        default:   return "Unknown";
    }
}

int mbr_init(mbr_info_t* info) {
    memset(info, 0, sizeof(mbr_info_t));
    info->fat16_partition = -1;

    uint8_t buf[512] __attribute__((aligned(4)));
    if (ata_read_sectors(0, 1, buf) < 0)
        return -1;

    mbr_t* mbr = (mbr_t*)buf;
    if (mbr->signature != MBR_SIGNATURE)
        return -1;

    info->valid = 1;
    info->count = 0;

    for (int i = 0; i < MBR_PARTITION_COUNT; i++) {
        memcpy(&info->entries[i], &mbr->partitions[i], sizeof(mbr_partition_t));
        if (info->entries[i].type != 0x00) {
            info->count++;
            if (info->fat16_partition < 0 && mbr_is_fat16(info->entries[i].type)) {
                info->fat16_partition = i;
            }
        }
    }

    serial_printf("[MBR] Found %d partition(s), FAT16 at index %d\n",
                  info->count, info->fat16_partition);
    return 0;
}

void mbr_list_partitions(const mbr_info_t* info) {
    if (!info->valid) {
        printf("No MBR found (whole-disk filesystem)\n");
        return;
    }

    printf("MBR Partition Table:\n");
    printf("%-4s %-4s %-14s %10s %10s\n", "Idx", "Boot", "Type", "Start(LBA)", "Size(MB)");
    printf("----------------------------------------------\n");

    for (int i = 0; i < MBR_PARTITION_COUNT; i++) {
        const mbr_partition_t* p = &info->entries[i];
        if (p->type == 0x00) continue;

        uint32_t size_mb = (p->sector_count / 2048);
        printf("%-4d %-4s %-14s %10u %10u\n",
               i,
               (p->status & 0x80) ? "Yes" : "No",
               mbr_type_name(p->type),
               p->lba_start,
               size_mb);
    }
}
