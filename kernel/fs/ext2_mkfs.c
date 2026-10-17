#include <aurora/fs/ext2_mkfs.h>
#include <aurora/fs/ext2.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

int ext2_mkfs(struct block_device *bdev, uint32_t block_size, bool use_extents)
{
    if (!bdev) return -EINVAL;
    if (block_size != 1024 && block_size != 2048 && block_size != 4096) {
        block_size = 1024;
    }

    uint32_t sectors_per_block = block_size / bdev->block_size;
    uint32_t total_blocks = (uint32_t)(bdev->total_blocks / sectors_per_block);
    uint32_t first_data_block = (block_size == 1024) ? 1 : 0;

    uint32_t blocks_per_group = block_size * 8;
    if (blocks_per_group > total_blocks - first_data_block) {
        blocks_per_group = total_blocks - first_data_block;
    }

    uint32_t num_groups = (total_blocks - first_data_block + blocks_per_group - 1) / blocks_per_group;
    if (num_groups == 0) num_groups = 1;

    uint32_t inodes_per_group = 128;
    uint32_t inode_size = use_extents ? 256 : 128;
    uint32_t total_inodes = num_groups * inodes_per_group;

    uint32_t itable_blocks_per_group = (inodes_per_group * inode_size + block_size - 1) / block_size;

    /* Build superblock */
    struct ext2_super_block sb;
    memset(&sb, 0, sizeof(sb));

    sb.s_inodes_count = total_inodes;
    sb.s_blocks_count = total_blocks;
    sb.s_r_blocks_count = total_blocks / 20; /* 5% reserved */
    sb.s_first_data_block = first_data_block;
    sb.s_log_block_size = (block_size == 1024) ? 0 : ((block_size == 2048) ? 1 : 2);
    sb.s_blocks_per_group = blocks_per_group;
    sb.s_frags_per_group = blocks_per_group;
    sb.s_inodes_per_group = inodes_per_group;
    sb.s_magic = EXT2_SUPER_MAGIC;
    sb.s_state = 1; /* Clean */
    sb.s_errors = 1;
    sb.s_rev_level = EXT2_DYNAMIC_REV;
    sb.s_first_ino = 11;
    sb.s_inode_size = inode_size;
    sb.s_feature_compat = 0;
    sb.s_feature_incompat = use_extents ? 0x40 : 0x02; /* 0x40 = extents */
    sb.s_feature_ro_compat = 0;
    strncpy(sb.s_volume_name, "aurora_root", 15);

    /* Allocate descriptors */
    struct ext2_group_desc *gdescs = (struct ext2_group_desc *)kzalloc(num_groups * sizeof(struct ext2_group_desc), GFP_KERNEL);
    if (!gdescs) return -ENOMEM;

    uint32_t cur_block = first_data_block;
    if (block_size == 1024) {
        cur_block++; /* Skip superblock block 1 */
    }

    uint32_t gdesc_blocks = (num_groups * sizeof(struct ext2_group_desc) + block_size - 1) / block_size;
    cur_block += gdesc_blocks;

    /* Setup group 0 */
    gdescs[0].bg_block_bitmap = cur_block++;
    gdescs[0].bg_inode_bitmap = cur_block++;
    gdescs[0].bg_inode_table = cur_block;
    cur_block += itable_blocks_per_group;

    uint32_t root_dir_block = cur_block++;
    uint32_t lost_found_block = cur_block++;

    uint32_t used_blocks_grp0 = cur_block - first_data_block;
    gdescs[0].bg_free_blocks_count = (uint16_t)(blocks_per_group - used_blocks_grp0);
    gdescs[0].bg_free_inodes_count = (uint16_t)(inodes_per_group - 11);
    gdescs[0].bg_used_dirs_count = 2; /* root and lost+found */

    sb.s_free_blocks_count = total_blocks - used_blocks_grp0;
    sb.s_free_inodes_count = total_inodes - 11;

    /* Setup remaining groups if any */
    for (uint32_t g = 1; g < num_groups; g++) {
        uint32_t g_start = first_data_block + (g * blocks_per_group);
        gdescs[g].bg_block_bitmap = g_start++;
        gdescs[g].bg_inode_bitmap = g_start++;
        gdescs[g].bg_inode_table = g_start;
        g_start += itable_blocks_per_group;
        gdescs[g].bg_free_blocks_count = (uint16_t)(blocks_per_group - 2 - itable_blocks_per_group);
        gdescs[g].bg_free_inodes_count = (uint16_t)inodes_per_group;
        gdescs[g].bg_used_dirs_count = 0;
        sb.s_free_blocks_count -= (2 + itable_blocks_per_group);
    }

    /* 1. Write Superblock at byte offset 1024 */
    uint8_t *sb_buf = (uint8_t *)kzalloc(1024, GFP_KERNEL);
    if (!sb_buf) {
        kfree(gdescs);
        return -ENOMEM;
    }
    memcpy(sb_buf, &sb, sizeof(struct ext2_super_block));
    bdev_write(bdev, 2, 2, sb_buf);
    kfree(sb_buf);

    /* 2. Write Group Descriptors */
    uint8_t *gd_buf = (uint8_t *)kzalloc(gdesc_blocks * block_size, GFP_KERNEL);
    if (!gd_buf) {
        kfree(gdescs);
        return -ENOMEM;
    }
    memcpy(gd_buf, gdescs, num_groups * sizeof(struct ext2_group_desc));
    uint32_t gd_start = (block_size == 1024) ? 2 : 1;
    for (uint32_t i = 0; i < gdesc_blocks; i++) {
        bdev_write(bdev, (uint64_t)(gd_start + i) * sectors_per_block, sectors_per_block, gd_buf + (i * block_size));
    }
    kfree(gd_buf);

    /* 3. Write Block Bitmap for Group 0 */
    uint8_t *bmap_buf = (uint8_t *)kzalloc(block_size, GFP_KERNEL);
    if (!bmap_buf) {
        kfree(gdescs);
        return -ENOMEM;
    }
    for (uint32_t i = 0; i < used_blocks_grp0; i++) {
        bmap_buf[i / 8] |= (1 << (i % 8));
    }
    bdev_write(bdev, (uint64_t)gdescs[0].bg_block_bitmap * sectors_per_block, sectors_per_block, bmap_buf);
    kfree(bmap_buf);

    /* 4. Write Inode Bitmap for Group 0 (mark 1..11 used) */
    uint8_t *imap_buf = (uint8_t *)kzalloc(block_size, GFP_KERNEL);
    if (!imap_buf) {
        kfree(gdescs);
        return -ENOMEM;
    }
    for (uint32_t i = 0; i < 11; i++) {
        imap_buf[i / 8] |= (1 << (i % 8));
    }
    bdev_write(bdev, (uint64_t)gdescs[0].bg_inode_bitmap * sectors_per_block, sectors_per_block, imap_buf);
    kfree(imap_buf);

    /* 5. Write Inode Table for Group 0 */
    uint8_t *itable = (uint8_t *)kzalloc(itable_blocks_per_group * block_size, GFP_KERNEL);
    if (!itable) {
        kfree(gdescs);
        return -ENOMEM;
    }

    /* Inode 2: Root directory '/' */
    struct ext2_inode *root_ino = (struct ext2_inode *)(itable + (1 * inode_size));
    root_ino->i_mode = EXT2_S_IFDIR | 0755;
    root_ino->i_links_count = 3; /* ., .., lost+found */
    root_ino->i_size = block_size;
    root_ino->i_blocks = block_size / 512;
    if (use_extents) {
        root_ino->i_flags = EXT4_EXTENTS_FL;
        struct ext4_extent_header *eh = (struct ext4_extent_header *)root_ino->i_block;
        eh->eh_magic = EXT4_EXT_MAGIC;
        eh->eh_entries = 1;
        eh->eh_max = 4;
        eh->eh_depth = 0;
        struct ext4_extent *ext = (struct ext4_extent *)(eh + 1);
        ext[0].ee_block = 0;
        ext[0].ee_len = 1;
        ext[0].ee_start_hi = 0;
        ext[0].ee_start_lo = root_dir_block;
    } else {
        root_ino->i_block[0] = root_dir_block;
    }

    /* Inode 11: 'lost+found' */
    struct ext2_inode *lf_ino = (struct ext2_inode *)(itable + (10 * inode_size));
    lf_ino->i_mode = EXT2_S_IFDIR | 0700;
    lf_ino->i_links_count = 2; /* ., .. */
    lf_ino->i_size = block_size;
    lf_ino->i_blocks = block_size / 512;
    if (use_extents) {
        lf_ino->i_flags = EXT4_EXTENTS_FL;
        struct ext4_extent_header *eh = (struct ext4_extent_header *)lf_ino->i_block;
        eh->eh_magic = EXT4_EXT_MAGIC;
        eh->eh_entries = 1;
        eh->eh_max = 4;
        eh->eh_depth = 0;
        struct ext4_extent *ext = (struct ext4_extent *)(eh + 1);
        ext[0].ee_block = 0;
        ext[0].ee_len = 1;
        ext[0].ee_start_hi = 0;
        ext[0].ee_start_lo = lost_found_block;
    } else {
        lf_ino->i_block[0] = lost_found_block;
    }

    for (uint32_t i = 0; i < itable_blocks_per_group; i++) {
        bdev_write(bdev, (uint64_t)(gdescs[0].bg_inode_table + i) * sectors_per_block, sectors_per_block, itable + (i * block_size));
    }
    kfree(itable);

    /* 6. Write Root Directory block with '.', '..', and 'lost+found' */
    uint8_t *dir_block = (uint8_t *)kzalloc(block_size, GFP_KERNEL);
    if (!dir_block) {
        kfree(gdescs);
        return -ENOMEM;
    }

    /* '.' */
    struct ext2_dir_entry *de_dot = (struct ext2_dir_entry *)dir_block;
    de_dot->inode = EXT2_ROOT_INO;
    de_dot->rec_len = 12;
    de_dot->name_len = 1;
    de_dot->file_type = EXT2_FT_DIR;
    de_dot->name[0] = '.';

    /* '..' */
    struct ext2_dir_entry *de_dotdot = (struct ext2_dir_entry *)(dir_block + 12);
    de_dotdot->inode = EXT2_ROOT_INO;
    de_dotdot->rec_len = 12;
    de_dotdot->name_len = 2;
    de_dotdot->file_type = EXT2_FT_DIR;
    de_dotdot->name[0] = '.';
    de_dotdot->name[1] = '.';

    /* 'lost+found' */
    struct ext2_dir_entry *de_lf = (struct ext2_dir_entry *)(dir_block + 24);
    de_lf->inode = 11;
    de_lf->rec_len = block_size - 24;
    de_lf->name_len = 10;
    de_lf->file_type = EXT2_FT_DIR;
    memcpy(de_lf->name, "lost+found", 10);

    bdev_write(bdev, (uint64_t)root_dir_block * sectors_per_block, sectors_per_block, dir_block);

    /* 7. Write lost+found directory block with '.' and '..' */
    memset(dir_block, 0, block_size);
    de_dot = (struct ext2_dir_entry *)dir_block;
    de_dot->inode = 11;
    de_dot->rec_len = 12;
    de_dot->name_len = 1;
    de_dot->file_type = EXT2_FT_DIR;
    de_dot->name[0] = '.';

    de_dotdot = (struct ext2_dir_entry *)(dir_block + 12);
    de_dotdot->inode = EXT2_ROOT_INO;
    de_dotdot->rec_len = block_size - 12;
    de_dotdot->name_len = 2;
    de_dotdot->file_type = EXT2_FT_DIR;
    de_dotdot->name[0] = '.';
    de_dotdot->name[1] = '.';

    bdev_write(bdev, (uint64_t)lost_found_block * sectors_per_block, sectors_per_block, dir_block);
    kfree(dir_block);

    bdev_flush(bdev);
    kfree(gdescs);

    printk("<6>[MKFS] Formatted /dev/%s with %s (blocks: %u, inodes: %u, block_size: %u)\n",
           bdev->name, use_extents ? "ext4" : "ext2", total_blocks, total_inodes, block_size);

    return 0;
}
