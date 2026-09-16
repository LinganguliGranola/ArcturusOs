#include "ata.h"
#include "io.h"

#include <stddef.h>
#include <stdint.h>

#define ATA_REG_DATA       0
#define ATA_REG_SECCOUNT0  2
#define ATA_REG_LBA0       3
#define ATA_REG_LBA1       4
#define ATA_REG_LBA2       5
#define ATA_REG_HDDEVSEL   6
#define ATA_REG_COMMAND    7
#define ATA_REG_STATUS     7

#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY   0xEC

#define ATA_STATUS_ERR     0x01
#define ATA_STATUS_DRQ     0x08
#define ATA_STATUS_DF      0x20
#define ATA_STATUS_BSY     0x80

struct ata_device {
    uint16_t io_base;
    uint16_t control_base;
    uint8_t drive;
    uint32_t sectors;
};

static struct ata_device devices[ATA_MAX_DEVICES];
static uint8_t device_count;

static void ata_delay_400ns(uint16_t control_base) {
    inb(control_base);
    inb(control_base);
    inb(control_base);
    inb(control_base);
}

static int ata_wait_not_busy(uint16_t io_base) {
    uint32_t timeout = 10000000U;

    while (timeout-- != 0) {
        if (!(inb((uint16_t)(io_base + ATA_REG_STATUS)) & ATA_STATUS_BSY))
            return 1;
    }
    return 0;
}

static int ata_wait_drq(uint16_t io_base) {
    uint8_t status;
    uint32_t timeout = 10000000U;

    while (timeout-- != 0) {
        status = inb((uint16_t)(io_base + ATA_REG_STATUS));
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF))
            return 0;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ))
            return 1;
    }
    return 0;
}

static void ata_select_lba28(const struct ata_device *device, uint32_t lba) {
    outb((uint16_t)(device->io_base + ATA_REG_HDDEVSEL),
         (uint8_t)(0xE0 | (device->drive << 4) | ((lba >> 24) & 0x0F)));
    ata_delay_400ns(device->control_base);
    outb((uint16_t)(device->io_base + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(device->io_base + ATA_REG_LBA0), (uint8_t)lba);
    outb((uint16_t)(device->io_base + ATA_REG_LBA1), (uint8_t)(lba >> 8));
    outb((uint16_t)(device->io_base + ATA_REG_LBA2), (uint8_t)(lba >> 16));
}

void ata_initialize(void) {
    static const uint16_t io_bases[] = { 0x1F0, 0x170 };
    static const uint16_t control_bases[] = { 0x3F6, 0x376 };
    uint8_t channel;

    device_count = 0;
    for (channel = 0; channel < 2; channel++) {
        uint8_t drive;

        for (drive = 0; drive < 2; drive++) {
            uint16_t io_base = io_bases[channel];
            uint16_t control_base = control_bases[channel];
            uint16_t identify[256];
            uint8_t status;
            uint32_t sectors;

            outb((uint16_t)(io_base + ATA_REG_HDDEVSEL),
                 (uint8_t)(0xA0 | (drive << 4)));
            ata_delay_400ns(control_base);
            outb((uint16_t)(io_base + ATA_REG_SECCOUNT0), 0);
            outb((uint16_t)(io_base + ATA_REG_LBA0), 0);
            outb((uint16_t)(io_base + ATA_REG_LBA1), 0);
            outb((uint16_t)(io_base + ATA_REG_LBA2), 0);
            outb((uint16_t)(io_base + ATA_REG_COMMAND), ATA_CMD_IDENTIFY);

            status = inb((uint16_t)(io_base + ATA_REG_STATUS));
            if (status == 0 || status == 0xFF)
                continue;
            if (!ata_wait_not_busy(io_base))
                continue;

            /* Non-zero values identify ATAPI/SATA signature devices, not disks. */
            if (inb((uint16_t)(io_base + ATA_REG_LBA1)) != 0 ||
                inb((uint16_t)(io_base + ATA_REG_LBA2)) != 0)
                continue;
            if (!ata_wait_drq(io_base))
                continue;

            insw((uint16_t)(io_base + ATA_REG_DATA), identify, 256);
            sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
            if (sectors == 0)
                continue;

            devices[device_count].io_base = io_base;
            devices[device_count].control_base = control_base;
            devices[device_count].drive = drive;
            devices[device_count].sectors = sectors;
            device_count++;
        }
    }
}

uint8_t ata_device_count(void) {
    return device_count;
}

uint32_t ata_device_sectors(uint8_t device) {
    if (device >= device_count)
        return 0;
    return devices[device].sectors;
}

bool ata_read_sector(uint8_t device, uint32_t lba, void *buffer) {
    const struct ata_device *ata;

    if (device >= device_count || lba >= devices[device].sectors)
        return false;
    ata = &devices[device];
    if (!ata_wait_not_busy(ata->io_base))
        return false;
    ata_select_lba28(ata, lba);
    outb((uint16_t)(ata->io_base + ATA_REG_COMMAND), ATA_CMD_READ_PIO);
    if (!ata_wait_drq(ata->io_base))
        return false;
    insw((uint16_t)(ata->io_base + ATA_REG_DATA), buffer, ATA_SECTOR_SIZE / 2);
    return true;
}

bool ata_write_sector(uint8_t device, uint32_t lba, const void *buffer) {
    const struct ata_device *ata;

    if (device >= device_count || lba >= devices[device].sectors)
        return false;
    ata = &devices[device];
    if (!ata_wait_not_busy(ata->io_base))
        return false;
    ata_select_lba28(ata, lba);
    outb((uint16_t)(ata->io_base + ATA_REG_COMMAND), ATA_CMD_WRITE_PIO);
    if (!ata_wait_drq(ata->io_base))
        return false;
    outsw((uint16_t)(ata->io_base + ATA_REG_DATA), buffer, ATA_SECTOR_SIZE / 2);
    outb((uint16_t)(ata->io_base + ATA_REG_COMMAND), ATA_CMD_CACHE_FLUSH);
    return ata_wait_not_busy(ata->io_base);
}
