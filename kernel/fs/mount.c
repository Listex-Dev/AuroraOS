#include <aurora/fs/mount.h>
#include <aurora/fs/vfs.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

extern struct super_block *ramfs_create(void);
extern struct super_block *procfs_create(void);
extern struct super_block *devtmpfs_create(void);
extern struct super_block *tmpfs_create(void);
extern struct super_block *sysfs_create(void);

static struct vfsmount *mount_list_head = NULL;

int vfs_mount(const char *source, const char *target, const char *fstype, uint32_t flags, const void *data)
{
    (void)data;
    if (!target || !fstype) {
        return -EINVAL;
    }

    struct dentry *target_dentry = vfs_namei(target);
    if (!target_dentry) {
        return -ENOENT;
    }

    if (!target_dentry->d_inode || !S_ISDIR(target_dentry->d_inode->i_mode)) {
        return -ENOTDIR;
    }

    struct super_block *sb = NULL;
    if (strcmp(fstype, "ramfs") == 0) {
        sb = ramfs_create();
    } else if (strcmp(fstype, "procfs") == 0 || strcmp(fstype, "proc") == 0) {
        sb = procfs_create();
    } else if (strcmp(fstype, "devtmpfs") == 0 || strcmp(fstype, "devfs") == 0) {
        sb = devtmpfs_create();
    } else if (strcmp(fstype, "tmpfs") == 0) {
        sb = tmpfs_create();
    } else if (strcmp(fstype, "sysfs") == 0) {
        sb = sysfs_create();
    } else {
        printk("<3>[MOUNT] Unknown filesystem type: %s\n", fstype);
        return -ENODEV;
    }

    if (!sb || !sb->s_root) {
        return -ENOMEM;
    }

    struct vfsmount *mnt = (struct vfsmount *)kzalloc(sizeof(struct vfsmount), GFP_KERNEL);
    if (!mnt) {
        return -ENOMEM;
    }

    mnt->mnt_mountpoint = target_dentry;
    mnt->mnt_sb = sb;
    mnt->mnt_root = sb->s_root;
    mnt->mnt_flags = flags;
    strncpy(mnt->mnt_devname, source ? source : fstype, sizeof(mnt->mnt_devname) - 1);

    /* Attach mount to target dentry */
    target_dentry->d_mounted = mnt;

    /* Append to mount list */
    mnt->mnt_next = mount_list_head;
    mount_list_head = mnt;

    printk("<6>[MOUNT] Mounted %s on %s (fstype: %s)\n",
           source ? source : "none", target, fstype);

    return 0;
}

size_t vfs_format_mounts(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) {
        return 0;
    }

    buf[0] = '\0';
    size_t off = 0;

    for (struct vfsmount *m = mount_list_head; m != NULL; m = m->mnt_next) {
        const char *mp = m->mnt_mountpoint ? m->mnt_mountpoint->d_name : "/";
        const char *t = m->mnt_sb && m->mnt_sb->s_type ? m->mnt_sb->s_type : "unknown";
        int len = snprintf(buf + off, max_len - off, "%s /%s %s rw 0 0\n",
                           m->mnt_devname, strcmp(mp, "/") == 0 ? "" : mp, t);
        if (len > 0 && off + len < max_len) {
            off += len;
        } else {
            break;
        }
    }

    return off;
}
