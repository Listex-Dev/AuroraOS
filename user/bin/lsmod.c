/*
 * lsmod - Show the status of modules in the AuroraOS kernel
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    int fd = open("/proc/modules", O_RDONLY);
    if (fd < 0) {
        perror("lsmod: cannot open /proc/modules");
        return 1;
    }

    char buf[1024];
    ssize_t bytes_read = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytes_read < 0) {
        perror("lsmod: read /proc/modules");
        return 1;
    }

    buf[bytes_read] = '\0';

    printf("%-20s %8s %7s\n", "Module", "Size", "Used by");

    char *line = buf;
    while (*line) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        if (strlen(line) > 0) {
            char mod_name[64];
            unsigned long size = 0;
            int refcnt = 0;
            char state[32];
            unsigned long long base = 0;

            if (sscanf(line, "%s %lu %d %s %llx", mod_name, &size, &refcnt, state, &base) >= 3) {
                printf("%-20s %8lu %7d\n", mod_name, size, refcnt);
            } else {
                printf("%s\n", line);
            }
        }

        if (!newline) break;
        line = newline + 1;
    }

    return 0;
}
