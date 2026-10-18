#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "syscall.h"

struct linux_dirent64 {
    uint64_t        d_ino;
    int64_t         d_off;
    unsigned short  d_reclen;
    unsigned char   d_type;
    char            d_name[];
};

DIR *opendir(const char *name)
{
    int fd = open(name, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return NULL;
    }
    DIR *dirp = (DIR *)malloc(sizeof(DIR));
    if (!dirp) {
        close(fd);
        return NULL;
    }
    memset(dirp, 0, sizeof(DIR));
    dirp->fd = fd;
    return dirp;
}

struct dirent *readdir(DIR *dirp)
{
    if (!dirp) return NULL;

    if (dirp->buf_pos >= dirp->buf_end) {
        long nread = __syscall3(SYS_getdents64, dirp->fd, (long)dirp->buf, sizeof(dirp->buf));
        if (nread <= 0) {
            return NULL;
        }
        dirp->buf_pos = 0;
        dirp->buf_end = (int)nread;
    }

    struct linux_dirent64 *ld = (struct linux_dirent64 *)(dirp->buf + dirp->buf_pos);
    dirp->current.d_ino = ld->d_ino;
    dirp->current.d_off = ld->d_off;
    dirp->current.d_reclen = ld->d_reclen;
    dirp->current.d_type = ld->d_type;
    strncpy(dirp->current.d_name, ld->d_name, sizeof(dirp->current.d_name) - 1);
    dirp->current.d_name[sizeof(dirp->current.d_name) - 1] = '\0';

    dirp->buf_pos += ld->d_reclen;
    return &dirp->current;
}

int closedir(DIR *dirp)
{
    if (!dirp) return -1;
    int ret = close(dirp->fd);
    free(dirp);
    return ret;
}

void rewinddir(DIR *dirp)
{
    if (!dirp) return;
    lseek(dirp->fd, 0, SEEK_SET);
    dirp->buf_pos = 0;
    dirp->buf_end = 0;
}
