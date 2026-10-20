#include <aurora/drivers/block.h>
#include <aurora/fs/devtmpfs.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

static struct block_device *g_bdev_list = NULL;

static ssize_t bdev_file_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    struct inode *inode = file->f_inode;
    if (!inode) return -EINVAL;

    struct block_device *bdev = bdev_get_by_dev(inode->i_rdev);
    if (!bdev) return -ENODEV;

    uint64_t disk_bytes = bdev->total_blocks * bdev->block_size;
    if (*pos >= disk_bytes) {
        return 0; /* EOF */
    }

    size_t to_read = count;
    if (*pos + to_read > disk_bytes) {
        to_read = disk_bytes - *pos;
    }

    uint64_t start_lba = *pos / bdev->block_size;
    size_t offset_in_sector = *pos % bdev->block_size;
    uint32_t num_sectors = (to_read + offset_in_sector + bdev->block_size - 1) / bdev->block_size;

    uint8_t *tmp = (uint8_t *)kmalloc(num_sectors * bdev->block_size, GFP_KERNEL);
    if (!tmp) return -ENOMEM;

    int err = bdev_read(bdev, start_lba, num_sectors, tmp);
    if (err != 0) {
        kfree(tmp);
        return err;
    }

    memcpy(buf, tmp + offset_in_sector, to_read);
    kfree(tmp);

    *pos += to_read;
    return (ssize_t)to_read;
}

static ssize_t bdev_file_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    struct inode *inode = file->f_inode;
    if (!inode) return -EINVAL;

    struct block_device *bdev = bdev_get_by_dev(inode->i_rdev);
    if (!bdev) return -ENODEV;

    uint64_t disk_bytes = bdev->total_blocks * bdev->block_size;
    if (*pos >= disk_bytes) {
        return -ENOSPC;
    }

    size_t to_write = count;
    if (*pos + to_write > disk_bytes) {
        to_write = disk_bytes - *pos;
    }

    uint64_t start_lba = *pos / bdev->block_size;
    size_t offset_in_sector = *pos % bdev->block_size;
    uint32_t num_sectors = (to_write + offset_in_sector + bdev->block_size - 1) / bdev->block_size;

    uint8_t *tmp = (uint8_t *)kmalloc(num_sectors * bdev->block_size, GFP_KERNEL);
    if (!tmp) return -ENOMEM;

    /* Read-modify-write if partial sectors */
    if (offset_in_sector != 0 || (to_write % bdev->block_size) != 0) {
        bdev_read(bdev, start_lba, num_sectors, tmp);
    }

    memcpy(tmp + offset_in_sector, buf, to_write);

    int err = bdev_write(bdev, start_lba, num_sectors, tmp);
    kfree(tmp);
    if (err != 0) {
        return err;
    }

    *pos += to_write;
    return (ssize_t)to_write;
}

static uint64_t bdev_file_llseek(struct file *file, uint64_t offset, int whence)
{
    struct inode *inode = file->f_inode;
    struct block_device *bdev = inode ? bdev_get_by_dev(inode->i_rdev) : NULL;
    uint64_t disk_bytes = bdev ? (bdev->total_blocks * bdev->block_size) : 0;

    int64_t new_pos = 0;
    switch (whence) {
        case SEEK_SET:
            new_pos = (int64_t)offset;
            break;
        case SEEK_CUR:
            new_pos = (int64_t)file->f_pos + (int64_t)offset;
            break;
        case SEEK_END:
            new_pos = (int64_t)disk_bytes + (int64_t)offset;
            break;
        default:
            return (uint64_t)-EINVAL;
    }

    if (new_pos < 0 || (bdev && (uint64_t)new_pos > disk_bytes)) {
        return (uint64_t)-EINVAL;
    }

    file->f_pos = (uint64_t)new_pos;
    return file->f_pos;
}

struct file_operations g_bdev_fops = {
    .read = bdev_file_read,
    .write = bdev_file_write,
    .llseek = bdev_file_llseek,
};

void block_subsys_init(void)
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    g_bdev_list = NULL;
    printk("<6>[  0.000000] Block Layer: Initialized block device subsystem\n");
}

int bdev_register(struct block_device *bdev)
{
    if (!bdev || !bdev->ops) {
        return -EINVAL;
    }

    bdev->next = g_bdev_list;
    g_bdev_list = bdev;

    uint32_t major = (uint32_t)(bdev->dev_id >> 8);
    uint32_t minor = (uint32_t)(bdev->dev_id & 0xFF);

    /* Register device node in /dev (devtmpfs) with root:disk 0660 */
    devtmpfs_register_blkdev(major, minor, bdev->name, &g_bdev_fops, 0660);

    uint64_t mb = (bdev->total_blocks * bdev->block_size) / (1024 * 1024);
    printk("<6>[BDEV] Registered block device /dev/%s (%llu MB, %llu sectors, major %u, minor %u)\n",
           bdev->name, mb, bdev->total_blocks, major, minor);

    return 0;
}

int bdev_unregister(struct block_device *bdev)
{
    if (!bdev) return -EINVAL;

    struct block_device **curr = &g_bdev_list;
    while (*curr) {
        if (*curr == bdev) {
            *curr = bdev->next;
            return 0;
        }
        curr = &(*curr)->next;
    }
    return -ENOENT;
}

struct block_device *bdev_get_list(void)
{
    return g_bdev_list;
}

struct block_device *bdev_get(const char *name)
{
    if (!name) return NULL;

    const char *p = name;
    if (strncmp(p, "/dev/", 5) == 0) {
        p += 5;
    }

    for (struct block_device *b = g_bdev_list; b != NULL; b = b->next) {
        if (strcmp(b->name, p) == 0) {
            return b;
        }
    }
    return NULL;
}

struct block_device *bdev_get_by_dev(dev_t dev_id)
{
    for (struct block_device *b = g_bdev_list; b != NULL; b = b->next) {
        if (b->dev_id == dev_id) {
            return b;
        }
    }
    return NULL;
}

int bdev_read(struct block_device *bdev, uint64_t lba, uint32_t count, void *buffer)
{
    if (!bdev || !bdev->ops || !bdev->ops->read_blocks || !buffer) {
        return -EINVAL;
    }
    if (count > bdev->total_blocks || lba > bdev->total_blocks - count) {
        return -EIO;
    }
    return bdev->ops->read_blocks(bdev, lba, count, buffer);
}

int bdev_write(struct block_device *bdev, uint64_t lba, uint32_t count, const void *buffer)
{
    if (!bdev || !bdev->ops || !bdev->ops->write_blocks || !buffer) {
        return -EINVAL;
    }
    if (count > bdev->total_blocks || lba > bdev->total_blocks - count) {
        return -EIO;
    }
    return bdev->ops->write_blocks(bdev, lba, count, buffer);
}

int bdev_flush(struct block_device *bdev)
{
    if (!bdev || !bdev->ops) return -EINVAL;
    if (bdev->ops->flush) {
        return bdev->ops->flush(bdev);
    }
    return 0;
}
