#include "../include/ata.h"
#include "../include/io.h"
#include "../include/stdio.h"
#include "../include/string.h"

static int disk_detected = 0;

/* Wait for BSY to clear. Returns 0 on success, -1 on timeout. */
static int ata_wait_ready(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY))
            return 0;
        io_wait();
    }
    return -1;
}

/* Wait for DRQ to be set (data ready). Returns 0 on success, -1 on error/timeout. */
static int ata_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DF)  return -1;
        if (status & ATA_SR_DRQ) return 0;
        io_wait();
    }
    return -1;
}

int ata_init(void) {
    /* Select master drive */
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, ATA_DRIVE_MASTER);
    io_wait();

    /* Check if drive exists by reading status */
    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status == 0xFF) {
        /* 0xFF means no drive (floating bus) */
        serial_printf("[ATA] No disk detected\n");
        disk_detected = 0;
        return -1;
    }

    /* Wait for drive to be ready */
    if (ata_wait_ready() < 0) {
        serial_printf("[ATA] Drive not ready\n");
        disk_detected = 0;
        return -1;
    }

    disk_detected = 1;
    serial_printf("[ATA] Disk detected on primary master\n");
    return 0;
}

int ata_disk_present(void) {
    return disk_detected;
}

int ata_identify(uint16_t* identify_buf) {
    if (!disk_detected) return -1;

    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, ATA_DRIVE_MASTER);
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status == 0) {
        serial_printf("[ATA] IDENTIFY: drive does not exist\n");
        return -1;
    }

    if (ata_wait_ready() < 0) return -1;

    /* Check LBA_MID and LBA_HI for ATAPI (non-zero = not ATA) */
    if (inb(ATA_PRIMARY_IO + ATA_REG_LBA_MID) != 0 ||
        inb(ATA_PRIMARY_IO + ATA_REG_LBA_HI) != 0) {
        serial_printf("[ATA] IDENTIFY: not an ATA device\n");
        return -1;
    }

    if (ata_wait_drq() < 0) {
        serial_printf("[ATA] IDENTIFY: no DRQ\n");
        return -1;
    }

    /* Read 256 words (512 bytes) of identify data */
    for (int i = 0; i < 256; i++) {
        identify_buf[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }

    return 0;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer) {
    if (!disk_detected) return -1;
    if (count == 0) return 0;

    uint16_t* buf = (uint16_t*)buffer;

    /* Wait for drive to be ready */
    if (ata_wait_ready() < 0) return -1;

    /* Set up the read command */
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, count);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE,
         ATA_DRIVE_MASTER | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ);

    for (uint8_t s = 0; s < count; s++) {
        if (ata_wait_drq() < 0) {
            serial_printf("[ATA] Read error at LBA %u, sector %u\n", lba, s);
            return -1;
        }
        /* Read 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
        }
    }

    return 0;
}
