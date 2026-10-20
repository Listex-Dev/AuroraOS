#include <aurora/fs/vfs.h>
#include <aurora/fs/ramfs.h>
#include <aurora/fs/mount.h>
#include <aurora/fs/devtmpfs.h>
#include <aurora/drivers/block.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/heap.h>
#include <aurora/serial.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

static struct super_block *root_sb = NULL;

/* 1. Console File Operations */
static ssize_t console_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)pos;
    if (!buf) {
        return -EFAULT;
    }
    serial_write((const char *)buf, count);
    return (ssize_t)count;
}

static ssize_t console_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)pos;
    if (!buf) {
        return -EFAULT;
    }
    char *cbuf = (char *)buf;
    for (size_t i = 0; i < count; i++) {
        cbuf[i] = serial_getc();
    }
    return (ssize_t)count;
}

struct file_operations g_console_fops = {
    .read = console_read,
    .write = console_write,
};

struct dentry *vfs_get_root(void)
{
    return root_sb ? root_sb->s_root : NULL;
}

void vfs_set_root(struct super_block *sb)
{
    root_sb = sb;
}

void vfs_init(void)
{
    root_sb = ramfs_create();
    if (!root_sb) {
        printk("<0>[VFS] FAILED: Could not create root RamFS!\n");
        return;
    }

    /* Create standard initial mountpoint and system directories */
    vfs_mkdir("/dev", 0755);
    vfs_mkdir("/proc", 0555);
    vfs_mkdir("/sys", 0755);
    vfs_mkdir("/bin", 0755);
    vfs_mkdir("/tmp", 0777);
    vfs_mkdir("/run", 0755);
    vfs_mkdir("/etc", 0755);
    vfs_mkdir("/mnt", 0755);

    /* Mount synthetic and temporary filesystems */
    vfs_mount("proc", "/proc", "procfs", 0, NULL);
    vfs_mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    vfs_mount("sysfs", "/sys", "sysfs", 0, NULL);
    vfs_mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
    vfs_mount("tmpfs", "/run", "tmpfs", 0, NULL);

    printk("<6>[  0.000000] VFS: Virtual File System initialized with root RamFS at /\n");
}

int inode_permission(struct inode *inode, int mask)
{
    if (!inode) {
        return -EINVAL;
    }

    task_t *curr = sched_current();
    if (!curr || curr->euid == 0) {
        /* Superuser bypasses read/write permissions.
         * For execute, requires at least one execute bit or directory. */
        if (mask & 1) { /* MAY_EXEC */
            if (S_ISDIR(inode->i_mode) || (inode->i_mode & 0111)) {
                return 0;
            }
            return -EACCES;
        }
        return 0;
    }

    int mode_bits = 0;
    if (curr->euid == inode->i_uid) {
        mode_bits = (inode->i_mode >> 6) & 7;
    } else if (curr->egid == inode->i_gid) {
        mode_bits = (inode->i_mode >> 3) & 7;
    } else {
        mode_bits = inode->i_mode & 7;
    }

    if ((mask & 4) && !(mode_bits & 4)) return -EACCES; /* MAY_READ */
    if ((mask & 2) && !(mode_bits & 2)) return -EACCES; /* MAY_WRITE */
    if ((mask & 1) && !(mode_bits & 1)) return -EACCES; /* MAY_EXEC */
    return 0;
}

