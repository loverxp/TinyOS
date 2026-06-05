#ifndef FAT16_H
#define FAT16_H

#include "types.h"

/* FAT16 directory entry (32 bytes) */
typedef struct {
    char     name[11];       /* 8.3 filename, space-padded */
    uint8_t  attr;           /* File attributes */
    uint8_t  nt_reserved;    /* NT reserved byte */
    uint8_t  crt_time_tenth; /* Creation time, tenths of seconds */
    uint16_t crt_time;       /* Creation time */
    uint16_t crt_date;       /* Creation date */
    uint16_t lst_acess;      /* Last access date */
    uint16_t fst_clus_hi;    /* First cluster high 16 bits (FAT32) */
    uint16_t wrt_time;       /* Last write time */
    uint16_t wrt_date;       /* Last write date */
    uint16_t first_cluster;  /* First cluster low 16 bits */
    uint32_t file_size;      /* File size in bytes */
} __attribute__((packed)) fat16_entry_t;

/* Directory entry attributes */
#define FAT16_ATTR_READ_ONLY  0x01
#define FAT16_ATTR_HIDDEN     0x02
#define FAT16_ATTR_SYSTEM     0x04
#define FAT16_ATTR_VOLUME_ID  0x08
#define FAT16_ATTR_DIRECTORY  0x10
#define FAT16_ATTR_ARCHIVE    0x20
#define FAT16_ATTR_LFN        0x0F  /* Long filename entry */

/* BPB (BIOS Parameter Block) fields we care about */
typedef struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;     /* Sectors per FAT */
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t total_sectors_32;

    /* Derived values (computed at init) */
    uint32_t root_dir_sectors;
    uint32_t first_data_sector;
    uint32_t data_sectors;
    uint32_t total_clusters;
    uint32_t fat_start;       /* First sector of FAT */
    uint32_t root_dir_start;  /* First sector of root directory */
    uint32_t data_start;      /* First sector of data region */
    uint32_t total_size;      /* Total disk size in bytes */
} fat16_bpb_t;

/* API */
int fat16_init(void);
int fat16_find(const char* name, fat16_entry_t* out);
uint32_t fat16_read(const fat16_entry_t* entry, uint32_t offset,
                    void* buffer, uint32_t size);
int fat16_list(void);
const fat16_bpb_t* fat16_get_bpb(void);

/* Write operations */
int fat16_write(const char* name, const void* data, uint32_t size);
int fat16_delete(const char* name);

#endif /* FAT16_H */
