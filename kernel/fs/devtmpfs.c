#include <aurora/fs/devtmpfs.h>
#include <aurora/drivers/block.h>
#include <aurora/mm/heap.h>
#include <aurora/serial.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

static struct super_block *dev_sb = NULL;

/* 1. /dev/null */
static ssize_t null_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)buf; (void)count; (void)pos;
    return 0; /* EOF */
}

static ssize_t null_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)buf; (void)pos;
    return (ssize_t)count; /* Discard bytes */
}

static struct file_operations null_fops = {
    .read = null_read,
    .write = null_write,
};

/* 2. /dev/zero */
static ssize_t zero_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)pos;
    if (!buf) return -EFAULT;
    memset(buf, 0, count);
    return (ssize_t)count;
}

static ssize_t zero_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)buf; (void)pos;
    return (ssize_t)count;
}

static struct file_operations zero_fops = {
    .read = zero_read,
    .write = zero_write,
};

/* 3. /dev/urandom */
static uint64_t rng_state = 0x853c49e6748fea9bULL;

static uint64_t xorshift64(void)
{
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return rng_state = x;
}

static ssize_t urandom_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)pos;
    if (!buf) return -EFAULT;
    uint8_t *b = (uint8_t *)buf;
    for (size_t i = 0; i < count; i++) {
        b[i] = (uint8_t)(xorshift64() & 0xFF);
    }
    return (ssize_t)count;
}

static ssize_t urandom_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)pos;
    if (buf && count >= 8) {
        rng_state ^= *(const uint64_t *)buf;
    }
    return (ssize_t)count;
}

static struct file_operations urandom_fops = {
    .read = urandom_read,
    .write = urandom_write,
};

/* 4. /dev/console & /dev/tty */
static ssize_t console_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)pos;
    if (!buf) return -EFAULT;
    char *cbuf = (char *)buf;
    for (size_t i = 0; i < count; i++) {
        cbuf[i] = serial_getc();
    }
    return (ssize_t)count;
}

static ssize_t console_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)file; (void)pos;
    if (!buf) return -EFAULT;
    serial_write((const char *)buf, count);
    return (ssize_t)count;
}

static struct file_operations console_fops = {
    .read = console_read,
    .write = console_write,
};

int devtmpfs_register_chrdev(uint32_t major, uint32_t minor, const char *name,
                            struct file_operations *fops, mode_t mode)
{
    if (!dev_sb || !name || !fops) {
        return -EINVAL;
    }

    /* Update existing device node if present */
    for (struct dentry *c = dev_sb->s_root->d_subdirs; c != NULL; c = c->d_next) {
        if (strcmp(c->d_name, name) == 0 && c->d_inode) {
            c->d_inode->i_mode = S_IFCHR | (mode & 0777);
            c->d_inode->i_fop = fops;
            c->d_inode->i_rdev = (major << 8) | (minor & 0xFF);
            return 0;
        }
    }

    struct inode *inode = inode_alloc(dev_sb);
    if (!inode) {
        return -ENOMEM;
    }

    inode->i_mode = S_IFCHR | (mode & 0777);
    inode->i_fop = fops;
    inode->i_rdev = (major << 8) | (minor & 0xFF);

    struct dentry *d = dentry_create(name, dev_sb->s_root, inode);
    if (!d) {
        inode_free(inode);
        return -ENOMEM;
    }

    return 0;
}

int devtmpfs_register_blkdev(uint32_t major, uint32_t minor, const char *name,
                            struct file_operations *fops, mode_t mode)
{
    if (!dev_sb || !name || !fops) {
        return -EINVAL;
    }

    /* Update existing device node if present */
    for (struct dentry *c = dev_sb->s_root->d_subdirs; c != NULL; c = c->d_next) {
        if (strcmp(c->d_name, name) == 0 && c->d_inode) {
            c->d_inode->i_mode = S_IFBLK | (mode & 0777);
            c->d_inode->i_fop = fops;
            c->d_inode->i_rdev = (major << 8) | (minor & 0xFF);
            return 0;
        }
    }

    struct inode *inode = inode_alloc(dev_sb);
    if (!inode) {
        return -ENOMEM;
    }

    inode->i_mode = S_IFBLK | (mode & 0777);
    inode->i_fop = fops;
    inode->i_rdev = (major << 8) | (minor & 0xFF);