int vfs_open(const char *path, int flags, mode_t mode, struct file **res)
{
    if (!path || !res) {
        return -EFAULT;
    }

    struct dentry *dentry = vfs_namei(path);

    if (!dentry) {
        if (!(flags & O_CREAT)) {
            return -ENOENT;
        }

        /* Create new file in parent directory */
        char last_comp[128];
        struct dentry *parent = vfs_namei_parent(path, last_comp);
        if (!parent || !parent->d_inode) {
            return -ENOENT;
        }

        /* Check write permission on parent directory */
        int perr = inode_permission(parent->d_inode, 2);
        if (perr < 0) {
            return perr;
        }

        if (!parent->d_inode->i_op || !parent->d_inode->i_op->create) {
            return -EPERM;
        }

        dentry = dentry_create(last_comp, parent, NULL);
        if (!dentry) {
            return -ENOMEM;
        }

        task_t *curr = sched_current();
        mode_t effective_mode = mode;
        if (curr) {
            effective_mode = mode & ~curr->umask;
        }

        int err = parent->d_inode->i_op->create(parent->d_inode, dentry, effective_mode);
        if (err < 0) {
            dentry_free(dentry);
            return err;
        }

        if (dentry->d_inode) {
            if (curr) {
                dentry->d_inode->i_uid = curr->euid;
                dentry->d_inode->i_gid = curr->egid;
            }
            dentry->d_inode->i_mode = (dentry->d_inode->i_mode & ~07777) | (effective_mode & 07777);
            if (dentry->d_inode->i_sb && dentry->d_inode->i_sb->s_op && dentry->d_inode->i_sb->s_op->write_inode) {
                dentry->d_inode->i_sb->s_op->write_inode(dentry->d_inode);
            }
        }
    } else if ((flags & O_CREAT) && (flags & O_EXCL)) {
        return -EEXIST;
    }

    struct inode *inode = dentry->d_inode;
    if (!inode) {
        return -ENOENT;
    }

    /* Check DAC permissions for requested open access mode */
    int req_mask = 0;
    int acc = (flags & O_ACCMODE);
    if (acc == O_RDONLY) req_mask |= 4;
    else if (acc == O_WRONLY) req_mask |= 2;
    else if (acc == O_RDWR) req_mask |= 6;
    if (flags & (O_TRUNC | O_APPEND)) req_mask |= 2;

    int perr = inode_permission(inode, req_mask);
    if (perr < 0) {
        return perr;
    }

    if (S_ISDIR(inode->i_mode) && ((flags & O_ACCMODE) != O_RDONLY)) {
        return -EISDIR;
    }

    if ((flags & O_TRUNC) && S_ISREG(inode->i_mode) && ((flags & O_ACCMODE) != O_RDONLY)) {
        inode->i_size = 0;
        if (inode->i_op && inode->i_op->truncate) {
            inode->i_op->truncate(inode);
        }
    }

    struct file *file = (struct file *)kzalloc(sizeof(struct file), GFP_KERNEL);
    if (!file) {
        return -ENOMEM;
    }

    file->f_dentry = dentry;
    file->f_inode = inode;
    file->f_flags = flags;
    file->f_mode = mode;
    file->f_op = inode->i_fop;
    if (!file->f_op) {
        if (S_ISBLK(inode->i_mode)) {
            file->f_op = &g_bdev_fops;
        } else if (S_ISCHR(inode->i_mode)) {
            file->f_op = chrdev_lookup(inode->i_rdev);
        }
    }
    file->f_count = 1;
    file->f_pos = (flags & O_APPEND) ? inode->i_size : 0;

    if (file->f_op && file->f_op->open) {
        int err = file->f_op->open(inode, file);
        if (err < 0) {
            kfree(file);
            return err;
        }
    }

    *res = file;
    return 0;
}

int vfs_close(struct file *file)
{
    if (!file) {
        return -EBADF;
    }

    file->f_count--;
    if (file->f_count <= 0) {
        if ((file->f_flags & (O_WRONLY | O_RDWR)) && file->f_inode) {
            if (file->f_inode->i_sb && file->f_inode->i_sb->s_op && file->f_inode->i_sb->s_op->write_inode) {
                file->f_inode->i_sb->s_op->write_inode(file->f_inode);
            }
        }
        if (file->f_op && file->f_op->release) {
            file->f_op->release(file->f_inode, file);
        }
        kfree(file);
    }

    return 0;
}

ssize_t vfs_read(struct file *file, void *buf, size_t count)
{
    if (!file || !file->f_op) {
        return -EBADF;
    }

    if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
        return -EBADF;
    }

    if (!file->f_op->read) {
        return -EINVAL;
    }

    return file->f_op->read(file, buf, count, &file->f_pos);
}

