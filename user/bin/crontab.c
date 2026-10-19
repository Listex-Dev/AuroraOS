/*
 * AuroraOS /bin/crontab - Maintain crontab files for individual users
 *
 * Usage:
 *   crontab -l          (list crontab)
 *   crontab -r          (remove crontab)
 *   crontab <file>      (install crontab from file)
 *   crontab -u <user>   (specify user, root only)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>

#define CRON_DIR "/var/spool/cron/crontabs"

int main(int argc, char **argv)
{
    const char *user = "root";
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        user = pw->pw_name;
    }

    int do_list = 0;
    int do_remove = 0;
    const char *install_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            do_list = 1;
        } else if (strcmp(argv[i], "-r") == 0) {
            do_remove = 1;
        } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            if (getuid() != 0) {
                fprintf(stderr, "crontab: must be root to use -u\n");
                return 1;
            }
            user = argv[++i];
        } else if (argv[i][0] != '-') {
            install_file = argv[i];
        }
    }

    mkdir("/var", 0755);
    mkdir("/var/spool", 0755);
    mkdir("/var/spool/cron", 0755);
    mkdir(CRON_DIR, 0755);

    char crontab_path[512];
    snprintf(crontab_path, sizeof(crontab_path), "%s/%s", CRON_DIR, user);

    if (do_list) {
        int fd = open(crontab_path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "no crontab for %s\n", user);
            return 1;
        }
        char buf[1024];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, n);
        }
        close(fd);
        return 0;
    }

    if (do_remove) {
        if (unlink(crontab_path) < 0 && errno != ENOENT) {
            perror("crontab: remove");
            return 1;
        }
        return 0;
    }

    if (install_file) {
        int in_fd = open(install_file, O_RDONLY);
        if (in_fd < 0) {
            perror("crontab: open input file");
            return 1;
        }

        int out_fd = open(crontab_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (out_fd < 0) {
            perror("crontab: open target crontab");
            close(in_fd);
            return 1;
        }

        char buf[1024];
        ssize_t n;
        while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
            write(out_fd, buf, n);
        }

        close(in_fd);
        close(out_fd);
        return 0;
    }

    printf("Usage: crontab [-u user] {-l | -r | <file>}\n");
    return 1;
}
