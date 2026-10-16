#include <aurora/proc/binaries.h>
#include <aurora/string.h>

extern char bin_hello_start[];
extern char bin_hello_end[];

struct binary_entry {
    const char *path;
    const char *start;
    const char *end;
};

static const struct binary_entry builtin_table[] = {
    { "/bin/hello", bin_hello_start, bin_hello_end },
    { NULL, NULL, NULL }
};

const void *find_builtin_binary(const char *path, size_t *size_out)
{
    if (!path) {
        return NULL;
    }

    for (int i = 0; builtin_table[i].path != NULL; i++) {
        if (strcmp(builtin_table[i].path, path) == 0) {
            if (size_out) {
                *size_out = builtin_table[i].end - builtin_table[i].start;
            }
            return (const void *)builtin_table[i].start;
        }
    }

    return NULL;
}
