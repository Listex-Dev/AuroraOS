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
extern char bin_syslogd_start[],  bin_syslogd_end[];
extern char bin_logger_start[],   bin_logger_end[];
extern char bin_crond_start[],    bin_crond_end[];
extern char bin_crontab_start[],  bin_crontab_end[];
extern char bin_sshd_start[],     bin_sshd_end[];
extern char bin_telinit_start[],  bin_telinit_end[];
extern char bin_touch_start[],    bin_touch_end[];
extern char bin_echo_start[],     bin_echo_end[];
extern char bin_test_m16_start[], bin_test_m16_end[];

struct binary_entry {
    const char *path;
    const char *start;
    const char *end;
    mode_t mode;
    uid_t uid;
    gid_t gid;
};

static const struct binary_entry builtin_table[] = {
    { "/bin/hello",    bin_hello_start,    bin_hello_end,    0755,  0, 0 },
    { "/bin/ls",       bin_ls_start,       bin_ls_end,       0755,  0, 0 },
    { "/bin/cat",      bin_cat_start,      bin_cat_end,      0755,  0, 0 },
    { "/bin/cp",       bin_cp_start,       bin_cp_end,       0755,  0, 0 },
    { "/bin/ps",       bin_ps_start,       bin_ps_end,       0755,  0, 0 },
    { "/bin/vi",       bin_vi_start,       bin_vi_end,       0755,  0, 0 },
    { "/bin/grep",     bin_grep_start,     bin_grep_end,     0755,  0, 0 },
    { "/bin/test_m10", bin_test_m10_start, bin_test_m10_end, 0755,  0, 0 },
    { "/bin/sleep",    bin_sleep_start,    bin_sleep_end,    0755,  0, 0 },
    { "/bin/sh",       bin_sh_start,       bin_sh_end,       0755,  0, 0 },
    { "/bin/getty",    bin_getty_start,    bin_getty_end,    0755,  0, 0 },
    { "/bin/login",    bin_login_start,    bin_login_end,    0755,  0, 0 },
    { "/bin/init",     bin_init_start,     bin_init_end,     0755,  0, 0 },
    { "/sbin/init",    bin_init_start,     bin_init_end,     0755,  0, 0 },
    { "/bin/test_m11", bin_test_m11_start, bin_test_m11_end, 0755,  0, 0 },
    { "/bin/mknod",    bin_mknod_start,    bin_mknod_end,    0755,  0, 0 },
    { "/bin/fdisk",    bin_fdisk_start,    bin_fdisk_end,    0755,  0, 0 },
    { "/sbin/fdisk",   bin_fdisk_start,    bin_fdisk_end,    0755,  0, 0 },
    { "/bin/mount",    bin_mount_start,    bin_mount_end,    0755,  0, 0 },
    { "/bin/test_m12", bin_test_m12_start, bin_test_m12_end, 0755,  0, 0 },
    { "/bin/ping",     bin_ping_start,     bin_ping_end,     0755,  0, 0 },
    { "/bin/test_m13", bin_test_m13_start, bin_test_m13_end, 0755,  0, 0 },
    { "/bin/dhclient", bin_dhclient_start, bin_dhclient_end, 0755,  0, 0 },
    { "/sbin/dhclient",bin_dhclient_start, bin_dhclient_end, 0755,  0, 0 },
    { "/bin/nslookup", bin_nslookup_start, bin_nslookup_end, 0755,  0, 0 },
    { "/bin/ifconfig", bin_ifconfig_start, bin_ifconfig_end, 0755,  0, 0 },
    { "/sbin/ifconfig",bin_ifconfig_start, bin_ifconfig_end, 0755,  0, 0 },
    { "/bin/netstat",  bin_netstat_start,  bin_netstat_end,  0755,  0, 0 },
    { "/bin/wget",     bin_wget_start,     bin_wget_end,     0755,  0, 0 },
    { "/bin/test_m14", bin_test_m14_start, bin_test_m14_end, 0755,  0, 0 },
    { "/bin/id",       bin_id_start,       bin_id_end,       0755,  0, 0 },
    { "/usr/bin/id",   bin_id_start,       bin_id_end,       0755,  0, 0 },
    { "/bin/su",       bin_su_start,       bin_su_end,       04755, 0, 0 },
    { "/usr/bin/su",   bin_su_start,       bin_su_end,       04755, 0, 0 },
    { "/bin/chmod",    bin_chmod_start,    bin_chmod_end,    0755,  0, 0 },
    { "/bin/test_m15", bin_test_m15_start, bin_test_m15_end, 0755,  0, 0 },
    { "/bin/syslogd",  bin_syslogd_start,  bin_syslogd_end,  0755,  0, 0 },
    { "/sbin/syslogd", bin_syslogd_start,  bin_syslogd_end,  0755,  0, 0 },
    { "/bin/logger",   bin_logger_start,   bin_logger_end,   0755,  0, 0 },
    { "/usr/bin/logger", bin_logger_start, bin_logger_end,   0755,  0, 0 },
    { "/bin/crond",    bin_crond_start,    bin_crond_end,    0755,  0, 0 },
    { "/sbin/crond",   bin_crond_start,    bin_crond_end,    0755,  0, 0 },
    { "/bin/crontab",  bin_crontab_start,  bin_crontab_end,  0755,  0, 0 },
    { "/usr/bin/crontab", bin_crontab_start, bin_crontab_end, 0755, 0, 0 },
    { "/bin/sshd",     bin_sshd_start,     bin_sshd_end,     0755,  0, 0 },
    { "/sbin/sshd",    bin_sshd_start,     bin_sshd_end,     0755,  0, 0 },
    { "/bin/telinit",  bin_telinit_start,  bin_telinit_end,  0755,  0, 0 },
    { "/sbin/telinit", bin_telinit_start,  bin_telinit_end,  0755,  0, 0 },
    { "/bin/touch",    bin_touch_start,    bin_touch_end,    0755,  0, 0 },
    { "/usr/bin/touch",bin_touch_start,    bin_touch_end,    0755,  0, 0 },
    { "/bin/echo",     bin_echo_start,     bin_echo_end,     0755,  0, 0 },
    { "/usr/bin/echo", bin_echo_start,     bin_echo_end,     0755,  0, 0 },
    { "/bin/test_m16", bin_test_m16_start, bin_test_m16_end, 0755,  0, 0 },
    { NULL, NULL, NULL, 0, 0, 0 }
};

const void *find_builtin_binary_meta(const char *path, size_t *size_out, mode_t *mode_out, uid_t *uid_out, gid_t *gid_out)
{
    if (!path) {
        return NULL;
    }

    for (int i = 0; builtin_table[i].path != NULL; i++) {
        if (strcmp(builtin_table[i].path, path) == 0) {
            if (size_out) {
                *size_out = builtin_table[i].end - builtin_table[i].start;
            }
            if (mode_out) {
                *mode_out = builtin_table[i].mode ? builtin_table[i].mode : 0755;
            }
            if (uid_out) {
                *uid_out = builtin_table[i].uid;
            }
            if (gid_out) {
                *gid_out = builtin_table[i].gid;
            }
            return (const void *)builtin_table[i].start;
        }
    }

    return NULL;
}

const void *find_builtin_binary(const char *path, size_t *size_out)
{
    return find_builtin_binary_meta(path, size_out, NULL, NULL, NULL);
}
