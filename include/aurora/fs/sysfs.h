#ifndef _AURORA_FS_SYSFS_H
#define _AURORA_FS_SYSFS_H

#include <aurora/fs/vfs.h>

typedef ssize_t (*sysfs_read_fn)(char *buf, size_t max_len);
typedef ssize_t (*sysfs_write_fn)(const char *buf, size_t count);

struct super_block *sysfs_create(void);

struct netdev;
void sysfs_register_netdev(struct netdev *dev);

#endif /* _AURORA_FS_SYSFS_H */
