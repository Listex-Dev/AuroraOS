#include <aurora/fs/ramfs.h>
#include <aurora/fs/vfs.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/errno.h>

struct ramfs_data {
    char   *buf;
    size_t  capacity;
};

static ssize_t ramfs_file_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    struct inode *inode = file->f_inode;
    if (!inode || !inode->i_private) {
        return 0;
    }

    struct ramfs_data *data = (struct ramfs_data *)inode->i_private;
    if (*pos >= inode->i_size) {
        return 0; /* EOF */
    }

    size_t to_read = count;
    if (*pos + to_read > inode->i_size) {
        to_read = inode->i_size - *pos;
    }

    if (to_read > 0 && data->buf) {
        memcpy(buf, data->buf + *pos, to_read);
        *pos += to_read;
    }

    return (ssize_t)to_read;
}

static ssize_t ramfs_file_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    struct inode *inode = file->f_inode;
    if (!inode) {
        return -EINVAL;
    }

    struct ramfs_data *data = (struct ramfs_data *)inode->i_private;
    if (!data) {
        data = (struct ramfs_data *)kzalloc(sizeof(struct ramfs_data), GFP_KERNEL);
        if (!data) {
            return -ENOMEM;
        }
        inode->i_private = data;
    }

    size_t required = *pos + count;
    if (required > data->capacity) {
        size_t new_cap = data->capacity ? data->capacity * 2 : 4096;
        while (new_cap < required) {
            new_cap *= 2;
        }

        char *new_buf = (char *)kmalloc(new_cap, GFP_KERNEL);
        if (!new_buf) {
            return -ENOMEM;
        }

        if (data->buf) {
            memcpy(new_buf, data->buf, inode->i_size);
            kfree(data->buf);
        }
        data->buf = new_buf;
        data->capacity = new_cap;
    }

    memcpy(data->buf + *pos, buf, count);
    *pos += count;

    if (*pos > inode->i_size) {
        inode->i_size = *pos;
    }

    return (ssize_t)count;
}

static uint64_t ramfs_file_llseek(struct file *file, uint64_t offset, int whence)
{
    struct inode *inode = file->f_inode;
    int64_t new_pos = 0;

    switch (whence) {
        case SEEK_SET:
            new_pos = (int64_t)offset;
            break;
        case SEEK_CUR:
            new_pos = (int64_t)file->f_pos + (int64_t)offset;
            break;
        case SEEK_END:
            new_pos = (int64_t)(inode ? inode->i_size : 0) + (int64_t)offset;
            break;
        default:
            return (uint64_t)-EINVAL;
    }

    if (new_pos < 0) {
        return (uint64_t)-EINVAL;
    }

    file->f_pos = (uint64_t)new_pos;
    return file->f_pos;
}

static struct file_operations ramfs_file_ops = {
    .read = ramfs_file_read,
    .write = ramfs_file_write,
    .llseek = ramfs_file_llseek,
};

static int ramfs_dir_create(struct inode *dir, struct dentry *dentry, mode_t mode)
{
    struct inode *inode = inode_alloc(dir->i_sb);
    if (!inode) {
        return -ENOMEM;
    }

    inode->i_mode = S_IFREG | (mode & 0777);
    inode->i_fop = &ramfs_file_ops;
    inode->i_size = 0;

    struct ramfs_data *data = (struct ramfs_data *)kzalloc(sizeof(struct ramfs_data), GFP_KERNEL);
    if (!data) {
        inode_free(inode);
        return -ENOMEM;
    }
    inode->i_private = data;

    dentry->d_inode = inode;
    return 0;
}

static int ramfs_dir_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode);

static struct inode_operations ramfs_dir_inode_ops = {
    .create = ramfs_dir_create,
    .mkdir = NULL, /* Set dynamically */
};

static int ramfs_dir_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode)
{
    struct inode *inode = inode_alloc(dir->i_sb);
    if (!inode) {
        return -ENOMEM;
    }

    inode->i_mode = S_IFDIR | (mode & 0777);
    inode->i_op = &ramfs_dir_inode_ops;
    inode->i_size = 0;

    dentry->d_inode = inode;
    return 0;
}

static int ramfs_dir_unlink(struct inode *dir, struct dentry *dentry)
{
    (void)dir;
    if (!dentry || !dentry->d_inode) {
        return -ENOENT;
    }

    struct inode *inode = dentry->d_inode;
    if (inode->i_private) {
        struct ramfs_data *data = (struct ramfs_data *)inode->i_private;
        if (data->buf) {
            kfree(data->buf);
        }
        kfree(data);
        inode->i_private = NULL;
    }

    struct inode *del_inode = inode;
    dentry_free(dentry);
    inode_free(del_inode);
    return 0;
}

struct super_block *ramfs_create(void)
{
    ramfs_dir_inode_ops.mkdir = ramfs_dir_mkdir;
    ramfs_dir_inode_ops.unlink = ramfs_dir_unlink;

    struct super_block *sb = (struct super_block *)kzalloc(sizeof(struct super_block), GFP_KERNEL);
    if (!sb) {
        return NULL;
    }

    sb->s_blocksize = 4096;
    sb->s_type = "ramfs";

    /* Create root directory inode */
    struct inode *root_inode = inode_alloc(sb);
    if (!root_inode) {
        kfree(sb);
        return NULL;
    }

    root_inode->i_mode = S_IFDIR | 0755;
    root_inode->i_op = &ramfs_dir_inode_ops;

    /* Create root dentry */
    struct dentry *root_dentry = dentry_create("/", NULL, root_inode);
    if (!root_dentry) {
        kfree(root_inode);
        kfree(sb);
        return NULL;
    }

    sb->s_root = root_dentry;
    return sb;
}
