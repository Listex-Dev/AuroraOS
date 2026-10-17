#include <aurora/syscall.h>
#include <aurora/fs/vfs.h>
#include <aurora/fs/mount.h>
#include <aurora/proc/sched.h>
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

