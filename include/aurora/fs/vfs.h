#ifndef _AURORA_FS_VFS_H
#define _AURORA_FS_VFS_H

#include <aurora/types.h>
#include <aurora/fs/fcntl.h>
#include <aurora/fs/stat.h>

#define NR_OPEN 256

struct super_block;
struct inode;
struct dentry;
struct file;
struct vfsmount;

/* 1. File Operations */
struct file_operations {
    int (*open)(struct inode *inode, struct file *file);
    int (*release)(struct inode *inode, struct file *file);
    ssize_t (*read)(struct file *file, void *buf, size_t count, uint64_t *pos);
    ssize_t (*write)(struct file *file, const void *buf, size_t count, uint64_t *pos);
    uint64_t (*llseek)(struct file *file, uint64_t offset, int whence);
    int (*ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
};

/* 2. Inode Operations */
struct inode_operations {
    struct dentry *(*lookup)(struct inode *dir, struct dentry *dentry);
    int (*create)(struct inode *dir, struct dentry *dentry, mode_t mode);
    int (*mkdir)(struct inode *dir, struct dentry *dentry, mode_t mode);
    int (*unlink)(struct inode *dir, struct dentry *dentry);
    int (*rmdir)(struct inode *dir, struct dentry *dentry);
};

/* 3. Superblock Operations */
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *inode);
    void (*write_inode)(struct inode *inode);
};

/* 4. Superblock */
struct super_block {
    struct super_operations *s_op;
    struct dentry           *s_root;
    void                    *s_fs_info;
    dev_t                    s_dev;
    uint32_t                 s_flags;
    uint32_t                 s_blocksize;
    const char              *s_type;
};

/* 5. Inode */
struct inode {
    ino_t                    i_ino;
    mode_t                   i_mode;
    uid_t                    i_uid;
    gid_t                    i_gid;
    uint32_t                 i_nlink;
    size_t                   i_size;
    uint64_t                 i_atime;
    uint64_t                 i_mtime;
    uint64_t                 i_ctime;
    dev_t                    i_rdev;

    struct inode_operations *i_op;
    struct file_operations  *i_fop;
    struct super_block      *i_sb;

    int                      i_count;
    void                    *i_private;
};

/* 6. Directory Entry (dentry) */
struct dentry {
    char                     d_name[128];
    struct dentry           *d_parent;
    struct inode            *d_inode;
    struct super_block      *d_sb;
    int                      d_count;

    struct vfsmount         *d_mounted; /* If mounted over, points to vfsmount */
    struct dentry           *d_subdirs; /* Child list head */
    struct dentry           *d_next;    /* Sibling link */
};

/* 7. VFS Mount Record */
struct vfsmount {
    struct dentry           *mnt_mountpoint;
    struct super_block      *mnt_sb;
    struct dentry           *mnt_root;
    char                     mnt_devname[64];
    uint32_t                 mnt_flags;
    struct vfsmount         *mnt_next;
};

/* 7. Open File Object */
struct file {
    struct dentry           *f_dentry;
    struct inode            *f_inode;
    uint64_t                 f_pos;
    uint32_t                 f_flags;
    mode_t                   f_mode;
    struct file_operations  *f_op;
    int                      f_count;
    void                    *private_data;
};

/* 8. Process File Descriptors */
struct files_struct {
    struct file             *fd[NR_OPEN];
    uint32_t                 fd_flags[NR_OPEN];
    int                      count;
};

/* 9. Process Filesystem Context */
struct fs_struct {
    struct dentry           *root;
    struct dentry           *pwd;
    int                      count;
};

/* VFS Initialization and Core API */
void vfs_init(void);

struct dentry *vfs_get_root(void);
struct files_struct *files_alloc(void);
struct files_struct *files_clone(struct files_struct *old_files);
void files_free(struct files_struct *files);

struct fs_struct *fs_alloc(void);
struct fs_struct *fs_clone(struct fs_struct *old_fs);
void fs_free(struct fs_struct *fs);

struct dentry *dentry_create(const char *name, struct dentry *parent, struct inode *inode);
struct dentry *dentry_lookup(struct dentry *parent, const char *name);
void dentry_free(struct dentry *dentry);

struct inode *inode_alloc(struct super_block *sb);
void inode_free(struct inode *inode);

struct dentry *vfs_namei(const char *path);
struct dentry *vfs_namei_parent(const char *path, char *last_comp);

int vfs_open(const char *path, int flags, mode_t mode, struct file **res);
int vfs_close(struct file *file);
ssize_t vfs_read(struct file *file, void *buf, size_t count);
ssize_t vfs_write(struct file *file, const void *buf, size_t count);
uint64_t vfs_lseek(struct file *file, uint64_t offset, int whence);
int vfs_stat(const char *path, struct stat *statbuf);
int vfs_fstat(struct file *file, struct stat *statbuf);
int vfs_mkdir(const char *path, mode_t mode);
int vfs_unlink(const char *path);
int vfs_mount(const char *source, const char *target, const char *fstype, uint32_t flags, const void *data);

/* Global device operations for console */
extern struct file_operations g_console_fops;

#endif /* _AURORA_FS_VFS_H */
