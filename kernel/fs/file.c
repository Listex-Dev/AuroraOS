#include <aurora/fs/vfs.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>

struct files_struct *files_alloc(void)
{
    struct files_struct *files = (struct files_struct *)kzalloc(sizeof(struct files_struct), GFP_KERNEL);
    if (!files) {
        return NULL;
    }
    files->count = 1;

    /* Pre-populate stdin (0), stdout (1), stderr (2) with console */
    struct file *con = (struct file *)kzalloc(sizeof(struct file), GFP_KERNEL);
    if (con) {
        con->f_op = &g_console_fops;
        con->f_flags = O_RDWR;
        con->f_count = 3; /* Shared across 0, 1, 2 */

        files->fd[0] = con;
        files->fd[1] = con;
        files->fd[2] = con;
    }

    return files;
}

struct files_struct *files_clone(struct files_struct *old_files)
{
    if (!old_files) {
        return files_alloc();
    }

    struct files_struct *new_files = (struct files_struct *)kzalloc(sizeof(struct files_struct), GFP_KERNEL);
    if (!new_files) {
        return NULL;
    }
    new_files->count = 1;

    for (int i = 0; i < NR_OPEN; i++) {
        if (old_files->fd[i]) {
            new_files->fd[i] = old_files->fd[i];
            new_files->fd_flags[i] = old_files->fd_flags[i];
            new_files->fd[i]->f_count++;
        }
    }

    return new_files;
}

void files_free(struct files_struct *files)
{
    if (!files) {
        return;
    }

    for (int i = 0; i < NR_OPEN; i++) {
        if (files->fd[i]) {
            vfs_close(files->fd[i]);
            files->fd[i] = NULL;
        }
    }

    kfree(files);
}

struct fs_struct *fs_alloc(void)
{
    struct fs_struct *fs = (struct fs_struct *)kzalloc(sizeof(struct fs_struct), GFP_KERNEL);
    if (!fs) {
        return NULL;
    }
    fs->count = 1;
    fs->root = vfs_get_root();
    fs->pwd = vfs_get_root();
    return fs;
}

struct fs_struct *fs_clone(struct fs_struct *old_fs)
{
    if (!old_fs) {
        return fs_alloc();
    }

    struct fs_struct *new_fs = (struct fs_struct *)kzalloc(sizeof(struct fs_struct), GFP_KERNEL);
    if (!new_fs) {
        return NULL;
    }
    new_fs->count = 1;
    new_fs->root = old_fs->root;
    new_fs->pwd = old_fs->pwd;
    return new_fs;
}

void fs_free(struct fs_struct *fs)
{
    if (!fs) {
        return;
    }
    kfree(fs);
}
