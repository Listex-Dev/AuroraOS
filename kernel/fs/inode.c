#include <aurora/fs/vfs.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>

static ino_t next_ino = 1;

struct inode *inode_alloc(struct super_block *sb)
{
    struct inode *ino = NULL;
    if (sb && sb->s_op && sb->s_op->alloc_inode) {
        ino = sb->s_op->alloc_inode(sb);
    } else {
        ino = (struct inode *)kzalloc(sizeof(struct inode), GFP_KERNEL);
    }

    if (!ino) {
        return NULL;
    }

    ino->i_ino = next_ino++;
    ino->i_count = 1;
    ino->i_nlink = 1;
    ino->i_sb = sb;
    return ino;
}

void inode_free(struct inode *inode)
{
    if (!inode) {
        return;
    }

    if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->destroy_inode) {
        inode->i_sb->s_op->destroy_inode(inode);
    } else {
        kfree(inode);
    }
}
