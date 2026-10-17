#ifndef _AURORA_FS_EXT2_FSCK_H
#define _AURORA_FS_EXT2_FSCK_H

#include <aurora/types.h>
#include <aurora/drivers/block.h>

int ext2_fsck(struct block_device *bdev);

#endif /* _AURORA_FS_EXT2_FSCK_H */
