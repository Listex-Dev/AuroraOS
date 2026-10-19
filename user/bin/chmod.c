/*
 * AuroraOS /bin/chmod - Change file mode bits
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <octal_mode> <file>\n", argv[0]);
        return 1;
    }

    char *endptr = NULL;
    mode_t mode = (mode_t)strtoul(argv[1], &endptr, 8);
    if (!endptr || *endptr != '\0') {
        fprintf(stderr, "chmod: invalid mode: '%s'\n", argv[1]);
        return 1;
    }

    if (chmod(argv[2], mode) != 0) {
        fprintf(stderr, "chmod: changing permissions of '%s': %s\n", argv[2], strerror(errno));
        return 1;
    }

    return 0;
}
