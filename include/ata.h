#ifndef ATA_H
#define ATA_H

#include "types.h"

/* ATA I/O port registers (Primary bus) */
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6

#define ATA_REG_DATA        0x00    /* Data register (16-bit) */
#define ATA_REG_ERROR       0x01    /* Error register (read) */
#define ATA_REG_FEATURES    0x01    /* Features register (write) */
#define ATA_REG_SECCOUNT    0x02    /* Sector count */
#define ATA_REG_LBA_LO      0x03    /* LBA low byte (sector) */
#define ATA_REG_LBA_MID     0x04    /* LBA mid byte (cylinder low) */
#define ATA_REG_LBA_HI      0x05    /* LBA high byte (cylinder high) */
#define ATA_REG_DRIVE       0x06    /* Drive/head select */
#define ATA_REG_STATUS      0x07    /* Status (read) */
#define ATA_REG_COMMAND     0x07    /* Command (write) */

/* Alternate status register (control port) */
#define ATA_REG_ALT_STATUS  0x06    /* Alt status (read from ctrl port) */
#define ATA_REG_DEV_CTRL    0x06    /* Device control (write to ctrl port) */

/* Status register bits */
#define ATA_SR_BSY  0x80    /* Busy */
#define ATA_SR_DRDY 0x40    /* Drive ready */
#define ATA_SR_DF   0x20    /* Drive fault */
#define ATA_SR_DRQ  0x08    /* Data request */
#define ATA_SR_ERR  0x01    /* Error */

/* ATA commands */
#define ATA_CMD_READ        0x20    /* Read sectors (PIO, 28-bit LBA) */
#define ATA_CMD_WRITE       0x30    /* Write sectors (PIO, 28-bit LBA) */
#define ATA_CMD_IDENTIFY    0xEC    /* Identify device */

/* Sector size */
#define ATA_SECTOR_SIZE     512

/* Drive selection */
#define ATA_DRIVE_MASTER    0xE0    /* Master drive, LBA mode */
#define ATA_DRIVE_SLAVE     0xF0    /* Slave drive, LBA mode */

/* API */
int ata_init(void);
int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer);
int ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer);
int ata_identify(uint16_t* identify_buf);
int ata_disk_present(void);

#endif /* ATA_H */
