#ifndef _AURORA_FS_MOUNT_H
#define _AURORA_FS_MOUNT_H

#include <aurora/fs/vfs.h>

int vfs_mount(const char *source, const char *target, const char *fstype, uint32_t flags, const void *data);
size_t vfs_format_mounts(char *buf, size_t max_len);

#endif /* _AURORA_FS_MOUNT_H */
