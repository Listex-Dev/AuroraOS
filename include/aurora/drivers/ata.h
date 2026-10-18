#ifndef _AURORA_DRIVERS_ATA_H
#define _AURORA_DRIVERS_ATA_H

#include <aurora/types.h>
#include <aurora/drivers/block.h>

int ata_init(void);
void ata_scan_partitions(struct block_device *disk_bdev);

#endif /* _AURORA_DRIVERS_ATA_H */
