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
extern char bin_mknod_start[],    bin_mknod_end[];
extern char bin_fdisk_start[],    bin_fdisk_end[];
extern char bin_mount_start[],    bin_mount_end[];
extern char bin_test_m12_start[], bin_test_m12_end[];
extern char bin_ping_start[],     bin_ping_end[];
extern char bin_test_m13_start[], bin_test_m13_end[];
extern char bin_dhclient_start[], bin_dhclient_end[];
extern char bin_nslookup_start[], bin_nslookup_end[];
extern char bin_ifconfig_start[], bin_ifconfig_end[];
extern char bin_netstat_start[],  bin_netstat_end[];
extern char bin_wget_start[],     bin_wget_end[];
extern char bin_test_m14_start[], bin_test_m14_end[];
extern char bin_id_start[],       bin_id_end[];
extern char bin_su_start[],       bin_su_end[];
extern char bin_chmod_start[],    bin_chmod_end[];
extern char bin_test_m15_start[], bin_test_m15_end[];

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
    { "/bin/mknod",    bin_mknod_start,    bin_mknod_end },
    { "/bin/fdisk",    bin_fdisk_start,    bin_fdisk_end },
    { "/sbin/fdisk",   bin_fdisk_start,    bin_fdisk_end },
    { "/bin/mount",    bin_mount_start,    bin_mount_end },
    { "/bin/test_m12", bin_test_m12_start, bin_test_m12_end },
    { "/bin/ping",     bin_ping_start,     bin_ping_end },
    { "/bin/test_m13", bin_test_m13_start, bin_test_m13_end },
    { "/bin/dhclient", bin_dhclient_start, bin_dhclient_end },
    { "/sbin/dhclient",bin_dhclient_start, bin_dhclient_end },
    { "/bin/nslookup", bin_nslookup_start, bin_nslookup_end },
    { "/bin/ifconfig", bin_ifconfig_start, bin_ifconfig_end },
    { "/sbin/ifconfig",bin_ifconfig_start, bin_ifconfig_end },
    { "/bin/netstat",  bin_netstat_start,  bin_netstat_end },
    { "/bin/wget",     bin_wget_start,     bin_wget_end },
    { "/bin/test_m14", bin_test_m14_start, bin_test_m14_end },
    { "/bin/id",       bin_id_start,       bin_id_end },
    { "/usr/bin/id",   bin_id_start,       bin_id_end },
    { "/bin/su",       bin_su_start,       bin_su_end },
    { "/bin/chmod",    bin_chmod_start,    bin_chmod_end },
    { "/bin/test_m15", bin_test_m15_start, bin_test_m15_end },
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
