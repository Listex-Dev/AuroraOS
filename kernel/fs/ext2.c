#include <aurora/fs/ext2.h>
#include <aurora/drivers/block.h>
#include <aurora/fs/vfs.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

struct ext2_fs_info {
    struct block_device *bdev;
    struct ext2_super_block sb;
    uint32_t block_size;
    uint32_t sectors_per_block;
    uint32_t num_groups;
    uint32_t inode_size;
    struct ext2_group_desc *group_descs;
};

struct ext2_inode_info {
    uint32_t ino;
    struct ext2_inode raw;
};

static int ext2_read_block(struct ext2_fs_info *fs, uint32_t block_num, void *buf)
{
    uint64_t lba = (uint64_t)block_num * fs->sectors_per_block;
    return bdev_read(fs->bdev, lba, fs->sectors_per_block, buf);
}

static int ext2_write_block(struct ext2_fs_info *fs, uint32_t block_num, const void *buf)
{
    uint64_t lba = (uint64_t)block_num * fs->sectors_per_block;
    return bdev_write(fs->bdev, lba, fs->sectors_per_block, buf);
}

static void ext2_sync_metadata(struct ext2_fs_info *fs)
{
    /* Write back superblock at byte 1024 */
    uint8_t sec_buf[1024];
    memset(sec_buf, 0, sizeof(sec_buf));
    memcpy(sec_buf, &fs->sb, sizeof(struct ext2_super_block));
    bdev_write(fs->bdev, 2, 2, sec_buf);

    /* Write back all block group descriptors */
    size_t bg_bytes = fs->num_groups * sizeof(struct ext2_group_desc);
    size_t bg_blocks = (bg_bytes + fs->block_size - 1) / fs->block_size;
    uint32_t bg_first_block = (fs->block_size == 1024) ? 2 : 1;
    for (size_t i = 0; i < bg_blocks; i++) {
        ext2_write_block(fs, bg_first_block + i, ((const uint8_t *)fs->group_descs) + (i * fs->block_size));
    }
    bdev_flush(fs->bdev);
}

static int ext2_read_raw_inode(struct ext2_fs_info *fs, uint32_t ino, struct ext2_inode *out)
{
    if (ino == 0 || ino > fs->sb.s_inodes_count) {
        return -EINVAL;
    }

    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;

    uint32_t inode_table_block = fs->group_descs[group].bg_inode_table;
    uint32_t byte_offset = index * fs->inode_size;
    uint32_t block_offset = byte_offset / fs->block_size;
    uint32_t offset_in_block = byte_offset % fs->block_size;

    uint8_t *block_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!block_buf) return -ENOMEM;

    int err = ext2_read_block(fs, inode_table_block + block_offset, block_buf);
    if (err == 0) {
        memcpy(out, block_buf + offset_in_block, sizeof(struct ext2_inode));
    }
    kfree(block_buf);
    return err;
}

static int ext2_write_raw_inode(struct ext2_fs_info *fs, uint32_t ino, const struct ext2_inode *in)
{
    if (ino == 0 || ino > fs->sb.s_inodes_count) {
        return -EINVAL;
    }

    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;

    uint32_t inode_table_block = fs->group_descs[group].bg_inode_table;
    uint32_t byte_offset = index * fs->inode_size;
    uint32_t block_offset = byte_offset / fs->block_size;
    uint32_t offset_in_block = byte_offset % fs->block_size;

    uint8_t *block_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!block_buf) return -ENOMEM;

    int err = ext2_read_block(fs, inode_table_block + block_offset, block_buf);
    if (err == 0) {
        memcpy(block_buf + offset_in_block, in, sizeof(struct ext2_inode));
        err = ext2_write_block(fs, inode_table_block + block_offset, block_buf);
    }
    kfree(block_buf);
    return err;
}

static uint32_t ext2_alloc_block(struct ext2_fs_info *fs)
{
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!bitmap) return 0;

    for (uint32_t g = 0; g < fs->num_groups; g++) {
        if (fs->group_descs[g].bg_free_blocks_count == 0) continue;

        uint32_t bmap_block = fs->group_descs[g].bg_block_bitmap;
        if (ext2_read_block(fs, bmap_block, bitmap) != 0) continue;

        for (uint32_t i = 0; i < fs->sb.s_blocks_per_group; i++) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx = i % 8;

            if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                bitmap[byte_idx] |= (1 << bit_idx);
                ext2_write_block(fs, bmap_block, bitmap);

                fs->group_descs[g].bg_free_blocks_count--;
                fs->sb.s_free_blocks_count--;
                ext2_sync_metadata(fs);

                kfree(bitmap);
                uint32_t allocated = fs->sb.s_first_data_block + (g * fs->sb.s_blocks_per_group) + i;

                /* Zero out new block */
                uint8_t *zero_buf = (uint8_t *)kzalloc(fs->block_size, GFP_KERNEL);
                if (zero_buf) {
                    ext2_write_block(fs, allocated, zero_buf);
                    kfree(zero_buf);
                }
                return allocated;
            }
        }
    }

    kfree(bitmap);
    return 0;
}

