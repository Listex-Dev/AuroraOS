#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int match_pattern(const char *line, const char *pattern, int opt_i)
{
    if (opt_i) {
        size_t plen = strlen(pattern);
        size_t llen = strlen(line);
        if (plen > llen) return 0;
        for (size_t i = 0; i <= llen - plen; i++) {
            if (strncasecmp(line + i, pattern, plen) == 0) {
                return 1;
            }
        }
        return 0;
    } else {
        return (strstr(line, pattern) != NULL);
    }
}

static int grep_stream(FILE *f, const char *pattern, const char *prefix, int opt_i, int opt_n, int opt_v, int opt_c)
{
    char line[4096];
    int lineno = 0;
    int matches = 0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        int hit = match_pattern(line, pattern, opt_i);
        if (opt_v) hit = !hit;

        if (hit) {
            matches++;
            if (!opt_c) {
                if (prefix) printf("%s:", prefix);
                if (opt_n) printf("%d:", lineno);
                printf("%s\n", line);
            }
        }
    }

    if (opt_c) {
        if (prefix) printf("%s:", prefix);
        printf("%d\n", matches);
    }

    return (matches > 0) ? 0 : 1;
}

int main(int argc, char **argv)
{
    int opt_i = 0;
    int opt_n = 0;
    int opt_v = 0;
    int opt_c = 0;
    const char *pattern = NULL;
    int first_file_idx = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j]; j++) {
                if (argv[i][j] == 'i') opt_i = 1;
                else if (argv[i][j] == 'n') opt_n = 1;
                else if (argv[i][j] == 'v') opt_v = 1;
                else if (argv[i][j] == 'c') opt_c = 1;
            }
        } else if (!pattern) {
            pattern = argv[i];
        } else {
            first_file_idx = i;
            break;
        }
    }

    if (!pattern) {
        fprintf(stderr, "Usage: grep [-i] [-n] [-v] [-c] <pattern> [files...]\n");
        return 2;
    }

    if (first_file_idx == 0) {
        return grep_stream(stdin, pattern, NULL, opt_i, opt_n, opt_v, opt_c);
    }

    int file_count = argc - first_file_idx;
    int overall_hit = 1;

    for (int i = first_file_idx; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            perror(argv[i]);
            continue;
        }
        const char *pfx = (file_count > 1) ? argv[i] : NULL;
        int res = grep_stream(f, pattern, pfx, opt_i, opt_n, opt_v, opt_c);
        if (res == 0) overall_hit = 0;
        fclose(f);
    }

    return overall_hit;
}
