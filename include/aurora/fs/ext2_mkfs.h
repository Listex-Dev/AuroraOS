#ifndef _AURORA_FS_EXT2_MKFS_H
#define _AURORA_FS_EXT2_MKFS_H

#include <aurora/types.h>
#include <aurora/drivers/block.h>

int ext2_mkfs(struct block_device *bdev, uint32_t block_size, bool use_extents);

#endif /* _AURORA_FS_EXT2_MKFS_H */