ssize_t vfs_write(struct file *file, const void *buf, size_t count)
{
    if (!file || !file->f_op) {
        return -EBADF;
    }

    if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
        return -EBADF;
    }

    if (!file->f_op->write) {
        return -EINVAL;
    }

    if (file->f_flags & O_APPEND && file->f_inode) {
        file->f_pos = file->f_inode->i_size;
    }

    return file->f_op->write(file, buf, count, &file->f_pos);
}

uint64_t vfs_lseek(struct file *file, uint64_t offset, int whence)
{
    if (!file || !file->f_op) {
        return (uint64_t)-EBADF;
    }

    if (file->f_op->llseek) {
        return file->f_op->llseek(file, offset, whence);
    }

    return (uint64_t)-ESPIPE;
}

int vfs_stat(const char *path, struct stat *statbuf)
{
    if (!path || !statbuf) {
        return -EFAULT;
    }

    struct dentry *dentry = vfs_namei(path);
    if (!dentry || !dentry->d_inode) {
        return -ENOENT;
    }

    struct inode *inode = dentry->d_inode;
    memset(statbuf, 0, sizeof(struct stat));
    statbuf->st_ino = inode->i_ino;
    statbuf->st_mode = inode->i_mode;
    statbuf->st_nlink = inode->i_nlink;
    statbuf->st_uid = inode->i_uid;
    statbuf->st_gid = inode->i_gid;
    statbuf->st_rdev = inode->i_rdev;
    statbuf->st_size = inode->i_size;
    statbuf->st_blksize = 4096;
    statbuf->st_atime = inode->i_atime;
    statbuf->st_mtime = inode->i_mtime;
    statbuf->st_ctime = inode->i_ctime;

    return 0;
}

int vfs_fstat(struct file *file, struct stat *statbuf)
{
    if (!file || !statbuf) {
        return -EBADF;
    }

    struct inode *inode = file->f_inode;
    if (!inode) {
        /* Virtual character device / console stream */
        memset(statbuf, 0, sizeof(struct stat));
        statbuf->st_mode = S_IFCHR | 0666;
        statbuf->st_rdev = (5 << 8) | 1; /* console */
        statbuf->st_blksize = 1024;
        return 0;
    }

    memset(statbuf, 0, sizeof(struct stat));
    statbuf->st_ino = inode->i_ino;
    statbuf->st_mode = inode->i_mode;
    statbuf->st_nlink = inode->i_nlink;
    statbuf->st_uid = inode->i_uid;
    statbuf->st_gid = inode->i_gid;
    statbuf->st_rdev = inode->i_rdev;
    statbuf->st_size = inode->i_size;
    statbuf->st_blksize = 4096;
    statbuf->st_atime = inode->i_atime;
    statbuf->st_mtime = inode->i_mtime;
    statbuf->st_ctime = inode->i_ctime;

    return 0;
}

int vfs_mkdir(const char *path, mode_t mode)
{
    if (!path) {
        return -EFAULT;
    }

    char last_comp[128];
    struct dentry *parent = vfs_namei_parent(path, last_comp);
    if (!parent || !parent->d_inode) {
        return -ENOENT;
    }

    if (dentry_lookup(parent, last_comp) != NULL) {
        return -EEXIST;
    }

    if (!parent->d_inode->i_op || !parent->d_inode->i_op->mkdir) {
        return -EPERM;
    }

    task_t *curr = sched_current();
    if (parent->d_inode) {
        int perr = inode_permission(parent->d_inode, 2);
        if (perr < 0) return perr;
    }

    mode_t effective_mode = mode;
    if (curr) {
        effective_mode = mode & ~curr->umask;
    }

    struct dentry *child = dentry_create(last_comp, parent, NULL);
    if (!child) {
        return -ENOMEM;
    }

    int err = parent->d_inode->i_op->mkdir(parent->d_inode, child, effective_mode);
    if (err < 0) {
        dentry_free(child);
        return err;
    }

    if (child->d_inode) {
        if (curr) {
            child->d_inode->i_uid = curr->euid;
            child->d_inode->i_gid = curr->egid;
        }
        child->d_inode->i_mode = (child->d_inode->i_mode & ~07777) | (effective_mode & 07777);
        if (child->d_inode->i_sb && child->d_inode->i_sb->s_op && child->d_inode->i_sb->s_op->write_inode) {
            child->d_inode->i_sb->s_op->write_inode(child->d_inode);
        }
    }

    return 0;
}

