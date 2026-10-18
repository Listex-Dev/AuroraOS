#include <aurora/fs/sysfs.h>
#include <aurora/fs/vfs.h>
#include <aurora/drivers/pci.h>
#include <aurora/drivers/block.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

struct sysfs_node {
    sysfs_read_fn  read_fn;
    sysfs_write_fn write_fn;
    ssize_t (*read_data_fn)(char *buf, size_t max_len, void *data);
    void *data;
};

static char sysfs_hostname[64] = "aurora\n";

static ssize_t sysfs_file_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    struct inode *inode = file->f_inode;
    if (!inode || !inode->i_private) {
        return 0;
    }

    struct sysfs_node *node = (struct sysfs_node *)inode->i_private;
    char kbuf[512];
    ssize_t total = 0;

    if (node->read_data_fn) {
        total = node->read_data_fn(kbuf, sizeof(kbuf), node->data);
    } else if (node->read_fn) {
        total = node->read_fn(kbuf, sizeof(kbuf));
    } else {
        return 0;
    }

    if (total <= 0) {
        return 0;
    }

    if (*pos >= (uint64_t)total) {
        return 0;
    }

    size_t to_read = count;
    if (*pos + to_read > (size_t)total) {
        to_read = total - *pos;
    }

    memcpy(buf, kbuf + *pos, to_read);
    *pos += to_read;
    return (ssize_t)to_read;
}

static ssize_t sysfs_file_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    struct inode *inode = file->f_inode;
    if (!inode || !inode->i_private) {
        return -EINVAL;
    }

    struct sysfs_node *node = (struct sysfs_node *)inode->i_private;
    if (!node->write_fn) {
        return -EACCES;
    }

    return node->write_fn((const char *)buf, count);
}

static uint64_t sysfs_file_llseek(struct file *file, uint64_t offset, int whence)
{
    if (whence == SEEK_SET) {
        file->f_pos = offset;
        return file->f_pos;
    }
    return (uint64_t)-EINVAL;
}

static struct file_operations sysfs_file_ops = {
    .read = sysfs_file_read,
    .write = sysfs_file_write,
    .llseek = sysfs_file_llseek,
};

static ssize_t read_hostname(char *buf, size_t max_len)
{
    return snprintf(buf, max_len, "%s", sysfs_hostname);
}

static ssize_t write_hostname(const char *buf, size_t count)
{
    size_t len = count;
    if (len >= sizeof(sysfs_hostname)) {
        len = sizeof(sysfs_hostname) - 1;
    }
    memcpy(sysfs_hostname, buf, len);
    sysfs_hostname[len] = '\0';
    return (ssize_t)count;
}

static ssize_t read_version(char *buf, size_t max_len)
{
    return snprintf(buf, max_len, "1.0.0\n");
}

static ssize_t read_ostype(char *buf, size_t max_len)
{
    return snprintf(buf, max_len, "AuroraOS\n");
}

static ssize_t read_tty_active(char *buf, size_t max_len)
{
    return snprintf(buf, max_len, "tty0\n");
}

static ssize_t read_cpu_online(char *buf, size_t max_len)
{
    return snprintf(buf, max_len, "0\n");
}

static void create_sysfs_file(struct super_block *sb, struct dentry *parent, const char *name,
                              sysfs_read_fn rfn, sysfs_write_fn wfn, mode_t mode)
{
    struct inode *inode = inode_alloc(sb);
    if (!inode) return;

    inode->i_mode = S_IFREG | (mode & 0777);
    inode->i_fop = &sysfs_file_ops;

    struct sysfs_node *node = (struct sysfs_node *)kzalloc(sizeof(struct sysfs_node), GFP_KERNEL);
    if (node) {
        node->read_fn = rfn;
        node->write_fn = wfn;
        inode->i_private = node;
    }

    dentry_create(name, parent, inode);
}

static void create_sysfs_file_data(struct super_block *sb, struct dentry *parent, const char *name,
                                  ssize_t (*rfn)(char *buf, size_t max_len, void *data),
                                  void *data, mode_t mode)
{
    struct inode *inode = inode_alloc(sb);
    if (!inode) return;

    inode->i_mode = S_IFREG | (mode & 0777);
    inode->i_fop = &sysfs_file_ops;

    struct sysfs_node *node = (struct sysfs_node *)kzalloc(sizeof(struct sysfs_node), GFP_KERNEL);
    if (node) {
        node->read_data_fn = rfn;
        node->data = data;
        inode->i_private = node;
    }

    dentry_create(name, parent, inode);
}

static struct dentry *create_sysfs_dir(struct super_block *sb, struct dentry *parent, const char *name)
{
    struct inode *inode = inode_alloc(sb);
    if (!inode) return NULL;

    inode->i_mode = S_IFDIR | 0755;
    return dentry_create(name, parent, inode);
}

