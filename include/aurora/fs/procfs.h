#ifndef _AURORA_FS_PROCFS_H
#define _AURORA_FS_PROCFS_H

#include <aurora/fs/vfs.h>

struct super_block *procfs_create(void);

typedef ssize_t (*proc_read_fn)(char *buf, size_t max_len);

int procfs_add_entry(const char *name, proc_read_fn generator);

#endif /* _AURORA_FS_PROCFS_H */
