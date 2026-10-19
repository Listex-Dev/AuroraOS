/*
 * AuroraOS /sbin/telinit - Change system runlevel
 *
 * Usage: telinit [0|1|3|6]
 * Runlevels:
 *   0: Halt / Power off
 *   1: Single-user maintenance mode
 *   3: Multi-user console mode (default)
 *   6: Reboot
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#define INITCTL_FILE "/run/initctl"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: telinit [0|1|3|6]\n");
        return 1;
    }

    int runlevel = atoi(argv[1]);
    if (runlevel != 0 && runlevel != 1 && runlevel != 3 && runlevel != 6) {
        fprintf(stderr, "telinit: invalid runlevel '%s' (valid: 0, 1, 3, 6)\n", argv[1]);
        return 1;
    }

    mkdir("/run", 0755);

    int fd = open(INITCTL_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("telinit: open initctl");
        return 1;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%d\n", runlevel);
    write(fd, buf, strlen(buf));
    close(fd);

    /* Signal PID 1 if present */
    if (kill(1, SIGUSR1) < 0 && errno != ESRCH) {
        perror("telinit: signal init");
        return 1;
    }

    printf("telinit: Requested transition to runlevel %d\n", runlevel);
    return 0;
}
