/*
 * AuroraOS Binary Loader from VFS
 *
 * Implements dynamic binary loading from the root filesystem (ext4/ramfs).
 * All embedded binary tables and .incbin assemblies are removed from the kernel.
 */

#include <aurora/proc/binaries.h>
#include <aurora/fs/vfs.h>
#include <aurora/syscall.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>

const void *find_builtin_binary_meta(const char *path, size_t *size_out, mode_t *mode_out, uid_t *uid_out, gid_t *gid_out)
{
    if (!path) return NULL;

    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) return NULL;

    struct stat st;
    if (sys_fstat(fd, &st) < 0 || st.st_size == 0) {
        sys_close(fd);
        return NULL;
    }

    void *buf = kmalloc(st.st_size, GFP_KERNEL);
    if (!buf) {
        sys_close(fd);
        return NULL;
    }

    ssize_t rd = sys_read(fd, buf, st.st_size);
    sys_close(fd);

    if (rd != (ssize_t)st.st_size) {
        kfree(buf);
        return NULL;
    }

    if (size_out) *size_out = st.st_size;
    if (mode_out) *mode_out = st.st_mode ? st.st_mode : 0755;
    if (uid_out)  *uid_out  = st.st_uid;
    if (gid_out)  *gid_out  = st.st_gid;

    return buf;
}

const void *find_builtin_binary(const char *path, size_t *size_out)
{
    return find_builtin_binary_meta(path, size_out, NULL, NULL, NULL);
}
