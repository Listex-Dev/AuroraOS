/*
 * AuroraOS /sbin/init - Master PID 1 process
 *
 * Responsibilities:
 * - Mount /proc, /dev, /sys, /tmp
 * - Spawn and supervise /bin/getty on /dev/console
 * - Reap orphaned zombie processes
 * - Supervise runlevels and system shutdown/reboot
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

static pid_t spawn_getty(const char *tty_dev)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("init: fork getty");
        return -1;
    }

    if (pid == 0) {
        /* Child */
        char *argv[] = { "/bin/getty", (char *)tty_dev, NULL };
        execve("/bin/getty", argv, environ);
        fprintf(stderr, "init: execve /bin/getty failed: %s\n", strerror(errno));
        _exit(1);
    }

    return pid;
}

int main(int argc, char **argv)
{
    /* If run with arguments as telinit/shutdown */
    if (argc > 1) {
        int runlevel = atoi(argv[1]);
        if (runlevel == 0 || runlevel == 6) {
            printf("init: System shutting down (runlevel %d)...\n", runlevel);
            kill(-1, SIGTERM);
            sleep(1);
            kill(-1, SIGKILL);
            _exit(0);
        }
    }

    /* PID 1 initialization */
    if (getpid() == 1) {
        /* Ignore terminal interrupt signals in init */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);

        /* Ensure essential directories exist */
        mkdir("/proc", 0755);
        mkdir("/dev", 0755);
        mkdir("/sys", 0755);
        mkdir("/tmp", 0777);
        mkdir("/run", 0755);
        mkdir("/root", 0700);

        umask(022);

        setenv("PATH", "/bin:/usr/bin", 1);
        setenv("TERM", "linux", 1);
        setenv("HOME", "/root", 1);

        printf("AuroraOS init (PID 1) started. Spawning console getty...\n");

        pid_t getty_pid = spawn_getty("/dev/console");

        /* Supervisor and zombie reaper loop */
        while (1) {
            int status = 0;
            pid_t reaped = waitpid(-1, &status, WNOHANG);

            if (reaped > 0) {
                if (reaped == getty_pid) {
                    printf("init: getty (PID %d) exited, respawning...\n", reaped);
                    sleep(1);
                    getty_pid = spawn_getty("/dev/console");
                }
            } else {
                sleep(1);
            }
        }
    }

    /* If not running as PID 1, act as helper */
    printf("init: Usage: init [0|1|3|6]\n");
    return 0;
}
