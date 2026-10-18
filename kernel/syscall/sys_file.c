#include <aurora/syscall.h>
#include <aurora/fs/vfs.h>
#include <aurora/fs/mount.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

static struct files_struct *get_current_files(void)
{
    task_t *curr = sched_current();
    if (!curr) {
        return NULL;
    }
    if (!curr->files) {
        curr->files = files_alloc();
    }
    return (struct files_struct *)curr->files;
}

int64_t sys_open(const char *filename, int flags, mode_t mode)
{
    if (!filename) {
        return -EFAULT;
    }

    struct files_struct *files = get_current_files();
    if (!files) {
        return -EMFILE;
    }

    int free_fd = -1;
    for (int i = 0; i < NR_OPEN; i++) {
        if (!files->fd[i]) {
            free_fd = i;
            break;
        }
    }

    if (free_fd < 0) {
        return -EMFILE;
    }

    struct file *file = NULL;
    int err = vfs_open(filename, flags, mode, &file);
    if (err < 0) {
        return err;
    }

    files->fd[free_fd] = file;
    files->fd_flags[free_fd] = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;

    return free_fd;
}

int64_t sys_close(int fd)
{
    if (fd < 0 || fd >= NR_OPEN) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[fd]) {
        return -EBADF;
    }

    struct file *file = files->fd[fd];
    files->fd[fd] = NULL;
    files->fd_flags[fd] = 0;

    return vfs_close(file);
}

int64_t sys_read(int fd, void *buf, size_t count)
{
    if (fd < 0 || fd >= NR_OPEN) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[fd]) {
        return -EBADF;
    }

    return vfs_read(files->fd[fd], buf, count);
}

int64_t sys_write(int fd, const void *buf, size_t count)
{
    if (fd < 0 || fd >= NR_OPEN) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[fd]) {
        return -EBADF;
    }

    return vfs_write(files->fd[fd], buf, count);
}

int64_t sys_lseek(int fd, uint64_t offset, int whence)
{
    if (fd < 0 || fd >= NR_OPEN) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[fd]) {
        return -EBADF;
    }

    return (int64_t)vfs_lseek(files->fd[fd], offset, whence);
}

int64_t sys_dup(int oldfd)
{
    if (oldfd < 0 || oldfd >= NR_OPEN) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[oldfd]) {
        return -EBADF;
    }

    int free_fd = -1;
    for (int i = 0; i < NR_OPEN; i++) {
        if (!files->fd[i]) {
            free_fd = i;
            break;
        }
    }

    if (free_fd < 0) {
        return -EMFILE;
    }

    files->fd[free_fd] = files->fd[oldfd];
    files->fd[free_fd]->f_count++;
    files->fd_flags[free_fd] = 0;

    return free_fd;
}

int64_t sys_dup2(int oldfd, int newfd)
{
    if (oldfd < 0 || oldfd >= NR_OPEN || newfd < 0 || newfd >= NR_OPEN) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[oldfd]) {
        return -EBADF;
    }

    if (oldfd == newfd) {
        return newfd;
    }

    if (files->fd[newfd]) {
        vfs_close(files->fd[newfd]);
        files->fd[newfd] = NULL;
    }

    files->fd[newfd] = files->fd[oldfd];
    files->fd[newfd]->f_count++;
    files->fd_flags[newfd] = 0;

    return newfd;
}

int64_t sys_stat(const char *filename, struct stat *statbuf)
{
    return vfs_stat(filename, statbuf);
}

int64_t sys_fstat(int fd, struct stat *statbuf)
{
    if (fd < 0 || fd >= NR_OPEN) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[fd]) {
        return -EBADF;
    }

    return vfs_fstat(files->fd[fd], statbuf);
}

int64_t sys_mkdir(const char *pathname, mode_t mode)
{
    return vfs_mkdir(pathname, mode);
}

int64_t sys_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    return vfs_mknod(pathname, mode, dev);
}

int64_t sys_unlink(const char *pathname)
{
    return vfs_unlink(pathname);
}

int64_t sys_pivot_root(const char *new_root, const char *put_old)
{
    return vfs_pivot_root(new_root, put_old);
}

int64_t sys_mount(const char *dev_name, const char *dir_name, const char *type, unsigned long flags, const void *data)
{
    return vfs_mount(dev_name, dir_name, type, (uint32_t)flags, data);
}

int64_t sys_ioctl(int fd, unsigned long cmd, void *arg)
{
    if (fd < 0 || fd >= NR_OPEN) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[fd]) {
        return -EBADF;
    }

    struct file *file = files->fd[fd];
    if (file->f_op && file->f_op->ioctl) {
        return file->f_op->ioctl(file, (unsigned int)cmd, (unsigned long)arg);
    }

    return -ENOTTY;
}

struct linux_dirent64 {
    uint64_t        d_ino;
    int64_t         d_off;
    unsigned short  d_reclen;
    unsigned char   d_type;
    char            d_name[];
};

