#ifndef _AURORA_DRIVERS_RAMDISK_H
#define _AURORA_DRIVERS_RAMDISK_H

#include <aurora/types.h>
#include <aurora/drivers/block.h>

int ramdisk_init(size_t size_bytes);

#endif /* _AURORA_DRIVERS_RAMDISK_H */
