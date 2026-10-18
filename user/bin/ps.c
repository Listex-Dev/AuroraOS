#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>

static void show_process_info(const char *pid_str, const char *status_path)
{
    FILE *f = fopen(status_path, "r");
    if (!f) return;

    char name[64] = "unknown";
    char state[16] = "R";
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Name:", 5) == 0) {
            char *p = line + 5;
            while (*p == ' ' || *p == '\t') p++;
            size_t len = strlen(p);
            if (len > 0 && p[len - 1] == '\n') p[len - 1] = '\0';
            strncpy(name, p, sizeof(name) - 1);
        } else if (strncmp(line, "State:", 6) == 0) {
            char *p = line + 6;
            while (*p == ' ' || *p == '\t') p++;
            state[0] = *p;
            state[1] = '\0';
        }
    }
    fclose(f);

    printf("%5s %-8s %-4s %-8s %s\n", pid_str, "tty1", state, "00:00:00", name);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    printf("%5s %-8s %-4s %-8s %s\n", "PID", "TTY", "STAT", "TIME", "COMMAND");

    /* Show kernel/init task 1 */
    printf("%5s %-8s %-4s %-8s %s\n", "1", "tty1", "S", "00:00:00", "init");

    /* Iterate /proc to find any process directories */
    DIR *d = opendir("/proc");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] >= '0' && ent->d_name[0] <= '9') {
                char stat_path[256];
                snprintf(stat_path, sizeof(stat_path), "/proc/%s/status", ent->d_name);
                show_process_info(ent->d_name, stat_path);
            }
        }
        closedir(d);
    }

    /* Also show current running process */
    char cur_pid[16];
    snprintf(cur_pid, sizeof(cur_pid), "%d", getpid());
    show_process_info(cur_pid, "/proc/self/status");

    return 0;
}
