#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>

static void print_mounts(void)
{
    int fd = open("/proc/mounts", O_RDONLY);
    if (fd < 0) {
        perror("/proc/mounts");
        return;
    }

    char buf[1024];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        fputs(buf, stdout);
    }
    close(fd);
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        print_mounts();
        return 0;
    }

    const char *fstype = NULL;
    const char *source = NULL;
    const char *target = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            fstype = argv[++i];
        } else if (!source) {
            source = argv[i];
        } else if (!target) {
            target = argv[i];
        }
    }

    if (!source || !target) {
        fprintf(stderr, "Usage: %s [-t type] device dir\n", argv[0]);
        return 1;
    }

    if (!fstype) {
        if (strstr(source, "proc")) {
            fstype = "procfs";
        } else if (strstr(source, "sys")) {
            fstype = "sysfs";
        } else if (strstr(source, "dev")) {
            fstype = "ext4";
        } else {
            fstype = "ext4";
        }
    }

    int ret = mount(source, target, fstype, 0, NULL);
    if (ret != 0 && strcmp(fstype, "ext4") == 0) {
        /* Retry with ext2 */
        ret = mount(source, target, "ext2", 0, NULL);
    }

    if (ret < 0) {
        perror("mount");
        return 1;
    }

    return 0;
}
