#include <aurora/proc/binaries.h>
#include <aurora/string.h>

extern char bin_hello_start[],    bin_hello_end[];
extern char bin_ls_start[],       bin_ls_end[];
extern char bin_cat_start[],      bin_cat_end[];
extern char bin_cp_start[],       bin_cp_end[];
extern char bin_ps_start[],       bin_ps_end[];
extern char bin_vi_start[],       bin_vi_end[];
extern char bin_grep_start[],     bin_grep_end[];
extern char bin_test_m10_start[], bin_test_m10_end[];
extern char bin_sleep_start[],    bin_sleep_end[];
extern char bin_sh_start[],       bin_sh_end[];
extern char bin_getty_start[],    bin_getty_end[];
extern char bin_login_start[],    bin_login_end[];
extern char bin_init_start[],     bin_init_end[];
extern char bin_test_m11_start[], bin_test_m11_end[];

struct binary_entry {
    const char *path;
    const char *start;
    const char *end;
};

static const struct binary_entry builtin_table[] = {
    { "/bin/hello",    bin_hello_start,    bin_hello_end },
    { "/bin/ls",       bin_ls_start,       bin_ls_end },
    { "/bin/cat",      bin_cat_start,      bin_cat_end },
    { "/bin/cp",       bin_cp_start,       bin_cp_end },
    { "/bin/ps",       bin_ps_start,       bin_ps_end },
    { "/bin/vi",       bin_vi_start,       bin_vi_end },
    { "/bin/grep",     bin_grep_start,     bin_grep_end },
    { "/bin/test_m10", bin_test_m10_start, bin_test_m10_end },
    { "/bin/sleep",    bin_sleep_start,    bin_sleep_end },
    { "/bin/sh",       bin_sh_start,       bin_sh_end },
    { "/bin/getty",    bin_getty_start,    bin_getty_end },
    { "/bin/login",    bin_login_start,    bin_login_end },
    { "/bin/init",     bin_init_start,     bin_init_end },
    { "/sbin/init",    bin_init_start,     bin_init_end },
    { "/bin/test_m11", bin_test_m11_start, bin_test_m11_end },
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