static void ext2_free_block(struct ext2_fs_info *fs, uint32_t block_num)
{
    if (block_num < fs->sb.s_first_data_block) return;

    uint32_t rel = block_num - fs->sb.s_first_data_block;
    uint32_t group = rel / fs->sb.s_blocks_per_group;
    uint32_t index = rel % fs->sb.s_blocks_per_group;

    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!bitmap) return;

    uint32_t bmap_block = fs->group_descs[group].bg_block_bitmap;
    if (ext2_read_block(fs, bmap_block, bitmap) == 0) {
        bitmap[index / 8] &= ~(1 << (index % 8));
        ext2_write_block(fs, bmap_block, bitmap);

        fs->group_descs[group].bg_free_blocks_count++;
        fs->sb.s_free_blocks_count++;
        ext2_sync_metadata(fs);
    }
    kfree(bitmap);
}

static uint32_t ext2_alloc_inode(struct ext2_fs_info *fs, bool is_dir)
{
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!bitmap) return 0;

    for (uint32_t g = 0; g < fs->num_groups; g++) {
        if (fs->group_descs[g].bg_free_inodes_count == 0) continue;

        uint32_t imap_block = fs->group_descs[g].bg_inode_bitmap;
        if (ext2_read_block(fs, imap_block, bitmap) != 0) continue;

        for (uint32_t i = 0; i < fs->sb.s_inodes_per_group; i++) {
            uint32_t ino = (g * fs->sb.s_inodes_per_group) + i + 1;
            if (ino < fs->sb.s_first_ino && ino != EXT2_ROOT_INO) continue;

            uint32_t byte_idx = i / 8;
            uint32_t bit_idx = i % 8;

            if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                bitmap[byte_idx] |= (1 << bit_idx);
                ext2_write_block(fs, imap_block, bitmap);

                fs->group_descs[g].bg_free_inodes_count--;
                if (is_dir) {
                    fs->group_descs[g].bg_used_dirs_count++;
                }
                fs->sb.s_free_inodes_count--;
                ext2_sync_metadata(fs);

                kfree(bitmap);
                return ino;
            }
        }
    }

    kfree(bitmap);
    return 0;
}

static void ext2_free_inode(struct ext2_fs_info *fs, uint32_t ino, bool is_dir)
{
    if (ino == 0 || ino > fs->sb.s_inodes_count) return;

    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;

    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!bitmap) return;

    uint32_t imap_block = fs->group_descs[group].bg_inode_bitmap;
    if (ext2_read_block(fs, imap_block, bitmap) == 0) {
        bitmap[index / 8] &= ~(1 << (index % 8));
        ext2_write_block(fs, imap_block, bitmap);

        fs->group_descs[group].bg_free_inodes_count++;
        if (is_dir && fs->group_descs[group].bg_used_dirs_count > 0) {
            fs->group_descs[group].bg_used_dirs_count--;
        }
        fs->sb.s_free_inodes_count++;
        ext2_sync_metadata(fs);
    }
    kfree(bitmap);
}

static uint32_t ext4_find_extent(struct ext2_fs_info *fs, struct ext4_extent_header *eh, uint32_t lblock)
{
    if (eh->eh_magic != EXT4_EXT_MAGIC) return 0;
    if (eh->eh_depth == 0) {
        struct ext4_extent *ext = (struct ext4_extent *)(eh + 1);
        for (uint16_t i = 0; i < eh->eh_entries; i++) {
            if (lblock >= ext[i].ee_block && lblock < ext[i].ee_block + ext[i].ee_len) {
                return ext[i].ee_start_lo + (lblock - ext[i].ee_block);
            }
        }
        return 0;
    } else {
        /* Index node */
        struct ext4_extent_idx *idx = (struct ext4_extent_idx *)(eh + 1);
        int chosen = -1;
        for (uint16_t i = 0; i < eh->eh_entries; i++) {
            if (lblock >= idx[i].ei_block) {
                chosen = i;
            } else {
                break;
            }
        }
        if (chosen < 0) return 0;
        uint8_t *leaf_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
        if (!leaf_buf) return 0;
        uint32_t res = 0;
        if (ext2_read_block(fs, idx[chosen].ei_leaf_lo, leaf_buf) == 0) {
            struct ext4_extent_header *child_eh = (struct ext4_extent_header *)leaf_buf;
            res = ext4_find_extent(fs, child_eh, lblock);
        }
        kfree(leaf_buf);
        return res;
    }
}

