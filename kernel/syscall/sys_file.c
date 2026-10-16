#include <aurora/syscall.h>
#include <aurora/errno.h>
#include <aurora/serial.h>

int64_t sys_write(int fd, const void *buf, size_t count)
{
    if (fd == 1 || fd == 2) {
        /* Standard output and standard error go to serial terminal console */
        if (!buf) {
            return -EFAULT;
        }
        serial_write((const char *)buf, count);
        return (int64_t)count;
    }

    return -EBADF;
}

int64_t sys_read(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -EBADF;
}

int64_t sys_close(int fd)
{
    (void)fd;
    return 0;
}
