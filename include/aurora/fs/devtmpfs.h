#ifndef _AURORA_FS_DEVTMPFS_H
#define _AURORA_FS_DEVTMPFS_H

#include <aurora/fs/vfs.h>

struct super_block *devtmpfs_create(void);

int devtmpfs_register_chrdev(uint32_t major, uint32_t minor, const char *name,
                            struct file_operations *fops, mode_t mode);

int devtmpfs_register_blkdev(uint32_t major, uint32_t minor, const char *name,
                            struct file_operations *fops, mode_t mode);

#endif /* _AURORA_FS_DEVTMPFS_H */