int64_t sys_getdents64(int fd, void *dirp, size_t count)
{
    if (fd < 0 || fd >= NR_OPEN || !dirp || count == 0) {
        return -EBADF;
    }

    struct files_struct *files = get_current_files();
    if (!files || !files->fd[fd]) {
        return -EBADF;
    }

    struct file *file = files->fd[fd];
    if (!file->f_dentry || !file->f_inode || !S_ISDIR(file->f_inode->i_mode)) {
        return -ENOTDIR;
    }

    struct dentry *dir_dentry = file->f_dentry;
    if (dir_dentry->d_mounted) {
        dir_dentry = dir_dentry->d_mounted->mnt_root;
    }

    uint8_t *out = (uint8_t *)dirp;
    size_t bytes_written = 0;
    uint64_t current_idx = 0;

    /* Entry 0: "." */
    if (file->f_pos <= 0) {
        const char *name = ".";
        size_t namelen = 1;
        unsigned short reclen = (unsigned short)((sizeof(uint64_t) + sizeof(int64_t) + sizeof(unsigned short) + sizeof(unsigned char) + namelen + 1 + 7) & ~7);
        if (bytes_written + reclen > count) {
            return bytes_written;
        }

        struct linux_dirent64 *d = (struct linux_dirent64 *)(out + bytes_written);
        d->d_ino = dir_dentry->d_inode ? dir_dentry->d_inode->i_ino : 1;
        d->d_off = 1;
        d->d_reclen = reclen;
        d->d_type = 4; /* DT_DIR */
        memcpy(d->d_name, name, namelen + 1);

        bytes_written += reclen;
        file->f_pos = 1;
    }

    /* Entry 1: ".." */
    if (file->f_pos <= 1) {
        const char *name = "..";
        size_t namelen = 2;
        unsigned short reclen = (unsigned short)((sizeof(uint64_t) + sizeof(int64_t) + sizeof(unsigned short) + sizeof(unsigned char) + namelen + 1 + 7) & ~7);
        if (bytes_written + reclen > count) {
            return bytes_written;
        }

        struct dentry *parent = dir_dentry->d_parent ? dir_dentry->d_parent : dir_dentry;
        struct linux_dirent64 *d = (struct linux_dirent64 *)(out + bytes_written);
        d->d_ino = parent->d_inode ? parent->d_inode->i_ino : 1;
        d->d_off = 2;
        d->d_reclen = reclen;
        d->d_type = 4; /* DT_DIR */
        memcpy(d->d_name, name, namelen + 1);

        bytes_written += reclen;
        file->f_pos = 2;
    }

    /* Iterate child dentries from dir_dentry->d_subdirs */
    current_idx = 2;
    for (struct dentry *child = dir_dentry->d_subdirs; child != NULL; child = child->d_next, current_idx++) {
        if (current_idx < file->f_pos) {
            continue;
        }

        size_t namelen = strlen(child->d_name);
        unsigned short reclen = (unsigned short)((sizeof(uint64_t) + sizeof(int64_t) + sizeof(unsigned short) + sizeof(unsigned char) + namelen + 1 + 7) & ~7);
        if (bytes_written + reclen > count) {
            break;
        }

        struct linux_dirent64 *d = (struct linux_dirent64 *)(out + bytes_written);
        d->d_ino = child->d_inode ? child->d_inode->i_ino : (current_idx + 100);
        d->d_off = current_idx + 1;
        d->d_reclen = reclen;
        if (child->d_inode && S_ISDIR(child->d_inode->i_mode)) {
            d->d_type = 4; /* DT_DIR */
        } else if (child->d_inode && S_ISCHR(child->d_inode->i_mode)) {
            d->d_type = 2; /* DT_CHR */
        } else if (child->d_inode && S_ISBLK(child->d_inode->i_mode)) {
            d->d_type = 6; /* DT_BLK */
        } else {
            d->d_type = 8; /* DT_REG */
        }
        memcpy(d->d_name, child->d_name, namelen + 1);

        bytes_written += reclen;
        file->f_pos = current_idx + 1;
    }

    return (int64_t)bytes_written;
}

int64_t sys_chdir(const char *pathname)
{
    if (!pathname) {
        return -EFAULT;
    }
    struct dentry *d = vfs_namei(pathname);
    if (!d || !d->d_inode) {
        return -ENOENT;
    }
    if (!S_ISDIR(d->d_inode->i_mode)) {
        return -ENOTDIR;
    }
    task_t *curr = sched_current();
    if (curr && curr->fs) {
        struct fs_struct *fs = (struct fs_struct *)curr->fs;
        fs->pwd = d;
    }
    return 0;
}

int64_t sys_getcwd(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return -EINVAL;
    }
    task_t *curr = sched_current();
    struct dentry *pwd = (curr && curr->fs && ((struct fs_struct *)curr->fs)->pwd) ?
                          ((struct fs_struct *)curr->fs)->pwd : vfs_get_root();

    char path[256];
    char temp[256];
    path[0] = '\0';

    struct dentry *d = pwd;

    while (d && d != vfs_get_root()) {
        struct dentry *mp = vfs_get_mountpoint_for_root(d);
        if (mp) {
            d = mp;
        }
        if (d == vfs_get_root() || d->d_parent == d) {
            break;
        }
        snprintf(temp, sizeof(temp), "/%s%s", d->d_name, path);
        strncpy(path, temp, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        d = d->d_parent;
    }

    if (path[0] == '\0') {
        strncpy(path, "/", sizeof(path) - 1);
    }

    if (strlen(path) + 1 > size) {
        return -ERANGE;
    }

    strncpy(buf, path, size - 1);
    buf[size - 1] = '\0';
    return (int64_t)strlen(buf);
}

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

