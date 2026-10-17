#ifndef _AURORA_DRIVERS_BLOCK_H
#define _AURORA_DRIVERS_BLOCK_H

#include <aurora/types.h>
#include <aurora/fs/vfs.h>

struct block_device;

struct block_device_ops {
    int (*read_blocks)(struct block_device *bdev, uint64_t lba, uint32_t count, void *buffer);
    int (*write_blocks)(struct block_device *bdev, uint64_t lba, uint32_t count, const void *buffer);
    int (*flush)(struct block_device *bdev);
};

struct block_device {
    char name[32];               /* e.g. "sda", "sda1", "ram0" */
    dev_t dev_id;                /* (major << 8) | minor */
    uint32_t block_size;         /* sector size, typically 512 */
    uint64_t total_blocks;       /* capacity in blocks */
    struct block_device_ops *ops;
    void *private_data;
    struct block_device *next;
};

void block_subsys_init(void);
int bdev_register(struct block_device *bdev);
int bdev_unregister(struct block_device *bdev);
struct block_device *bdev_get(const char *name);
struct block_device *bdev_get_by_dev(dev_t dev_id);
int bdev_read(struct block_device *bdev, uint64_t lba, uint32_t count, void *buffer);
int bdev_write(struct block_device *bdev, uint64_t lba, uint32_t count, const void *buffer);
int bdev_flush(struct block_device *bdev);

#endif /* _AURORA_DRIVERS_BLOCK_H */
