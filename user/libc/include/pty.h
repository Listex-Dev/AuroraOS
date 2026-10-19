#ifndef _PTY_H
#define _PTY_H

#include <termios.h>

int grantpt(int fd);
int unlockpt(int fd);
char *ptsname(int fd);
int ptsname_r(int fd, char *buf, size_t buflen);
int openpty(int *amaster, int *aslave, char *name,
            const struct termios *termp, const struct winsize *winp);

#endif /* _PTY_H */
