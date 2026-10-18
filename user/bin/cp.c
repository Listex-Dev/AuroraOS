#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>

static int copy_file(const char *src, const char *dst)
{
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) {
        perror(src);
        return 1;
    }

    struct stat st;
    if (fstat(sfd, &st) < 0) {
        close(sfd);
        perror(src);
        return 1;
    }

    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dfd < 0) {
        close(sfd);
        perror(dst);
        return 1;
    }

    char buf[4096];
    while (1) {
        ssize_t n = read(sfd, buf, sizeof(buf));
        if (n <= 0) break;
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(dfd, buf + written, n - written);
            if (w <= 0) {
                close(sfd);
                close(dfd);
                return 1;
            }
            written += w;
        }
    }

    close(sfd);
    close(dfd);
    return 0;
}

static int copy_recursive(const char *src, const char *dst)
{
    struct stat st;
    if (stat(src, &st) < 0) {
        perror(src);
        return 1;
    }

    if (!S_ISDIR(st.st_mode)) {
        return copy_file(src, dst);
    }

    mkdir(dst, 0755);
    DIR *d = opendir(src);
    if (!d) {
        perror(src);
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        char src_sub[512];
        char dst_sub[512];
        snprintf(src_sub, sizeof(src_sub), "%s/%s", src, ent->d_name);
        snprintf(dst_sub, sizeof(dst_sub), "%s/%s", dst, ent->d_name);
        copy_recursive(src_sub, dst_sub);
    }
    closedir(d);
    return 0;
}

int main(int argc, char **argv)
{
    int opt_r = 0;
    const char *src = NULL;
    const char *dst = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-R") == 0) {
            opt_r = 1;
        } else if (!src) {
            src = argv[i];
        } else if (!dst) {
            dst = argv[i];
        }
    }

    if (!src || !dst) {
        fprintf(stderr, "Usage: cp [-r] <source> <dest>\n");
        return 1;
    }

    if (opt_r) {
        return copy_recursive(src, dst);
    } else {
        return copy_file(src, dst);
    }
}
