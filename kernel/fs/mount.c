#include <aurora/fs/mount.h>
#include <aurora/fs/vfs.h>
#include <aurora/fs/ext2.h>
#include <aurora/drivers/block.h>
#include <aurora/proc/sched.h>
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
    } else if (strcmp(fstype, "ext2") == 0 || strcmp(fstype, "ext4") == 0) {
        if (!source) return -EINVAL;
        struct block_device *bdev = bdev_get(source);
        if (!bdev) {
            printk("<3>[MOUNT] Block device %s not found\n", source);
            return -ENODEV;
        }
        sb = ext2_mount(bdev);
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

int vfs_pivot_root(const char *new_root_path, const char *put_old_path)
{
    if (!new_root_path || !put_old_path) {
        return -EINVAL;
    }

    struct dentry *new_root_dentry = vfs_namei(new_root_path);
    struct dentry *put_old_dentry = vfs_namei(put_old_path);

    if (!new_root_dentry || !put_old_dentry) {
        return -ENOENT;
    }

    if (!new_root_dentry->d_inode || !S_ISDIR(new_root_dentry->d_inode->i_mode) ||
        !put_old_dentry->d_inode || !S_ISDIR(put_old_dentry->d_inode->i_mode)) {
        return -ENOTDIR;
    }

    /* Find mount record for new_root */
    struct vfsmount *new_mnt = NULL;
    for (struct vfsmount *m = mount_list_head; m != NULL; m = m->mnt_next) {
        if (m->mnt_root == new_root_dentry) {
            new_mnt = m;
            break;
        }
    }

    if (!new_mnt) {
        return -EINVAL;
    }

    struct dentry *old_root_dentry = vfs_get_root();

    /* Mount old root at put_old */
    struct vfsmount *old_mnt = (struct vfsmount *)kzalloc(sizeof(struct vfsmount), GFP_KERNEL);
    if (!old_mnt) return -ENOMEM;

    old_mnt->mnt_mountpoint = put_old_dentry;
    old_mnt->mnt_sb = old_root_dentry ? old_root_dentry->d_sb : NULL;
    old_mnt->mnt_root = old_root_dentry;
    strncpy(old_mnt->mnt_devname, "old_root", sizeof(old_mnt->mnt_devname) - 1);

    put_old_dentry->d_mounted = old_mnt;
    old_mnt->mnt_next = mount_list_head;
    mount_list_head = old_mnt;

    /* Switch global VFS root to new root */
    vfs_set_root(new_mnt->mnt_sb);

    /* Update current process root and pwd */
    task_t *curr = sched_current();
    if (curr && curr->fs) {
        struct fs_struct *fs = (struct fs_struct *)curr->fs;
        fs->root = new_mnt->mnt_root;
        fs->pwd = new_mnt->mnt_root;
    }

    printk("<6>[PIVOT_ROOT] Successfully pivoted root to %s (old root moved to %s)\n",
           new_root_path, put_old_path);

    return 0;
}

int vfs_mount_fstab(const char *fstab_path)
{
    if (!fstab_path) return -EINVAL;

    struct file *f = NULL;
    int err = vfs_open(fstab_path, O_RDONLY, 0, &f);
    if (err != 0 || !f) {
        return err;
    }

    char *buf = (char *)kmalloc(2048, GFP_KERNEL);
    if (!buf) {
        vfs_close(f);
        return -ENOMEM;
    }

    ssize_t n = vfs_read(f, buf, 2047);
    vfs_close(f);
    if (n <= 0) {
        kfree(buf);
        return 0;
    }
    buf[n] = '\0';

    int mounts_done = 0;
    char *line = buf;

    while (*line) {
        /* Find end of line */
        char *eol = strchr(line, '\n');
        if (eol) {
            *eol = '\0';
        }

        /* Skip leading whitespace */
        while (*line == ' ' || *line == '\t') line++;

        /* Skip comments and empty lines */
        if (*line != '\0' && *line != '#') {
            char dev[64], mp[64], fstype[32], opts[64];
            dev[0] = mp[0] = fstype[0] = opts[0] = '\0';

            /* Parse tokens */
            char *p = line;
            char *tokens[6] = { dev, mp, fstype, opts, NULL, NULL };
            for (int t = 0; t < 4 && *p; t++) {
                while (*p == ' ' || *p == '\t') p++;
                char *dst = tokens[t];
                int idx = 0;
                while (*p && *p != ' ' && *p != '\t' && idx < 63) {
                    dst[idx++] = *p++;
                }
                dst[idx] = '\0';
            }

            if (dev[0] && mp[0] && fstype[0]) {
                if (strcmp(mp, "/") == 0) {
                    /* Root filesystem already mounted */
                } else {
                    int merr = vfs_mount(dev, mp, fstype, 0, NULL);
                    if (merr == 0) {
                        mounts_done++;
                    }
                }
            }
        }

        if (!eol) break;
        line = eol + 1;
    }

    kfree(buf);
    printk("<6>[FSTAB] Parsed %s: mounted %d filesystems\n", fstab_path, mounts_done);
    return mounts_done;
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
