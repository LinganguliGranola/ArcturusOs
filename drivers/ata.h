#ifndef ATA_H
#define ATA_H

#include <stdbool.h>
#include <stdint.h>

#define ATA_SECTOR_SIZE 512U
#define ATA_MAX_DEVICES 4U

/* Probe the four legacy IDE positions (primary/secondary, master/slave). */
void ata_initialize(void);

uint8_t ata_device_count(void);
uint32_t ata_device_sectors(uint8_t device);

/* Read or write exactly one 512-byte sector using 28-bit LBA PIO. */
bool ata_read_sector(uint8_t device, uint32_t lba, void *buffer);
bool ata_write_sector(uint8_t device, uint32_t lba, const void *buffer);

#endif
