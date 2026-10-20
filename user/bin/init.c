/*
 * AuroraOS /sbin/init - Master PID 1 Process & Service Supervisor
 *
 * Responsibilities:
 * - Mount essential directories: /proc, /dev, /sys, /tmp, /run
 * - Initialize environment: PATH, TERM, HOME, umask
 * - Start system services according to /etc/rc.conf
 * - Supervise and respawn /bin/getty
 * - Manage runlevels:
 *     0: Halt / Power off
 *     1: Single-user maintenance mode
 *     3: Multi-user console mode (default)
 *     6: Reboot
 * - Reap orphaned zombie processes
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
#include <stdbool.h>

#define INITCTL_FILE "/run/initctl"
#define RC_CONF      "/etc/rc.conf"

static volatile int g_got_sigusr1 = 0;
static int g_current_runlevel = 3;
static pid_t g_getty_pid = -1;

static void sigusr1_handler(int sig)
{
    (void)sig;
    g_got_sigusr1 = 1;
}

static pid_t spawn_getty(const char *tty_dev)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("init: fork getty");
        return -1;
    }

    if (pid == 0) {
        char *argv[] = { "/bin/getty", (char *)tty_dev, NULL };
        execve("/bin/getty", argv, environ);
        fprintf(stderr, "init: execve /bin/getty failed: %s\n", strerror(errno));
        _exit(1);
    }

    return pid;
}

static void run_service_action(const char *service, const char *action)
{
    char script_path[128];
    snprintf(script_path, sizeof(script_path), "/etc/init.d/%s", service);

    if (access(script_path, X_OK) != 0 && access(script_path, R_OK) != 0) {
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { "/bin/sh", script_path, (char *)action, NULL };
        execve("/bin/sh", argv, environ);
        _exit(1);
    }

    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

static void start_services(void)
{
    /* Default services order */
    const char *services[] = { "networking", "syslogd", "crond", "sshd", NULL };

    /* Parse /etc/rc.conf if present */
    FILE *f = fopen(RC_CONF, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "services=", 9) == 0) {
                p += 9;
                if (*p == '"' || *p == '\'') p++;
                char *end = strchr(p, '"');
                if (!end) end = strchr(p, '\'');
                if (!end) end = strchr(p, '\n');
                if (end) *end = '\0';

                char *token = strtok(p, " ");
                while (token) {
                    run_service_action(token, "start");
                    token = strtok(NULL, " ");
                }
                fclose(f);
                return;
            }
        }
        fclose(f);
    }

    for (int i = 0; services[i]; i++) {
        run_service_action(services[i], "start");
    }
}

static void stop_services(void)
{
    const char *services_rev[] = { "sshd", "crond", "syslogd", "networking", NULL };
    for (int i = 0; services_rev[i]; i++) {
        run_service_action(services_rev[i], "stop");
    }
}

static void switch_runlevel(int new_level)
{
    printf("\ninit: Switching to runlevel %d...\n", new_level);

    if (new_level == 0) {
        /* Runlevel 0: Halt */
        stop_services();
        if (g_getty_pid > 0) {
            kill(g_getty_pid, SIGTERM);
            g_getty_pid = -1;
        }
        kill(-1, SIGTERM);
        sleep(1);
        kill(-1, SIGKILL);
        printf("init: System halted.\n");
        _exit(0);
    } else if (new_level == 1) {
        /* Runlevel 1: Single-user maintenance mode */
        run_service_action("sshd", "stop");
        run_service_action("crond", "stop");
        if (g_getty_pid > 0) {
            kill(g_getty_pid, SIGTERM);
            g_getty_pid = -1;
        }
        g_current_runlevel = 1;
        printf("init: Switched to runlevel 1 (single-user maintenance mode).\n");
    } else if (new_level == 3) {
        /* Runlevel 3: Multi-user mode */
        start_services();
        if (g_getty_pid <= 0) {
            g_getty_pid = spawn_getty("/dev/console");
        }
        g_current_runlevel = 3;
    } else if (new_level == 6) {
        /* Runlevel 6: Reboot */
        stop_services();
        if (g_getty_pid > 0) {
            kill(g_getty_pid, SIGTERM);
            g_getty_pid = -1;
        }
        kill(-1, SIGTERM);
        sleep(1);
        kill(-1, SIGKILL);
        printf("init: Restarting system.\n");
        _exit(0);
    }
}

int main(int argc, char **argv)
{
    /* If run with arguments as telinit/shutdown helper */
    if (argc > 1) {
        int runlevel = atoi(argv[1]);
        if (runlevel == 0 || runlevel == 1 || runlevel == 3 || runlevel == 6) {
            char *targv[] = { "/sbin/telinit", argv[1], NULL };
            execve("/sbin/telinit", targv, environ);
            _exit(0);
        }
    }

    /* PID 1 initialization */
    if (getpid() == 1) {
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGUSR1, sigusr1_handler);

        /* Ensure essential directories exist */
        mkdir("/proc", 0755);
        mkdir("/dev", 0755);
        mkdir("/dev/pts", 0755);
        mkdir("/sys", 0755);
        mkdir("/tmp", 0777);
        mkdir("/run", 0755);
        mkdir("/root", 0700);
        mkdir("/var", 0755);
        mkdir("/var/log", 0755);
        mkdir("/var/spool", 0755);
        mkdir("/var/spool/cron", 0755);
        mkdir("/var/spool/cron/crontabs", 0755);
        mkdir("/etc", 0755);
        mkdir("/etc/init.d", 0755);

        umask(022);

        setenv("PATH", "/bin:/usr/bin:/sbin", 1);
        setenv("TERM", "linux", 1);
        setenv("HOME", "/root", 1);
        setenv("HOSTNAME", "aurora", 1);

        printf("AuroraOS init (PID 1) started. Booting runlevel 3 (multi-user)...\n");

        /* Start services and spawn getty */
        start_services();
        g_getty_pid = spawn_getty("/dev/console");

        /* Supervisor and zombie reaper loop */
        while (1) {
            if (g_got_sigusr1) {
                g_got_sigusr1 = 0;
                int req_level = -1;
                int fd = open(INITCTL_FILE, O_RDONLY);
                if (fd >= 0) {
                    char buf[16];
                    ssize_t n = read(fd, buf, sizeof(buf) - 1);
                    close(fd);
                    if (n > 0) {
                        buf[n] = '\0';
                        req_level = atoi(buf);
                    }
                    unlink(INITCTL_FILE);
                }
                if (req_level >= 0 && req_level != g_current_runlevel) {
                    switch_runlevel(req_level);
                }
            }

            int status = 0;
            pid_t reaped;
            bool reaped_any = false;
            while ((reaped = waitpid(-1, &status, WNOHANG)) > 0) {
                reaped_any = true;
                if (reaped == g_getty_pid && g_current_runlevel == 3) {
                    printf("init: getty (PID %d) exited, respawning...\n", reaped);
                    sleep(1);
                    g_getty_pid = spawn_getty("/dev/console");
                }
            }

            if (!reaped_any) {
                sleep(1);
            }
        }
    }

    printf("init: Usage: init [0|1|3|6]\n");
    return 0;
}
