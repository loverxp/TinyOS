#ifndef MBR_H
#define MBR_H

#include "types.h"

#define MBR_PARTITION_COUNT  4
#define MBR_SIGNATURE        0xAA55

#define MBR_TYPE_FAT16_SMALL  0x04
#define MBR_TYPE_FAT16_LARGE  0x06
#define MBR_TYPE_FAT16_LBA    0x0E

typedef struct {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed)) mbr_partition_t;

typedef struct {
    uint8_t  bootstrap[446];
    mbr_partition_t partitions[MBR_PARTITION_COUNT];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

typedef struct {
    int valid;
    int count;
    int fat16_partition;
    mbr_partition_t entries[MBR_PARTITION_COUNT];
} mbr_info_t;

int  mbr_init(mbr_info_t* info);
int  mbr_is_fat16(uint8_t type);
void mbr_list_partitions(const mbr_info_t* info);
mbr_info_t* mbr_get_info(void);

#endif /* MBR_H */
