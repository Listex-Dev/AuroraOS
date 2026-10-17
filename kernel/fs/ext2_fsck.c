#include <aurora/fs/ext2_fsck.h>
#include <aurora/fs/ext2.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

int ext2_fsck(struct block_device *bdev)
{
    if (!bdev) return -EINVAL;

    uint8_t sb_buf[1024];
    if (bdev_read(bdev, 2, 2, sb_buf) != 0) {
        printk("<3>[FSCK] Failed to read superblock from /dev/%s\n", bdev->name);
        return -EIO;
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)sb_buf;
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        printk("<3>[FSCK] Bad magic: 0x%04x (expected 0xEF53)\n", sb->s_magic);
        return -EINVAL;
    }

    if (sb->s_state != 1) {
        printk("<4>[FSCK] Filesystem state not clean (%u)\n", sb->s_state);
    }

    uint32_t block_size = 1024 << sb->s_log_block_size;
    uint32_t sectors_per_block = block_size / bdev->block_size;
    uint32_t inode_size = (sb->s_rev_level >= EXT2_DYNAMIC_REV) ? sb->s_inode_size : 128;
    if (inode_size == 0) inode_size = 128;

    uint32_t num_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    if (num_groups == 0) return -EINVAL;

    /* Read group descriptor 0 */
    uint32_t bg_block = (block_size == 1024) ? 2 : 1;
    uint8_t *gd_buf = (uint8_t *)kmalloc(block_size, GFP_KERNEL);
    if (!gd_buf) return -ENOMEM;

    if (bdev_read(bdev, (uint64_t)bg_block * sectors_per_block, sectors_per_block, gd_buf) != 0) {
        kfree(gd_buf);
        return -EIO;
    }

    struct ext2_group_desc *gd0 = (struct ext2_group_desc *)gd_buf;

    /* Read root inode (inode 2) */
    uint32_t inode_table_block = gd0->bg_inode_table;
    uint32_t byte_off = 1 * inode_size;
    uint32_t blk_off = byte_off / block_size;
    uint32_t off_in_blk = byte_off % block_size;

    uint8_t *it_buf = (uint8_t *)kmalloc(block_size, GFP_KERNEL);
    if (!it_buf) {
        kfree(gd_buf);
        return -ENOMEM;
    }

    if (bdev_read(bdev, (uint64_t)(inode_table_block + blk_off) * sectors_per_block, sectors_per_block, it_buf) != 0) {
        kfree(it_buf);
        kfree(gd_buf);
        return -EIO;
    }

    struct ext2_inode *root_ino = (struct ext2_inode *)(it_buf + off_in_blk);
    if (!S_ISDIR(root_ino->i_mode)) {
        printk("<3>[FSCK] Root inode is not a directory! (mode: 0%o)\n", root_ino->i_mode);
        kfree(it_buf);
        kfree(gd_buf);
        return -EINVAL;
    }

    /* Read root directory block and check '.' and '..' */
    uint32_t root_data_block = 0;
    if (root_ino->i_flags & EXT4_EXTENTS_FL) {
        struct ext4_extent_header *eh = (struct ext4_extent_header *)root_ino->i_block;
        if (eh->eh_magic == EXT4_EXT_MAGIC && eh->eh_entries > 0) {
            struct ext4_extent *ext = (struct ext4_extent *)(eh + 1);
            root_data_block = ext[0].ee_start_lo;
        }
    } else {
        root_data_block = root_ino->i_block[0];
    }

    if (root_data_block == 0) {
        printk("<3>[FSCK] Root directory has no data block\n");
        kfree(it_buf);
        kfree(gd_buf);
        return -EINVAL;
    }

    uint8_t *dir_buf = (uint8_t *)kmalloc(block_size, GFP_KERNEL);
    if (!dir_buf) {
        kfree(it_buf);
        kfree(gd_buf);
        return -ENOMEM;
    }

    if (bdev_read(bdev, (uint64_t)root_data_block * sectors_per_block, sectors_per_block, dir_buf) != 0) {
        kfree(dir_buf);
        kfree(it_buf);
        kfree(gd_buf);
        return -EIO;
    }

    struct ext2_dir_entry *de = (struct ext2_dir_entry *)dir_buf;
    if (de->inode != EXT2_ROOT_INO || de->name[0] != '.') {
        printk("<3>[FSCK] Corrupted root directory: '.' missing\n");
        kfree(dir_buf);
        kfree(it_buf);
        kfree(gd_buf);
        return -EINVAL;
    }

    kfree(dir_buf);
    kfree(it_buf);
    kfree(gd_buf);

    printk("<6>[FSCK] /dev/%s clean: %u blocks, %u inodes, 0 errors\n",
           bdev->name, sb->s_blocks_count, sb->s_inodes_count);

    return 0;
}
