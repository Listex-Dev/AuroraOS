#include <aurora/fs/vfs.h>
#include <aurora/proc/sched.h>
#include <aurora/string.h>

struct dentry *vfs_namei(const char *path)
{
    if (!path || !*path) {
        return NULL;
    }

    struct dentry *curr = NULL;
    task_t *task = sched_current();
    struct fs_struct *fs = task ? (struct fs_struct *)task->fs : NULL;

    if (path[0] == '/') {
        curr = (fs && fs->root) ? fs->root : vfs_get_root();
        while (*path == '/') {
            path++;
        }
    } else {
        curr = (fs && fs->pwd) ? fs->pwd : vfs_get_root();
    }

    if (curr->d_mounted) {
        curr = curr->d_mounted->mnt_root;
    }

    if (!*path) {
        return curr; /* Path was "/" */
    }

    char comp[128];
    while (*path) {
        size_t len = 0;
        while (path[len] && path[len] != '/') {
            if (len < sizeof(comp) - 1) {
                comp[len] = path[len];
            }
            len++;
        }
        comp[len < sizeof(comp) ? len : sizeof(comp) - 1] = '\0';
        path += len;
        while (*path == '/') {
            path++;
        }

        if (strcmp(comp, ".") == 0) {
            continue;
        } else if (strcmp(comp, "..") == 0) {
            curr = curr->d_parent;
        } else {
            struct dentry *next = dentry_lookup(curr, comp);
            if (!next) {
                return NULL;
            }
            curr = next;
            if (curr->d_mounted) {
                curr = curr->d_mounted->mnt_root;
            }
        }
    }

    return curr;
}

struct dentry *vfs_namei_parent(const char *path, char *last_comp)
{
    if (!path || !*path) {
        return NULL;
    }

    /* Find the last slash */
    const char *last_slash = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    if (!last_slash) {
        /* No slash: file in current working directory */
        task_t *task = sched_current();
        struct fs_struct *fs = task ? (struct fs_struct *)task->fs : NULL;
        struct dentry *curr = (fs && fs->pwd) ? fs->pwd : vfs_get_root();
        if (last_comp) {
            strncpy(last_comp, path, 127);
            last_comp[127] = '\0';
        }
        return curr;
    }

    if (last_slash == path) {
        /* Parent is root "/" */
        if (last_comp) {
            strncpy(last_comp, last_slash + 1, 127);
            last_comp[127] = '\0';
        }
        return vfs_get_root();
    }

    /* Copy parent path into temporary buffer */
    char parent_path[256];
    size_t parent_len = last_slash - path;
    if (parent_len >= sizeof(parent_path)) {
        parent_len = sizeof(parent_path) - 1;
    }
    memcpy(parent_path, path, parent_len);
    parent_path[parent_len] = '\0';

    if (last_comp) {
        strncpy(last_comp, last_slash + 1, 127);
        last_comp[127] = '\0';
    }

    return vfs_namei(parent_path);
}