static uint32_t ext2_bmap(struct ext2_fs_info *fs, struct ext2_inode *raw, uint32_t lblock, bool allocate)
{
    /* 1. Check for ext4 extents */
    if (raw->i_flags & EXT4_EXTENTS_FL) {
        struct ext4_extent_header *eh = (struct ext4_extent_header *)raw->i_block;
        if (eh->eh_magic == EXT4_EXT_MAGIC) {
            uint32_t found = ext4_find_extent(fs, eh, lblock);
            if (found != 0) return found;

            if (allocate && eh->eh_depth == 0) {
                struct ext4_extent *ext = (struct ext4_extent *)(eh + 1);
                if (eh->eh_entries > 0) {
                    struct ext4_extent *last_ext = &ext[eh->eh_entries - 1];
                    if (last_ext->ee_block + last_ext->ee_len == lblock) {
                        uint32_t new_blk = ext2_alloc_block(fs);
                        if (!new_blk) return 0;
                        if (last_ext->ee_start_lo + last_ext->ee_len == new_blk && last_ext->ee_len < 32767) {
                            last_ext->ee_len++;
                            raw->i_blocks += (fs->block_size / 512);
                            return new_blk;
                        } else if (eh->eh_entries < eh->eh_max) {
                            uint16_t idx = eh->eh_entries++;
                            ext[idx].ee_block = lblock;
                            ext[idx].ee_len = 1;
                            ext[idx].ee_start_hi = 0;
                            ext[idx].ee_start_lo = new_blk;
                            raw->i_blocks += (fs->block_size / 512);
                            return new_blk;
                        } else {
                            ext2_free_block(fs, new_blk);
                            return 0;
                        }
                    }
                }
                if (eh->eh_entries < eh->eh_max) {
                    uint32_t new_blk = ext2_alloc_block(fs);
                    if (!new_blk) return 0;

                    uint16_t idx = eh->eh_entries++;
                    ext[idx].ee_block = lblock;
                    ext[idx].ee_len = 1;
                    ext[idx].ee_start_hi = 0;
                    ext[idx].ee_start_lo = new_blk;
                    raw->i_blocks += (fs->block_size / 512);
                    return new_blk;
                }
            }
        }
        return 0;
    }

    /* 2. Classic direct blocks (0..11) */
    if (lblock < 12) {
        if (raw->i_block[lblock] == 0 && allocate) {
            uint32_t new_blk = ext2_alloc_block(fs);
            if (!new_blk) return 0;
            raw->i_block[lblock] = new_blk;
            raw->i_blocks += (fs->block_size / 512);
        }
        return raw->i_block[lblock];
    }

    uint32_t ptrs = fs->block_size / sizeof(uint32_t);

    /* 3. Singly indirect block (12) */
    lblock -= 12;
    if (lblock < ptrs) {
        if (raw->i_block[12] == 0) {
            if (!allocate) return 0;
            uint32_t ind_blk = ext2_alloc_block(fs);
            if (!ind_blk) return 0;
            raw->i_block[12] = ind_blk;
            raw->i_blocks += (fs->block_size / 512);
        }

        uint32_t *table = (uint32_t *)kmalloc(fs->block_size, GFP_KERNEL);
        if (!table) return 0;
        ext2_read_block(fs, raw->i_block[12], table);

        if (table[lblock] == 0 && allocate) {
            uint32_t new_blk = ext2_alloc_block(fs);
            if (new_blk) {
                table[lblock] = new_blk;
                ext2_write_block(fs, raw->i_block[12], table);
                raw->i_blocks += (fs->block_size / 512);
            }
        }
        uint32_t phys = table[lblock];
        kfree(table);
        return phys;
    }

    /* 4. Doubly indirect block (13) */
    lblock -= ptrs;
    if (lblock < ptrs * ptrs) {
        uint32_t d1 = lblock / ptrs;
        uint32_t d2 = lblock % ptrs;

        if (raw->i_block[13] == 0) {
            if (!allocate) return 0;
            uint32_t blk = ext2_alloc_block(fs);
            if (!blk) return 0;
            raw->i_block[13] = blk;
            raw->i_blocks += (fs->block_size / 512);
        }

        uint32_t *tbl1 = (uint32_t *)kmalloc(fs->block_size, GFP_KERNEL);
        if (!tbl1) return 0;
        ext2_read_block(fs, raw->i_block[13], tbl1);

        if (tbl1[d1] == 0) {
            if (!allocate) { kfree(tbl1); return 0; }
            uint32_t blk = ext2_alloc_block(fs);
            if (!blk) { kfree(tbl1); return 0; }
            tbl1[d1] = blk;
            ext2_write_block(fs, raw->i_block[13], tbl1);
            raw->i_blocks += (fs->block_size / 512);
        }

        uint32_t ind_blk2 = tbl1[d1];
        kfree(tbl1);

        uint32_t *tbl2 = (uint32_t *)kmalloc(fs->block_size, GFP_KERNEL);
        if (!tbl2) return 0;
        ext2_read_block(fs, ind_blk2, tbl2);

        if (tbl2[d2] == 0 && allocate) {
            uint32_t blk = ext2_alloc_block(fs);
            if (blk) {
                tbl2[d2] = blk;
                ext2_write_block(fs, ind_blk2, tbl2);
                raw->i_blocks += (fs->block_size / 512);
            }
        }
        uint32_t phys = tbl2[d2];
        kfree(tbl2);
        return phys;
    }

    return 0;
}

