#include <aurora/drivers/ramdisk.h>
#include <aurora/drivers/block.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

struct ramdisk_priv {
    uint8_t *data;
    size_t size;
};

static int ramdisk_read_blocks(struct block_device *bdev, uint64_t lba, uint32_t count, void *buffer)
{
    struct ramdisk_priv *priv = (struct ramdisk_priv *)bdev->private_data;
    if (!priv || !priv->data || !buffer) return -EINVAL;
    if (count > bdev->total_blocks || lba > bdev->total_blocks - count) {
        return -EIO;
    }

    uint64_t offset = lba * bdev->block_size;
    uint64_t bytes = (uint64_t)count * bdev->block_size;

    if (offset + bytes > priv->size) {
        return -EIO;
    }

    memcpy(buffer, priv->data + offset, bytes);
    return 0;
}

static int ramdisk_write_blocks(struct block_device *bdev, uint64_t lba, uint32_t count, const void *buffer)
{
    struct ramdisk_priv *priv = (struct ramdisk_priv *)bdev->private_data;
    if (!priv || !priv->data || !buffer) return -EINVAL;
    if (count > bdev->total_blocks || lba > bdev->total_blocks - count) {
        return -EIO;
    }

    uint64_t offset = lba * bdev->block_size;
    uint64_t bytes = (uint64_t)count * bdev->block_size;

    if (offset + bytes > priv->size) {
        return -EIO;
    }

    memcpy(priv->data + offset, buffer, bytes);
    return 0;
}

static int ramdisk_flush(struct block_device *bdev)
{
    (void)bdev;
    return 0;
}

static struct block_device_ops g_ramdisk_ops = {
    .read_blocks = ramdisk_read_blocks,
    .write_blocks = ramdisk_write_blocks,
    .flush = ramdisk_flush,
};

int ramdisk_init(size_t size_bytes)
{
    if (size_bytes == 0) {
        size_bytes = 16 * 1024 * 1024; /* 16 MB default */
    }

    size_t pages = (size_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uint8_t *buf = (uint8_t *)kmalloc(pages * PAGE_SIZE, GFP_KERNEL);
    if (!buf) {
        printk("<3>[RAMDISK] Failed to allocate memory for ramdisk\n");
        return -ENOMEM;
    }
    memset(buf, 0, pages * PAGE_SIZE);

    struct ramdisk_priv *priv = (struct ramdisk_priv *)kzalloc(sizeof(struct ramdisk_priv), GFP_KERNEL);
    if (!priv) {
        kfree(buf);
        return -ENOMEM;
    }
    priv->data = buf;
    priv->size = pages * PAGE_SIZE;

    struct block_device *bdev = (struct block_device *)kzalloc(sizeof(struct block_device), GFP_KERNEL);
    if (!bdev) {
        kfree(priv);
        kfree(buf);
        return -ENOMEM;
    }

    strncpy(bdev->name, "ram0", sizeof(bdev->name) - 1);
    bdev->dev_id = (1 << 8) | 0; /* major 1, minor 0 */
    bdev->block_size = 512;
    bdev->total_blocks = priv->size / 512;
    bdev->ops = &g_ramdisk_ops;
    bdev->private_data = priv;

    return bdev_register(bdev);
}
