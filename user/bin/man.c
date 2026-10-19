/*
 * AuroraOS Manual Page Viewer (man)
 * Displays formatted manual pages from /usr/share/man/man{1,5,8}/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static int display_man_page(const char *path, const char *title)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    printf("\n\033[1m%s\033[0m - AuroraOS Manual Page\n\n", title);

    char buf[4096];
    ssize_t n;
    char line[512];
    int line_idx = 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                line[line_idx] = '\0';
                /* Check if line is a section header (all caps, start of line) */
                if (strcmp(line, "NAME") == 0 ||
                    strcmp(line, "SYNOPSIS") == 0 ||
                    strcmp(line, "DESCRIPTION") == 0 ||
                    strcmp(line, "OPTIONS") == 0 ||
                    strcmp(line, "FILES") == 0 ||
                    strcmp(line, "EXIT STATUS") == 0 ||
                    strcmp(line, "SEE ALSO") == 0 ||
                    strcmp(line, "AUTHOR") == 0 ||
                    strcmp(line, "BUGS") == 0) {
                    printf("\033[1m%s\033[0m\n", line);
                } else {
                    printf("%s\n", line);
                }
                line_idx = 0;
            } else if (line_idx < (int)sizeof(line) - 1) {
                line[line_idx++] = c;
            }
        }
    }
    if (line_idx > 0) {
        line[line_idx] = '\0';
        printf("%s\n", line);
    }

    printf("\n");
    close(fd);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("  man <command>             View command manual page\n");
        printf("  man <section> <command>   View page in specific section (1, 5, 8)\n");
        printf("  man -k <keyword>          Search manual descriptions\n");
        return 1;
    }

    if (strcmp(argv[1], "-k") == 0) {
        if (argc < 3) {
            fprintf(stderr, "man: missing search keyword\n");
            return 1;
        }
        printf("Searching manual pages for '%s'...\n", argv[2]);
        /* Simple search demo */
        return 0;
    }

    const char *sec = NULL;
    const char *name = NULL;

    if (argc >= 3 && (argv[1][0] >= '1' && argv[1][0] <= '9')) {
        sec = argv[1];
        name = argv[2];
    } else {
        name = argv[1];
    }

    char path[256];
    char title[128];

    if (sec) {
        snprintf(path, sizeof(path), "/usr/share/man/man%s/%s.%s", sec, name, sec);
        snprintf(title, sizeof(title), "%s(%s)", name, sec);
        if (display_man_page(path, title)) {
            return 0;
        }
    } else {
        /* Search order: man1, man8, man5 */
        snprintf(path, sizeof(path), "/usr/share/man/man1/%s.1", name);
        snprintf(title, sizeof(title), "%s(1)", name);
        if (display_man_page(path, title)) return 0;

        snprintf(path, sizeof(path), "/usr/share/man/man8/%s.8", name);
        snprintf(title, sizeof(title), "%s(8)", name);
        if (display_man_page(path, title)) return 0;

        snprintf(path, sizeof(path), "/usr/share/man/man5/%s.5", name);
        snprintf(title, sizeof(title), "%s(5)", name);
        if (display_man_page(path, title)) return 0;
    }

    fprintf(stderr, "No manual entry for %s\n", name);
    return 1;
}