static ssize_t ext2_file_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    struct inode *inode = file->f_inode;
    if (!inode || !inode->i_private || !inode->i_sb) return -EINVAL;

    struct ext2_fs_info *fs = (struct ext2_fs_info *)inode->i_sb->s_fs_info;
    struct ext2_inode_info *info = (struct ext2_inode_info *)inode->i_private;

    if (*pos >= info->raw.i_size) {
        return 0; /* EOF */
    }

    size_t to_read = count;
    if (*pos + to_read > info->raw.i_size) {
        to_read = info->raw.i_size - *pos;
    }

    uint8_t *blk_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!blk_buf) return -ENOMEM;

    size_t bytes_read = 0;
    while (bytes_read < to_read) {
        uint32_t lblock = (*pos + bytes_read) / fs->block_size;
        uint32_t blk_off = (*pos + bytes_read) % fs->block_size;
        size_t chunk = fs->block_size - blk_off;
        if (chunk > to_read - bytes_read) {
            chunk = to_read - bytes_read;
        }

        uint32_t pblock = ext2_bmap(fs, &info->raw, lblock, false);
        if (pblock == 0) {
            memset((uint8_t *)buf + bytes_read, 0, chunk);
        } else {
            ext2_read_block(fs, pblock, blk_buf);
            memcpy((uint8_t *)buf + bytes_read, blk_buf + blk_off, chunk);
        }

        bytes_read += chunk;
    }

    kfree(blk_buf);
    *pos += bytes_read;
    return (ssize_t)bytes_read;
}

static ssize_t ext2_file_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    struct inode *inode = file->f_inode;
    if (!inode || !inode->i_private || !inode->i_sb) return -EINVAL;

    struct ext2_fs_info *fs = (struct ext2_fs_info *)inode->i_sb->s_fs_info;
    struct ext2_inode_info *info = (struct ext2_inode_info *)inode->i_private;

    uint8_t *blk_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!blk_buf) return -ENOMEM;

    size_t bytes_written = 0;
    while (bytes_written < count) {
        uint32_t lblock = (*pos + bytes_written) / fs->block_size;
        uint32_t blk_off = (*pos + bytes_written) % fs->block_size;
        size_t chunk = fs->block_size - blk_off;
        if (chunk > count - bytes_written) {
            chunk = count - bytes_written;
        }

        uint32_t pblock = ext2_bmap(fs, &info->raw, lblock, true);
        if (pblock == 0) {
            break; /* Out of space */
        }

        if (blk_off != 0 || chunk < fs->block_size) {
            ext2_read_block(fs, pblock, blk_buf);
        }

        memcpy(blk_buf + blk_off, (const uint8_t *)buf + bytes_written, chunk);
        ext2_write_block(fs, pblock, blk_buf);

        bytes_written += chunk;
    }

    kfree(blk_buf);

    *pos += bytes_written;
    if (*pos > inode->i_size) {
        inode->i_size = *pos;
    }
    info->raw.i_size = (uint32_t)inode->i_size;

    ext2_write_raw_inode(fs, info->ino, &info->raw);
    ext2_sync_metadata(fs);

    return (ssize_t)bytes_written;
}

static uint64_t ext2_file_llseek(struct file *file, uint64_t offset, int whence)
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

    if (new_pos < 0) return (uint64_t)-EINVAL;
    file->f_pos = (uint64_t)new_pos;
    return file->f_pos;
}

static void ext2_truncate(struct inode *inode)
{
    if (!inode || !inode->i_sb || !inode->i_private) return;
    struct ext2_fs_info *fs = (struct ext2_fs_info *)inode->i_sb->s_fs_info;
    struct ext2_inode_info *info = (struct ext2_inode_info *)inode->i_private;
    if (!fs || !info) return;

    struct ext2_inode *raw = &info->raw;

    if (raw->i_flags & EXT4_EXTENTS_FL) {
        struct ext4_extent_header *eh = (struct ext4_extent_header *)raw->i_block;
        if (eh->eh_magic == EXT4_EXT_MAGIC && eh->eh_depth == 0) {
            struct ext4_extent *ext = (struct ext4_extent *)(eh + 1);
            for (uint16_t i = 0; i < eh->eh_entries; i++) {
                for (uint32_t b = 0; b < ext[i].ee_len; b++) {
                    ext2_free_block(fs, ext[i].ee_start_lo + b);
                }
            }
            eh->eh_entries = 0;
        }
    } else {
        /* Direct blocks */
        for (int i = 0; i < 12; i++) {
            if (raw->i_block[i]) {
                ext2_free_block(fs, raw->i_block[i]);
                raw->i_block[i] = 0;
            }
        }
        /* Singly indirect */
        if (raw->i_block[12]) {
            uint32_t *tbl = (uint32_t *)kmalloc(fs->block_size, GFP_KERNEL);
            if (tbl && ext2_read_block(fs, raw->i_block[12], tbl) == 0) {
                uint32_t ptrs = fs->block_size / sizeof(uint32_t);
                for (uint32_t i = 0; i < ptrs; i++) {
                    if (tbl[i]) ext2_free_block(fs, tbl[i]);
                }
            }
            if (tbl) kfree(tbl);
            ext2_free_block(fs, raw->i_block[12]);
            raw->i_block[12] = 0;
        }
        /* Doubly indirect */
        if (raw->i_block[13]) {
            uint32_t *tbl1 = (uint32_t *)kmalloc(fs->block_size, GFP_KERNEL);
            if (tbl1 && ext2_read_block(fs, raw->i_block[13], tbl1) == 0) {
                uint32_t ptrs = fs->block_size / sizeof(uint32_t);
                for (uint32_t i = 0; i < ptrs; i++) {
                    if (tbl1[i]) {
                        uint32_t *tbl2 = (uint32_t *)kmalloc(fs->block_size, GFP_KERNEL);
                        if (tbl2 && ext2_read_block(fs, tbl1[i], tbl2) == 0) {
                            for (uint32_t j = 0; j < ptrs; j++) {
                                if (tbl2[j]) ext2_free_block(fs, tbl2[j]);
                            }
                        }
                        if (tbl2) kfree(tbl2);
                        ext2_free_block(fs, tbl1[i]);
                    }
                }
            }
            if (tbl1) kfree(tbl1);
            ext2_free_block(fs, raw->i_block[13]);
            raw->i_block[13] = 0;
        }
    }

    raw->i_size = 0;
    raw->i_blocks = 0;
    inode->i_size = 0;

    ext2_write_raw_inode(fs, info->ino, raw);
    ext2_sync_metadata(fs);
}

