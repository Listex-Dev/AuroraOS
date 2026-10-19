#include <pty.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int grantpt(int fd)
{
    (void)fd;
    return 0;
}

int unlockpt(int fd)
{
    int lock = 0;
    return ioctl(fd, TIOCSPTLCK, &lock);
}

static char s_ptsname_buf[32];

char *ptsname(int fd)
{
    if (ptsname_r(fd, s_ptsname_buf, sizeof(s_ptsname_buf)) < 0) {
        return NULL;
    }
    return s_ptsname_buf;
}

int ptsname_r(int fd, char *buf, size_t buflen)
{
    if (!buf || buflen < 16) {
        errno = EINVAL;
        return -1;
    }

    unsigned int ptn = 0;
    if (ioctl(fd, TIOCGPTN, &ptn) < 0) {
        return -1;
    }

    snprintf(buf, buflen, "/dev/pts/%u", ptn);
    return 0;
}

int openpty(int *amaster, int *aslave, char *name,
            const struct termios *termp, const struct winsize *winp)
{
    if (!amaster || !aslave) {
        errno = EINVAL;
        return -1;
    }

    int master = open("/dev/ptmx", O_RDWR, 0);
    if (master < 0) {
        return -1;
    }

    if (grantpt(master) < 0 || unlockpt(master) < 0) {
        close(master);
        return -1;
    }

    char sname[32];
    if (ptsname_r(master, sname, sizeof(sname)) < 0) {
        close(master);
        return -1;
    }

    int slave = open(sname, O_RDWR, 0);
    if (slave < 0) {
        close(master);
        return -1;
    }

    if (termp) {
        tcsetattr(slave, TCSANOW, termp);
    }
    if (winp) {
        ioctl(slave, TIOCSWINSZ, winp);
    }

    if (name) {
        strcpy(name, sname);
    }

    *amaster = master;
    *aslave = slave;
    return 0;
}