    struct dentry *d = dentry_create(name, dev_sb->s_root, inode);
    if (!d) {
        inode_free(inode);
        return -ENOMEM;
    }

    return 0;
}

struct super_block *devtmpfs_create(void)
{
    struct super_block *sb = (struct super_block *)kzalloc(sizeof(struct super_block), GFP_KERNEL);
    if (!sb) {
        return NULL;
    }

    sb->s_blocksize = 1024;
    sb->s_type = "devtmpfs";

    struct inode *root_inode = inode_alloc(sb);
    if (!root_inode) {
        kfree(sb);
        return NULL;
    }
    root_inode->i_mode = S_IFDIR | 0755;

    struct dentry *root_dentry = dentry_create("dev", NULL, root_inode);
    if (!root_dentry) {
        kfree(root_inode);
        kfree(sb);
        return NULL;
    }
    sb->s_root = root_dentry;
    dev_sb = sb;

    /* Register canonical character devices (Section 8.2) */
    devtmpfs_register_chrdev(1, 3, "null", &null_fops, 0666);
    devtmpfs_register_chrdev(1, 5, "zero", &zero_fops, 0666);
    devtmpfs_register_chrdev(1, 9, "urandom", &urandom_fops, 0666);
    devtmpfs_register_chrdev(1, 8, "random", &urandom_fops, 0666);
    devtmpfs_register_chrdev(5, 1, "console", &console_fops, 0600);
    devtmpfs_register_chrdev(5, 0, "tty", &console_fops, 0666);
    devtmpfs_register_chrdev(4, 0, "tty0", &console_fops, 0620);
    devtmpfs_register_chrdev(4, 64, "ttyS0", &console_fops, 0660);

    /* Also populate all registered block devices */
    for (struct block_device *b = bdev_get_list(); b != NULL; b = b->next) {
        uint32_t major = (uint32_t)(b->dev_id >> 8);
        uint32_t minor = (uint32_t)(b->dev_id & 0xFF);
        devtmpfs_register_blkdev(major, minor, b->name, &g_bdev_fops, 0660);
    }

    return sb;
}

struct dentry *devtmpfs_mkdir(const char *name, mode_t mode)
{
    if (!dev_sb || !name) return NULL;
    for (struct dentry *c = dev_sb->s_root->d_subdirs; c != NULL; c = c->d_next) {
        if (strcmp(c->d_name, name) == 0) return c;
    }
    struct inode *inode = inode_alloc(dev_sb);
    if (!inode) return NULL;
    inode->i_mode = S_IFDIR | (mode & 0777);
    return dentry_create(name, dev_sb->s_root, inode);
}

int devtmpfs_register_chrdev_at(struct dentry *parent, uint32_t major, uint32_t minor,
                                const char *name, struct file_operations *fops, mode_t mode)
{
    if (!dev_sb || !name || !fops || !parent) return -EINVAL;
    for (struct dentry *c = parent->d_subdirs; c != NULL; c = c->d_next) {
        if (strcmp(c->d_name, name) == 0 && c->d_inode) {
            c->d_inode->i_mode = S_IFCHR | (mode & 0777);
            c->d_inode->i_fop = fops;
            c->d_inode->i_rdev = (major << 8) | (minor & 0xFF);
            return 0;
        }
    }
    struct inode *inode = inode_alloc(dev_sb);
    if (!inode) return -ENOMEM;
    inode->i_mode = S_IFCHR | (mode & 0777);
    inode->i_fop = fops;
    inode->i_rdev = (major << 8) | (minor & 0xFF);
    struct dentry *d = dentry_create(name, parent, inode);
    if (!d) {
        inode_free(inode);
        return -ENOMEM;
    }
    return 0;
}

extern struct file_operations g_tty_fops;
extern struct file_operations g_ptmx_fops;
extern struct file_operations g_pty_slave_fops;

struct file_operations *chrdev_lookup(dev_t dev)
{
    uint32_t major = (uint32_t)(dev >> 8);
    uint32_t minor = (uint32_t)(dev & 0xFF);

    if (major == 1) {
        if (minor == 3) return &null_fops;
        if (minor == 5) return &zero_fops;
        if (minor == 8 || minor == 9) return &urandom_fops;
    } else if (major == 4) {
        return &g_tty_fops;
    } else if (major == 5) {
        if (minor == 2) return &g_ptmx_fops;
        return &g_tty_fops;
    } else if (major == 136) {
        return &g_pty_slave_fops;
    }
    return NULL;
}

