/*
 * insmod - Insert a module into the AuroraOS kernel
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: insmod <filename.ko> [args...]\n");
        return 1;
    }

    const char *path = argv[1];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("insmod: open");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("insmod: fstat");
        close(fd);
        return 1;
    }

    if (st.st_size <= 0) {
        fprintf(stderr, "insmod: empty file '%s'\n", path);
        close(fd);
        return 1;
    }

    void *buf = malloc(st.st_size);
    if (!buf) {
        fprintf(stderr, "insmod: out of memory\n");
        close(fd);
        return 1;
    }

    ssize_t rd = read(fd, buf, st.st_size);
    close(fd);

    if (rd != (ssize_t)st.st_size) {
        fprintf(stderr, "insmod: failed to read complete file\n");
        free(buf);
        return 1;
    }

    /* Build argument string */
    char args[256];
    args[0] = '\0';
    for (int i = 2; i < argc; i++) {
        if (i > 2) strncat(args, " ", sizeof(args) - strlen(args) - 1);
        strncat(args, argv[i], sizeof(args) - strlen(args) - 1);
    }

    /* If no explicit name= argument, extract module name from filename */
    if (strstr(args, "name=") == NULL) {
        const char *slash = strrchr(path, '/');
        const char *fname = slash ? slash + 1 : path;
        char name_arg[128];
        snprintf(name_arg, sizeof(name_arg), "name=%s", fname);
        char *dot = strstr(name_arg, ".ko");
        if (dot) *dot = '\0';

        if (args[0]) {
            strncat(name_arg, " ", sizeof(name_arg) - strlen(name_arg) - 1);
            strncat(name_arg, args, sizeof(name_arg) - strlen(name_arg) - 1);
        }
        strncpy(args, name_arg, sizeof(args) - 1);
    }

    int ret = init_module(buf, st.st_size, args);
    free(buf);

    if (ret != 0) {
        perror("insmod: init_module");
        return 1;
    }

    return 0;
}
