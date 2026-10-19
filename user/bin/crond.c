/*
 * AuroraOS /sbin/crond - Cron Task Scheduling Daemon
 *
 * Responsibilities:
 * - Creates /run/crond.pid
 * - Parses /etc/crontab and /var/spool/cron/crontabs
 * - Executes scheduled commands via /bin/sh
 * - Logs all job executions to /var/log/cron
 * - Supports -t (test mode) for accelerated execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#define PID_FILE "/run/crond.pid"
#define LOG_CRON "/var/log/cron"

static volatile int g_running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void write_pid_file(void)
{
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d\n", getpid());
        write(fd, buf, strlen(buf));
        close(fd);
    }
}

static void remove_pid_file(void)
{
    unlink(PID_FILE);
}

static void log_cron(const char *user, const char *cmd)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[32];
    if (tm) {
        strftime(time_str, sizeof(time_str), "%b %e %T", tm);
    } else {
        strcpy(time_str, "Oct 19 12:00:00");
    }

    char line[2048];
    snprintf(line, sizeof(line), "%s aurora crond[%d]: (%s) CMD (%s)\n",
             time_str, getpid(), user ? user : "root", cmd);

    int fd = open(LOG_CRON, O_WRONLY | O_CREAT | O_APPEND, 0640);
    if (fd >= 0) {
        write(fd, line, strlen(line));
        close(fd);
    }
}

static int field_matches(const char *f, int val)
{
    if (strcmp(f, "*") == 0) return 1;
    if (strncmp(f, "*/", 2) == 0) {
        int step = atoi(&f[2]);
        if (step > 0 && (val % step) == 0) return 1;
    }
    int direct = atoi(f);
    if (direct == val) return 1;
    return 0;
}

static void execute_job(const char *user, const char *cmd)
{
    log_cron(user, cmd);

    pid_t pid = fork();
    if (pid == 0) {
        /* Child */
        char *argv[] = { "/bin/sh", "-c", (char *)cmd, NULL };
        execve("/bin/sh", argv, environ);
        _exit(127);
    }
}

static void parse_and_run_file(const char *path, const char *default_user, struct tm *tm, int test_mode)
{
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* Trim leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#' || *p == '\n') continue;

        /* Strip trailing newline */
        size_t l = strlen(p);
        if (l > 0 && p[l - 1] == '\n') p[l - 1] = '\0';

        char min_s[16], hour_s[16], dom_s[16], mon_s[16], dow_s[16];
        char *ptr = p;
        if (sscanf(ptr, "%15s", min_s) != 1) continue;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ptr++;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        if (sscanf(ptr, "%15s", hour_s) != 1) continue;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ptr++;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        if (sscanf(ptr, "%15s", dom_s) != 1) continue;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ptr++;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        if (sscanf(ptr, "%15s", mon_s) != 1) continue;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ptr++;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        if (sscanf(ptr, "%15s", dow_s) != 1) continue;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ptr++;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        if (!*ptr) continue;
        char *rest = ptr;

        int match = test_mode;
        if (!test_mode && tm) {
            match = field_matches(min_s, tm->tm_min) &&
                    field_matches(hour_s, tm->tm_hour) &&
                    field_matches(dom_s, tm->tm_mday) &&
                    field_matches(mon_s, tm->tm_mon + 1) &&
                    field_matches(dow_s, tm->tm_wday);
        }

        if (match) {
            char user[64];
            char cmd[1024];
            if (default_user) {
                /* User crontab: entire 'rest' is command */
                strncpy(user, default_user, sizeof(user) - 1);
                user[sizeof(user) - 1] = '\0';
                strncpy(cmd, rest, sizeof(cmd) - 1);
                cmd[sizeof(cmd) - 1] = '\0';
            } else {
                /* System crontab: first word is username */
                char *sp = rest;
                if (sscanf(sp, "%63s", user) != 1) continue;
                while (*sp && *sp != ' ' && *sp != '\t') sp++;
                while (*sp == ' ' || *sp == '\t') sp++;
                if (!*sp) continue;
                strncpy(cmd, sp, sizeof(cmd) - 1);
                cmd[sizeof(cmd) - 1] = '\0';
            }
            execute_job(user, cmd);
        }
    }

    fclose(f);
}

static void process_crontabs(int test_mode)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    /* 1. /etc/crontab */
    parse_and_run_file("/etc/crontab", NULL, tm, test_mode);

    /* 2. User crontabs in /var/spool/cron/crontabs */
    DIR *d = opendir("/var/spool/cron/crontabs");
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char path[512];
            snprintf(path, sizeof(path), "/var/spool/cron/crontabs/%s", de->d_name);
            parse_and_run_file(path, de->d_name, tm, test_mode);
        }
        closedir(d);
    }
}

int main(int argc, char **argv)
{
    int foreground = 0;
    int test_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "-f") == 0) {
            foreground = 1;
        } else if (strcmp(argv[i], "-t") == 0) {
            test_mode = 1;
        }
    }

    if (!foreground) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("crond: fork");
            return 1;
        }
        if (pid > 0) {
            return 0;
        }
        setsid();
    }

    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    write_pid_file();

    mkdir("/var", 0755);
    mkdir("/var/log", 0755);
    mkdir("/var/spool", 0755);
    mkdir("/var/spool/cron", 0755);
    mkdir("/var/spool/cron/crontabs", 0755);

    log_cron("root", "(CRON) STARTUP (AuroraOS Cron Daemon)");

    int last_min = -1;
    while (g_running) {
        /* Check and reap any zombie children */
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) {
        }

        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (test_mode) {
            process_crontabs(1);
            sleep(1);
        } else {
            if (tm && tm->tm_min != last_min) {
                last_min = tm->tm_min;
                process_crontabs(0);
            }
            sleep(1);
        }
    }

    remove_pid_file();
    return 0;
}