static struct inode_operations ext2_file_inode_ops = {
    .truncate = ext2_truncate,
};

static struct file_operations ext2_file_ops = {
    .read = ext2_file_read,
    .write = ext2_file_write,
    .llseek = ext2_file_llseek,
};

static struct inode_operations ext2_dir_inode_ops;

static struct inode *ext2_build_vfs_inode(struct super_block *sb, uint32_t ino, struct ext2_inode *raw)
{
    struct inode *inode = inode_alloc(sb);
    if (!inode) return NULL;

    struct ext2_inode_info *info = (struct ext2_inode_info *)kzalloc(sizeof(struct ext2_inode_info), GFP_KERNEL);
    if (!info) {
        inode_free(inode);
        return NULL;
    }
    info->ino = ino;
    memcpy(&info->raw, raw, sizeof(struct ext2_inode));

    inode->i_ino = ino;
    inode->i_mode = raw->i_mode;
    inode->i_uid = raw->i_uid;
    inode->i_gid = raw->i_gid;
    inode->i_nlink = raw->i_links_count;
    inode->i_size = raw->i_size;
    inode->i_private = info;

    if (S_ISDIR(raw->i_mode)) {
        inode->i_op = &ext2_dir_inode_ops;
        inode->i_fop = NULL;
    } else {
        inode->i_op = &ext2_file_inode_ops;
        inode->i_fop = &ext2_file_ops;
    }

    return inode;
}

static int ext2_add_dir_entry(struct ext2_fs_info *fs, struct ext2_inode *dir_raw, uint32_t dir_ino,
                              uint32_t new_ino, const char *name, uint8_t file_type)
{
    uint8_t name_len = (uint8_t)strlen(name);
    uint16_t req_len = ((8 + name_len + 3) & ~3);

    uint8_t *blk_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!blk_buf) return -ENOMEM;

    /* Scan existing blocks for space */
    uint32_t num_blocks = (dir_raw->i_size + fs->block_size - 1) / fs->block_size;
    if (num_blocks == 0 && dir_raw->i_blocks > 0) {
        num_blocks = dir_raw->i_blocks / (fs->block_size / 512);
    }
    if (num_blocks == 0) num_blocks = 1;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t pblock = ext2_bmap(fs, dir_raw, b, false);
        if (pblock == 0) continue;

        ext2_read_block(fs, pblock, blk_buf);
        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)(blk_buf + offset);
            uint16_t actual_len = ((8 + de->name_len + 3) & ~3);

            if (de->inode == 0 && de->rec_len >= req_len) {
                /* Empty slot */
                de->inode = new_ino;
                de->name_len = name_len;
                de->file_type = file_type;
                memcpy(de->name, name, name_len);
                ext2_write_block(fs, pblock, blk_buf);
                bdev_flush(fs->bdev);
                kfree(blk_buf);
                return 0;
            }

            if (de->rec_len >= actual_len + req_len) {
                /* Split record */
                uint16_t remaining = de->rec_len - actual_len;
                de->rec_len = actual_len;

                struct ext2_dir_entry *new_de = (struct ext2_dir_entry *)(blk_buf + offset + actual_len);
                new_de->inode = new_ino;
                new_de->rec_len = remaining;
                new_de->name_len = name_len;
                new_de->file_type = file_type;
                memcpy(new_de->name, name, name_len);

                ext2_write_block(fs, pblock, blk_buf);
                bdev_flush(fs->bdev);
                kfree(blk_buf);
                return 0;
            }

            if (de->rec_len == 0) break;
            offset += de->rec_len;
        }
    }

    /* Allocate a new directory block */
    uint32_t new_pblock = ext2_bmap(fs, dir_raw, num_blocks, true);
    if (!new_pblock) {
        kfree(blk_buf);
        return -ENOSPC;
    }

    memset(blk_buf, 0, fs->block_size);
    struct ext2_dir_entry *de = (struct ext2_dir_entry *)blk_buf;
    de->inode = new_ino;
    de->rec_len = fs->block_size;
    de->name_len = name_len;
    de->file_type = file_type;
    memcpy(de->name, name, name_len);

    ext2_write_block(fs, new_pblock, blk_buf);
    kfree(blk_buf);

    dir_raw->i_size += fs->block_size;
    ext2_write_raw_inode(fs, dir_ino, dir_raw);
    ext2_sync_metadata(fs);
    return 0;
}