static ssize_t read_pci_vendor(char *buf, size_t max_len, void *data)
{
    struct pci_device *dev = (struct pci_device *)data;
    return snprintf(buf, max_len, "0x%04x\n", dev->vendor_id);
}

static ssize_t read_pci_device(char *buf, size_t max_len, void *data)
{
    struct pci_device *dev = (struct pci_device *)data;
    return snprintf(buf, max_len, "0x%04x\n", dev->device_id);
}

static ssize_t read_pci_class(char *buf, size_t max_len, void *data)
{
    struct pci_device *dev = (struct pci_device *)data;
    return snprintf(buf, max_len, "0x%02x%02x%02x\n", dev->class_code, dev->subclass, dev->prog_if);
}

static ssize_t read_pci_driver(char *buf, size_t max_len, void *data)
{
    struct pci_device *dev = (struct pci_device *)data;
    return snprintf(buf, max_len, "%s\n", dev->driver_name);
}

static ssize_t read_bdev_size(char *buf, size_t max_len, void *data)
{
    struct block_device *b = (struct block_device *)data;
    return snprintf(buf, max_len, "%llu\n", b->total_blocks);
}

struct super_block *sysfs_create(void)
{
    struct super_block *sb = (struct super_block *)kzalloc(sizeof(struct super_block), GFP_KERNEL);
    if (!sb) {
        return NULL;
    }

    sb->s_blocksize = 1024;
    sb->s_type = "sysfs";

    struct inode *root_inode = inode_alloc(sb);
    if (!root_inode) {
        kfree(sb);
        return NULL;
    }
    root_inode->i_mode = S_IFDIR | 0755;

    struct dentry *root_dentry = dentry_create("sys", NULL, root_inode);
    if (!root_dentry) {
        kfree(root_inode);
        kfree(sb);
        return NULL;
    }
    sb->s_root = root_dentry;

    /* /sys/kernel */
    struct dentry *kernel_d = create_sysfs_dir(sb, root_dentry, "kernel");
    if (kernel_d) {
        create_sysfs_file(sb, kernel_d, "hostname", read_hostname, write_hostname, 0644);
        create_sysfs_file(sb, kernel_d, "version", read_version, NULL, 0444);
        create_sysfs_file(sb, kernel_d, "ostype", read_ostype, NULL, 0444);
    }

    /* /sys/class */
    struct dentry *class_d = create_sysfs_dir(sb, root_dentry, "class");
    if (class_d) {
        struct dentry *tty_d = create_sysfs_dir(sb, class_d, "tty");
        if (tty_d) {
            create_sysfs_file(sb, tty_d, "console", read_tty_active, NULL, 0444);
            create_sysfs_file(sb, tty_d, "tty0", read_tty_active, NULL, 0444);
        }
    }

    /* /sys/block */
    struct dentry *block_d = create_sysfs_dir(sb, root_dentry, "block");
    if (block_d) {
        for (struct block_device *b = bdev_get_list(); b != NULL; b = b->next) {
            struct dentry *b_d = create_sysfs_dir(sb, block_d, b->name);
            if (b_d) {
                create_sysfs_file_data(sb, b_d, "size", read_bdev_size, b, 0444);
            }
        }
    }

    /* /sys/devices */
    struct dentry *dev_d = create_sysfs_dir(sb, root_dentry, "devices");
    if (dev_d) {
        struct dentry *sys_d = create_sysfs_dir(sb, dev_d, "system");
        if (sys_d) {
            struct dentry *cpu_d = create_sysfs_dir(sb, sys_d, "cpu");
            if (cpu_d) {
                create_sysfs_file(sb, cpu_d, "online", read_cpu_online, NULL, 0444);
            }
        }

        /* /sys/devices/pci0000:00/0000:00:xx.x/... */
        struct dentry *pci_bus_d = create_sysfs_dir(sb, dev_d, "pci0000:00");
        if (pci_bus_d) {
            for (struct pci_device *pdev = pci_get_device_list(); pdev != NULL; pdev = pdev->next) {
                char dev_name[32];
                snprintf(dev_name, sizeof(dev_name), "0000:%02x:%02x.%d", pdev->bus, pdev->slot, pdev->func);
                struct dentry *pdev_d = create_sysfs_dir(sb, pci_bus_d, dev_name);
                if (pdev_d) {
                    create_sysfs_file_data(sb, pdev_d, "vendor", read_pci_vendor, pdev, 0444);
                    create_sysfs_file_data(sb, pdev_d, "device", read_pci_device, pdev, 0444);
                    create_sysfs_file_data(sb, pdev_d, "class", read_pci_class, pdev, 0444);
                    create_sysfs_file_data(sb, pdev_d, "driver", read_pci_driver, pdev, 0444);
                }
            }
        }
    }

    return sb;
}
