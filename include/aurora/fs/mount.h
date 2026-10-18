#ifndef _AURORA_FS_MOUNT_H
#define _AURORA_FS_MOUNT_H

#include <aurora/fs/vfs.h>

int vfs_mount(const char *source, const char *target, const char *fstype, uint32_t flags, const void *data);
size_t vfs_format_mounts(char *buf, size_t max_len);
int vfs_pivot_root(const char *new_root_path, const char *put_old_path);
int vfs_mount_fstab(const char *fstab_path);
struct dentry *vfs_get_mountpoint_for_root(struct dentry *d);

#endif /* _AURORA_FS_MOUNT_H */