static struct dentry *ext2_lookup(struct inode *dir, struct dentry *target)
{
    if (!dir || !dir->i_sb || !target || !target->d_name[0]) return NULL;

    struct ext2_fs_info *fs = (struct ext2_fs_info *)dir->i_sb->s_fs_info;
    struct ext2_inode_info *dir_info = (struct ext2_inode_info *)dir->i_private;
    const char *target_name = target->d_name;
    size_t target_len = strlen(target_name);

    uint8_t *blk_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!blk_buf) return NULL;

    uint32_t num_blocks = (dir_info->raw.i_size + fs->block_size - 1) / fs->block_size;
    if (num_blocks == 0 && dir_info->raw.i_blocks > 0) {
        num_blocks = dir_info->raw.i_blocks / (fs->block_size / 512);
    }
    if (num_blocks == 0) num_blocks = 1;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t pblock = ext2_bmap(fs, &dir_info->raw, b, false);
        if (pblock == 0) continue;

        ext2_read_block(fs, pblock, blk_buf);
        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)(blk_buf + offset);
            if (de->rec_len == 0) break;

            if (de->inode != 0 && de->name_len == target_len &&
                strncmp(de->name, target_name, target_len) == 0) {
                struct ext2_inode raw;
                if (ext2_read_raw_inode(fs, de->inode, &raw) == 0) {
                    struct inode *child_inode = ext2_build_vfs_inode(dir->i_sb, de->inode, &raw);
                    kfree(blk_buf);
                    if (!child_inode) return NULL;
                    return dentry_create(target_name, target->d_parent, child_inode);
                }
            }

            offset += de->rec_len;
        }
    }

    kfree(blk_buf);
    return NULL;
}

int ext2_populate_dir(struct dentry *dir_dentry)
{
    if (!dir_dentry || !dir_dentry->d_inode || !dir_dentry->d_sb) return -EINVAL;
    if (!S_ISDIR(dir_dentry->d_inode->i_mode)) return -ENOTDIR;

    struct inode *dir = dir_dentry->d_inode;
    struct ext2_fs_info *fs = (struct ext2_fs_info *)dir->i_sb->s_fs_info;
    struct ext2_inode_info *dir_info = (struct ext2_inode_info *)dir->i_private;
    if (!fs || !dir_info) return -EINVAL;

    uint8_t *blk_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!blk_buf) return -ENOMEM;

    uint32_t num_blocks = (dir_info->raw.i_size + fs->block_size - 1) / fs->block_size;
    if (num_blocks == 0 && dir_info->raw.i_blocks > 0) {
        num_blocks = dir_info->raw.i_blocks / (fs->block_size / 512);
    }
    if (num_blocks == 0) num_blocks = 1;

    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t pblock = ext2_bmap(fs, &dir_info->raw, b, false);
        if (pblock == 0) continue;

        if (ext2_read_block(fs, pblock, blk_buf) != 0) continue;

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)(blk_buf + offset);
            if (de->rec_len == 0) break;

            if (de->inode != 0) {
                char name[256];
                size_t nlen = de->name_len;
                if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
                memcpy(name, de->name, nlen);
                name[nlen] = '\0';

                if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                    bool exists = false;
                    for (struct dentry *c = dir_dentry->d_subdirs; c != NULL; c = c->d_next) {
                        if (strcmp(c->d_name, name) == 0) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        struct ext2_inode raw;
                        if (ext2_read_raw_inode(fs, de->inode, &raw) == 0) {
                            struct inode *child_inode = ext2_build_vfs_inode(dir->i_sb, de->inode, &raw);
                            if (child_inode) {
                                dentry_create(name, dir_dentry, child_inode);
                            }
                        }
                    }
                }
            }

            offset += de->rec_len;
        }
    }

    kfree(blk_buf);
    return 0;
}

static int ext2_dir_create(struct inode *dir, struct dentry *dentry, mode_t mode)
{
    if (!dir || !dir->i_sb || !dentry) return -EINVAL;

    struct ext2_fs_info *fs = (struct ext2_fs_info *)dir->i_sb->s_fs_info;
    struct ext2_inode_info *dir_info = (struct ext2_inode_info *)dir->i_private;

    uint32_t ino = ext2_alloc_inode(fs, false);
    if (!ino) return -ENOSPC;

    struct ext2_inode raw;
    memset(&raw, 0, sizeof(raw));
    raw.i_mode = S_IFREG | (mode & 0777);
    raw.i_links_count = 1;
    raw.i_size = 0;

    int err = ext2_write_raw_inode(fs, ino, &raw);
    if (err != 0) {
        ext2_free_inode(fs, ino, false);
        return err;
    }

    err = ext2_add_dir_entry(fs, &dir_info->raw, dir_info->ino, ino, dentry->d_name, EXT2_FT_REG_FILE);
    if (err != 0) {
        ext2_free_inode(fs, ino, false);
        return err;
    }

    struct inode *inode = ext2_build_vfs_inode(dir->i_sb, ino, &raw);
    if (!inode) return -ENOMEM;

    ext2_sync_metadata(fs);

    dentry->d_inode = inode;
    return 0;
}