int64_t sys_uname(struct utsname *buf)
{
    if (!buf) {
        return -EFAULT;
    }
    memset(buf, 0, sizeof(struct utsname));
    strncpy(buf->sysname, "AuroraOS", 64);
    strncpy(buf->nodename, "aurora", 64);
    strncpy(buf->release, "1.0.0", 64);
    strncpy(buf->version, "#1 AuroraOS x86-64 Monolithic", 64);
    strncpy(buf->machine, "x86_64", 64);
    strncpy(buf->domainname, "(none)", 64);
    return 0;
}

/* Pipe buffer definition */
struct pipe_buf {
    char data[4096];
    size_t head;
    size_t tail;
    size_t count;
    int readers;
    int writers;
};

static ssize_t pipe_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    if (!file->f_inode || !file->f_inode->i_private) return -EINVAL;
    struct pipe_buf *pipe = (struct pipe_buf *)file->f_inode->i_private;
    while (pipe->count == 0) {
        if (pipe->writers <= 0) return 0; /* EOF: all writers closed */
        sched_yield();
    }
    size_t to_read = count < pipe->count ? count : pipe->count;
    char *out = (char *)buf;
    for (size_t i = 0; i < to_read; i++) {
        out[i] = pipe->data[pipe->head];
        pipe->head = (pipe->head + 1) % sizeof(pipe->data);
    }
    pipe->count -= to_read;
    return (ssize_t)to_read;
}

static ssize_t pipe_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    if (!file->f_inode || !file->f_inode->i_private) return -EINVAL;
    struct pipe_buf *pipe = (struct pipe_buf *)file->f_inode->i_private;
    if (pipe->readers <= 0) return -EPIPE;
    const char *in = (const char *)buf;
    size_t written = 0;
    while (written < count) {
        if (pipe->readers <= 0) return -EPIPE;
        if (pipe->count >= sizeof(pipe->data)) {
            sched_yield();
            continue;
        }
        pipe->data[pipe->tail] = in[written++];
        pipe->tail = (pipe->tail + 1) % sizeof(pipe->data);
        pipe->count++;
    }
    return (ssize_t)written;
}

static int pipe_release(struct inode *inode, struct file *file)
{
    if (inode && inode->i_private) {
        struct pipe_buf *pipe = (struct pipe_buf *)inode->i_private;
        if ((file->f_flags & O_ACCMODE) == O_RDONLY) pipe->readers--;
        else if ((file->f_flags & O_ACCMODE) == O_WRONLY) pipe->writers--;
        if (pipe->readers <= 0 && pipe->writers <= 0) {
            kfree(pipe);
            inode->i_private = NULL;
        }
    }
    return 0;
}

static struct file_operations pipe_fops = {
    .read = pipe_read,
    .write = pipe_write,
    .release = pipe_release,
};

int64_t sys_pipe(int pipefd[2])
{
    if (!pipefd) return -EFAULT;

    struct files_struct *files = get_current_files();
    if (!files) return -EMFILE;

    int rfd = -1, wfd = -1;
    for (int i = 0; i < NR_OPEN; i++) {
        if (!files->fd[i]) {
            if (rfd < 0) rfd = i;
            else if (wfd < 0) { wfd = i; break; }
        }
    }
    if (rfd < 0 || wfd < 0) return -EMFILE;

    struct pipe_buf *pipe = (struct pipe_buf *)kzalloc(sizeof(struct pipe_buf), GFP_KERNEL);
    if (!pipe) return -ENOMEM;
    pipe->readers = 1;
    pipe->writers = 1;

    struct inode *inode = inode_alloc(NULL);
    if (!inode) { kfree(pipe); return -ENOMEM; }
    inode->i_mode = S_IFIFO | 0666;
    inode->i_fop = &pipe_fops;
    inode->i_private = pipe;

    struct file *rfile = (struct file *)kzalloc(sizeof(struct file), GFP_KERNEL);
    struct file *wfile = (struct file *)kzalloc(sizeof(struct file), GFP_KERNEL);
    if (!rfile || !wfile) {
        if (rfile) kfree(rfile);
        if (wfile) kfree(wfile);
        kfree(inode);
        kfree(pipe);
        return -ENOMEM;
    }

    rfile->f_inode = inode;
    rfile->f_op = &pipe_fops;
    rfile->f_flags = O_RDONLY;
    rfile->f_count = 1;

    wfile->f_inode = inode;
    wfile->f_op = &pipe_fops;
    wfile->f_flags = O_WRONLY;
    wfile->f_count = 1;

    files->fd[rfd] = rfile;
    files->fd[wfd] = wfile;

    pipefd[0] = rfd;
    pipefd[1] = wfd;
    return 0;
}

