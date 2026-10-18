#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static void format_mode(mode_t m, char *str)
{
    str[0] = S_ISDIR(m) ? 'd' : S_ISCHR(m) ? 'c' : S_ISBLK(m) ? 'b' : S_ISFIFO(m) ? 'p' : '-';
    str[1] = (m & S_IRUSR) ? 'r' : '-';
    str[2] = (m & S_IWUSR) ? 'w' : '-';
    str[3] = (m & S_IXUSR) ? 'x' : '-';
    str[4] = (m & S_IRGRP) ? 'r' : '-';
    str[5] = (m & S_IWGRP) ? 'w' : '-';
    str[6] = (m & S_IXGRP) ? 'x' : '-';
    str[7] = (m & S_IROTH) ? 'r' : '-';
    str[8] = (m & S_IWOTH) ? 'w' : '-';
    str[9] = (m & S_IXOTH) ? 'x' : '-';
    str[10] = '\0';
}

static void list_dir(const char *path, int opt_l, int opt_a)
{
    DIR *d = opendir(path);
    if (!d) {
        /* Maybe it's a file */
        struct stat st;
        if (stat(path, &st) == 0) {
            if (opt_l) {
                char modestr[12];
                format_mode(st.st_mode, modestr);
                printf("%s %8lld %s\n", modestr, (long long)st.st_size, path);
            } else {
                printf("%s\n", path);
            }
            return;
        }
        perror(path);
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!opt_a && ent->d_name[0] == '.') {
            continue;
        }

        if (opt_l) {
            char fullpath[512];
            if (strcmp(path, "/") == 0) {
                snprintf(fullpath, sizeof(fullpath), "/%s", ent->d_name);
            } else {
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);
            }

            struct stat st;
            if (stat(fullpath, &st) == 0) {
                char modestr[12];
                format_mode(st.st_mode, modestr);
                printf("%s %8lld %s\n", modestr, (long long)st.st_size, ent->d_name);
            } else {
                printf("%-10s %8s %s\n", (ent->d_type == DT_DIR) ? "drwxr-xr-x" : "-rw-r--r--", "0", ent->d_name);
            }
        } else {
            printf("%s  ", ent->d_name);
        }
    }
    if (!opt_l) {
        printf("\n");
    }
    closedir(d);
}

int main(int argc, char **argv)
{
    int opt_l = 0;
    int opt_a = 0;
    int opt_h = 0;
    const char *target = ".";

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j]; j++) {
                if (argv[i][j] == 'l') opt_l = 1;
                else if (argv[i][j] == 'a') opt_a = 1;
                else if (argv[i][j] == 'h') opt_h = 1;
            }
        } else {
            target = argv[i];
        }
    }
    (void)opt_h;

    list_dir(target, opt_l, opt_a);
    return 0;
}