static int ext2_dir_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode)
{
    if (!dir || !dir->i_sb || !dentry) return -EINVAL;

    struct ext2_fs_info *fs = (struct ext2_fs_info *)dir->i_sb->s_fs_info;
    struct ext2_inode_info *dir_info = (struct ext2_inode_info *)dir->i_private;

    uint32_t ino = ext2_alloc_inode(fs, true);
    if (!ino) return -ENOSPC;

    uint32_t blk = ext2_alloc_block(fs);
    if (!blk) {
        ext2_free_inode(fs, ino, true);
        return -ENOSPC;
    }

    struct ext2_inode raw;
    memset(&raw, 0, sizeof(raw));
    raw.i_mode = S_IFDIR | (mode & 0777);
    raw.i_links_count = 2; /* . and parent link */
    raw.i_size = fs->block_size;
    raw.i_blocks = fs->block_size / 512;
    raw.i_block[0] = blk;

    /* Write . and .. into the new directory block */
    uint8_t *blk_buf = (uint8_t *)kzalloc(fs->block_size, GFP_KERNEL);
    if (!blk_buf) {
        ext2_free_block(fs, blk);
        ext2_free_inode(fs, ino, true);
        return -ENOMEM;
    }

    struct ext2_dir_entry *dot = (struct ext2_dir_entry *)blk_buf;
    dot->inode = ino;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';

    struct ext2_dir_entry *dotdot = (struct ext2_dir_entry *)(blk_buf + 12);
    dotdot->inode = dir_info->ino;
    dotdot->rec_len = fs->block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    ext2_write_block(fs, blk, blk_buf);
    kfree(blk_buf);

    ext2_write_raw_inode(fs, ino, &raw);

    /* Link into parent directory */
    ext2_add_dir_entry(fs, &dir_info->raw, dir_info->ino, ino, dentry->d_name, EXT2_FT_DIR);
    dir_info->raw.i_links_count++;
    ext2_write_raw_inode(fs, dir_info->ino, &dir_info->raw);
    ext2_sync_metadata(fs);

    struct inode *inode = ext2_build_vfs_inode(dir->i_sb, ino, &raw);
    if (!inode) return -ENOMEM;

    dentry->d_inode = inode;
    return 0;
}

static int ext2_dir_unlink(struct inode *dir, struct dentry *dentry)
{
    if (!dir || !dir->i_sb || !dentry || !dentry->d_inode) return -EINVAL;

    struct ext2_fs_info *fs = (struct ext2_fs_info *)dir->i_sb->s_fs_info;
    struct ext2_inode_info *dir_info = (struct ext2_inode_info *)dir->i_private;
    struct ext2_inode_info *file_info = (struct ext2_inode_info *)dentry->d_inode->i_private;

    /* Find entry in parent directory and clear inode */
    uint8_t *blk_buf = (uint8_t *)kmalloc(fs->block_size, GFP_KERNEL);
    if (!blk_buf) return -ENOMEM;

    uint32_t num_blocks = (dir_info->raw.i_size + fs->block_size - 1) / fs->block_size;
    if (num_blocks == 0 && dir_info->raw.i_blocks > 0) {
        num_blocks = dir_info->raw.i_blocks / (fs->block_size / 512);
    }
    if (num_blocks == 0) num_blocks = 1;
    bool found = false;

    for (uint32_t b = 0; b < num_blocks && !found; b++) {
        uint32_t pblock = ext2_bmap(fs, &dir_info->raw, b, false);
        if (pblock == 0) continue;

        ext2_read_block(fs, pblock, blk_buf);
        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)(blk_buf + offset);
            if (de->rec_len == 0) break;

            if (de->inode == file_info->ino) {
                de->inode = 0;
                ext2_write_block(fs, pblock, blk_buf);
                found = true;
                break;
            }

            offset += de->rec_len;
        }
    }
    kfree(blk_buf);

    if (!found) return -ENOENT;

    /* Decrement link count and free if 0 */
    if (file_info->raw.i_links_count > 0) {
        file_info->raw.i_links_count--;
    }

    bool is_dir = S_ISDIR(file_info->raw.i_mode);

    if (file_info->raw.i_links_count == 0) {
        /* Free all allocated data blocks */
        uint32_t blocks = (file_info->raw.i_size + fs->block_size - 1) / fs->block_size;
        for (uint32_t b = 0; b < blocks; b++) {
            uint32_t pblk = ext2_bmap(fs, &file_info->raw, b, false);
            if (pblk != 0) {
                ext2_free_block(fs, pblk);
            }
        }
        ext2_free_inode(fs, file_info->ino, is_dir);
    } else {
        ext2_write_raw_inode(fs, file_info->ino, &file_info->raw);
    }

    if (is_dir && dir_info->raw.i_links_count > 2) {
        dir_info->raw.i_links_count--;
        dir->i_nlink = dir_info->raw.i_links_count;
        ext2_write_raw_inode(fs, dir_info->ino, &dir_info->raw);
    }

    ext2_sync_metadata(fs);
    dentry_free(dentry);
    return 0;
}

static struct inode_operations ext2_dir_inode_ops = {
    .lookup = ext2_lookup,
    .create = ext2_dir_create,
    .mkdir  = ext2_dir_mkdir,
    .unlink = ext2_dir_unlink,
};