int vfs_unlink(const char *path)
{
    if (!path) {
        return -EFAULT;
    }

    char last_comp[128];
    struct dentry *parent = vfs_namei_parent(path, last_comp);
    if (!parent || !parent->d_inode) {
        return -ENOENT;
    }

    struct dentry *target = dentry_lookup(parent, last_comp);
    if (!target) {
        return -ENOENT;
    }

    if (!parent->d_inode->i_op || !parent->d_inode->i_op->unlink) {
        return -EPERM;
    }

    return parent->d_inode->i_op->unlink(parent->d_inode, target);
}

int vfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    if (!path) {
        return -EFAULT;
    }

    if (vfs_namei(path) != NULL) {
        return -EEXIST;
    }

    char last_comp[128];
    struct dentry *parent = vfs_namei_parent(path, last_comp);
    if (!parent || !parent->d_inode) {
        return -ENOENT;
    }

    if (!S_ISDIR(parent->d_inode->i_mode)) {
        return -ENOTDIR;
    }

    if (parent->d_inode->i_op && parent->d_inode->i_op->mknod) {
        struct dentry *child = dentry_create(last_comp, parent, NULL);
        if (!child) return -ENOMEM;
        int err = parent->d_inode->i_op->mknod(parent->d_inode, child, mode, dev);
        if (err < 0) {
            dentry_free(child);
            return err;
        }
        return 0;
    }

    /* Fallback generic in-memory / devtmpfs inode creation */
    struct inode *inode = inode_alloc(parent->d_inode->i_sb);
    if (!inode) {
        return -ENOMEM;
    }

    inode->i_mode = mode;
    inode->i_rdev = dev;
    task_t *curr = sched_current();
    inode->i_uid = curr ? curr->euid : 0;
    inode->i_gid = curr ? curr->egid : 0;

    if (S_ISCHR(mode)) {
        inode->i_fop = chrdev_lookup(dev);
    } else if (S_ISBLK(mode)) {
        inode->i_fop = &g_bdev_fops;
    }

    struct dentry *child = dentry_create(last_comp, parent, inode);
    if (!child) {
        inode_free(inode);
        return -ENOMEM;
    }

    return 0;
}

int64_t sys_chmod(const char *pathname, mode_t mode)
{
    if (!pathname) return -EFAULT;
    struct dentry *d = vfs_namei(pathname);
    if (!d || !d->d_inode) return -ENOENT;

    task_t *curr = sched_current();
    if (curr && curr->euid != 0 && curr->euid != d->d_inode->i_uid) {
        return -EPERM;
    }

    d->d_inode->i_mode = (d->d_inode->i_mode & ~07777) | (mode & 07777);
    if (d->d_inode->i_sb && d->d_inode->i_sb->s_op && d->d_inode->i_sb->s_op->write_inode) {
        d->d_inode->i_sb->s_op->write_inode(d->d_inode);
    }
    return 0;
}

int64_t sys_fchmod(int fd, mode_t mode)
{
    task_t *curr = sched_current();
    if (!curr || !curr->files) return -EBADF;

    struct files_struct *files = (struct files_struct *)curr->files;
    if (fd < 0 || fd >= NR_OPEN || !files->fd[fd]) return -EBADF;

    struct inode *inode = files->fd[fd]->f_inode;
    if (!inode) return -EBADF;

    if (curr->euid != 0 && curr->euid != inode->i_uid) {
        return -EPERM;
    }

    inode->i_mode = (inode->i_mode & ~07777) | (mode & 07777);
    if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->write_inode) {
        inode->i_sb->s_op->write_inode(inode);
    }
    return 0;
}
