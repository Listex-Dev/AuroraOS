#include <aurora/fs/vfs.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>

struct dentry *dentry_create(const char *name, struct dentry *parent, struct inode *inode)
{
    struct dentry *d = (struct dentry *)kzalloc(sizeof(struct dentry), GFP_KERNEL);
    if (!d) {
        return NULL;
    }

    if (name) {
        strncpy(d->d_name, name, sizeof(d->d_name) - 1);
    }
    d->d_parent = parent ? parent : d;
    d->d_inode = inode;
    d->d_sb = parent ? parent->d_sb : (inode ? inode->i_sb : NULL);
    d->d_count = 1;

    if (parent && parent != d) {
        /* Link into parent's subdirs list */
        d->d_next = parent->d_subdirs;
        parent->d_subdirs = d;
    }

    return d;
}

struct dentry *dentry_lookup(struct dentry *parent, const char *name)
{
    if (!parent || !name) {
        return NULL;
    }

    /* Fast path: check in-memory dentry child list */
    for (struct dentry *child = parent->d_subdirs; child != NULL; child = child->d_next) {
        if (strcmp(child->d_name, name) == 0) {
            return child;
        }
    }

    /* Slow path: invoke underlying filesystem lookup if inode operation provided */
    if (parent->d_inode && parent->d_inode->i_op && parent->d_inode->i_op->lookup) {
        struct dentry probe;
        memset(&probe, 0, sizeof(probe));
        strncpy(probe.d_name, name, sizeof(probe.d_name) - 1);
        probe.d_parent = parent;
        probe.d_sb = parent->d_sb;
        struct dentry *res = parent->d_inode->i_op->lookup(parent->d_inode, &probe);
        if (res == &probe) {
            res = dentry_create(name, parent, probe.d_inode);
        }
        return res;
    }

    return NULL;
}

void dentry_free(struct dentry *dentry)
{
    if (!dentry) {
        return;
    }

    /* Unlink from parent if present */
    if (dentry->d_parent && dentry->d_parent != dentry) {
        struct dentry **curr = &dentry->d_parent->d_subdirs;
        while (*curr) {
            if (*curr == dentry) {
                *curr = dentry->d_next;
                break;
            }
            curr = &(*curr)->d_next;
        }
    }

    kfree(dentry);
}