static void ext2_vfs_write_inode(struct inode *inode)
{
    if (!inode || !inode->i_sb || !inode->i_private) return;
    struct ext2_fs_info *fs = (struct ext2_fs_info *)inode->i_sb->s_fs_info;
    struct ext2_inode_info *info = (struct ext2_inode_info *)inode->i_private;
    if (!fs || !info) return;

    info->raw.i_mode = inode->i_mode;
    info->raw.i_uid = (uint16_t)inode->i_uid;
    info->raw.i_gid = (uint16_t)inode->i_gid;
    info->raw.i_size = (uint32_t)inode->i_size;
    info->raw.i_links_count = (uint16_t)inode->i_nlink;
    info->raw.i_mtime = (uint32_t)inode->i_mtime;
    info->raw.i_atime = (uint32_t)inode->i_atime;
    info->raw.i_ctime = (uint32_t)inode->i_ctime;

    ext2_write_raw_inode(fs, info->ino, &info->raw);
    ext2_sync_metadata(fs);
}

static struct super_operations ext2_super_ops = {
    .write_inode = ext2_vfs_write_inode,
};

struct super_block *ext2_mount(struct block_device *bdev)
{
    if (!bdev) return NULL;

    /* Read superblock at byte 1024 (LBA 2 for 512b sectors) */
    uint8_t sb_buf[1024];
    if (bdev_read(bdev, 2, 2, sb_buf) != 0) {
        printk("<3>[EXT2] Failed to read superblock from /dev/%s\n", bdev->name);
        return NULL;
    }

    struct ext2_super_block *raw_sb = (struct ext2_super_block *)sb_buf;
    if (raw_sb->s_magic != EXT2_SUPER_MAGIC) {
        printk("<3>[EXT2] Invalid magic 0x%04x on /dev/%s (expected 0xEF53)\n",
               raw_sb->s_magic, bdev->name);
        return NULL;
    }

    struct ext2_fs_info *fs = (struct ext2_fs_info *)kzalloc(sizeof(struct ext2_fs_info), GFP_KERNEL);
    if (!fs) return NULL;

    memcpy(&fs->sb, raw_sb, sizeof(struct ext2_super_block));
    fs->bdev = bdev;
    fs->block_size = 1024 << raw_sb->s_log_block_size;
    fs->sectors_per_block = fs->block_size / bdev->block_size;
    fs->inode_size = (raw_sb->s_rev_level >= EXT2_DYNAMIC_REV) ? raw_sb->s_inode_size : 128;
    if (fs->inode_size == 0) fs->inode_size = 128;

    fs->num_groups = (raw_sb->s_blocks_count + raw_sb->s_blocks_per_group - 1) / raw_sb->s_blocks_per_group;

    /* Read block group descriptors */
    size_t bg_bytes = fs->num_groups * sizeof(struct ext2_group_desc);
    size_t bg_blocks = (bg_bytes + fs->block_size - 1) / fs->block_size;
    fs->group_descs = (struct ext2_group_desc *)kmalloc(bg_blocks * fs->block_size, GFP_KERNEL);
    if (!fs->group_descs) {
        kfree(fs);
        return NULL;
    }

    uint32_t bg_first_block = (fs->block_size == 1024) ? 2 : 1;
    for (size_t i = 0; i < bg_blocks; i++) {
        ext2_read_block(fs, bg_first_block + i, ((uint8_t *)fs->group_descs) + (i * fs->block_size));
    }

    struct super_block *sb = (struct super_block *)kzalloc(sizeof(struct super_block), GFP_KERNEL);
    if (!sb) {
        kfree(fs->group_descs);
        kfree(fs);
        return NULL;
    }

    sb->s_op = &ext2_super_ops;
    sb->s_blocksize = fs->block_size;
    sb->s_type = (raw_sb->s_feature_incompat & 0x40) ? "ext4" : "ext2";
    sb->s_fs_info = fs;

    /* Load root inode 2 */
    struct ext2_inode root_raw;
    if (ext2_read_raw_inode(fs, EXT2_ROOT_INO, &root_raw) != 0) {
        kfree(sb);
        kfree(fs->group_descs);
        kfree(fs);
        return NULL;
    }

    struct inode *root_inode = ext2_build_vfs_inode(sb, EXT2_ROOT_INO, &root_raw);
    if (!root_inode) {
        kfree(sb);
        kfree(fs->group_descs);
        kfree(fs);
        return NULL;
    }

    struct dentry *root_dentry = dentry_create("/", NULL, root_inode);
    if (!root_dentry) {
        inode_free(root_inode);
        kfree(sb);
        kfree(fs->group_descs);
        kfree(fs);
        return NULL;
    }
    sb->s_root = root_dentry;

    printk("<6>[EXT2] Mounted %s (%s) on /dev/%s (block_size: %u, inodes: %u, blocks: %u, free: %u)\n",
           sb->s_type, (raw_sb->s_feature_incompat & 0x40) ? "extents" : "indirect",
           bdev->name, fs->block_size, raw_sb->s_inodes_count, raw_sb->s_blocks_count, raw_sb->s_free_blocks_count);

    return sb;
}
