#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static int cat_fd(int fd)
{
    char buf[4096];
    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(STDOUT_FILENO, buf + written, n - written);
            if (w <= 0) return 1;
            written += w;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc <= 1) {
        return cat_fd(STDIN_FILENO);
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            if (cat_fd(STDIN_FILENO) != 0) status = 1;
            continue;
        }

        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            perror(argv[i]);
            status = 1;
            continue;
        }

        if (cat_fd(fd) != 0) {
            status = 1;
        }
        close(fd);
    }

    return status;
}
