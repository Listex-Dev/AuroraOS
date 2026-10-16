#ifndef _AURORA_FS_CPIO_H
#define _AURORA_FS_CPIO_H

#include <aurora/types.h>

struct cpio_newc_header {
    char c_magic[6];     /* "070701" or "070702" */
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
} __attribute__((packed));

int cpio_extract(const void *archive, size_t size);
int cpio_extract_initrd(void);

#endif /* _AURORA_FS_CPIO_H */
