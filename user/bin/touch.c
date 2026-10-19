#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        int fd = open(argv[i], O_WRONLY | O_CREAT, 0644);
        if (fd >= 0) {
            close(fd);
        }
    }
    return 0;
}
