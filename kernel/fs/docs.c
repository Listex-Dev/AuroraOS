/*
 * AuroraOS Documentation and Package Subsystem Initialization
 *
 * All man pages, handbook documentation, and pkg databases are decoupled
 * and reside as real files on the ext4 rootfs (under /usr/share/man/,
 * /usr/share/doc/, and /var/lib/pkg/).
 *
 * This function only ensures required directory mountpoints exist in VFS
 * and provides minimal fallback configurations if booting on bare ramfs.
 */

#include <aurora/fs/docs.h>
#include <aurora/fs/vfs.h>
#include <aurora/syscall.h>
#include <aurora/string.h>
#include <aurora/printk.h>

void docs_and_pkg_subsys_init(void)
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    /* 1. Ensure directory structure exists */
    sys_mkdir("/usr", 0755);
    sys_mkdir("/usr/share", 0755);
    sys_mkdir("/usr/share/man", 0755);
    sys_mkdir("/usr/share/man/man1", 0755);
    sys_mkdir("/usr/share/man/man5", 0755);
    sys_mkdir("/usr/share/man/man8", 0755);
    sys_mkdir("/usr/share/doc", 0755);
    sys_mkdir("/usr/share/doc/aurora", 0755);

    sys_mkdir("/etc", 0755);
    sys_mkdir("/etc/pkg", 0755);
    sys_mkdir("/etc/pkg/trusted", 0755);

    sys_mkdir("/var", 0755);
    sys_mkdir("/var/lib", 0755);
    sys_mkdir("/var/lib/pkg", 0755);
    sys_mkdir("/var/cache", 0755);
    sys_mkdir("/var/cache/pkg", 0755);
    sys_mkdir("/packages", 0755);

    /* 2. Fallback key if not present on rootfs */
    int fd = sys_open("/etc/pkg/trusted/aurora.key", O_RDONLY, 0);
    if (fd >= 0) {
        sys_close(fd);
    } else {
        fd = sys_open("/etc/pkg/trusted/aurora.key", O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            const char *key = "aurora-os-trusted-key-v1-2026-pkg-signing-secret\n";
            sys_write(fd, key, strlen(key));
            sys_close(fd);
        }
    }

    /* 3. Fallback package database if not present on rootfs */
    fd = sys_open("/var/lib/pkg/db", O_RDONLY, 0);
    if (fd >= 0) {
        sys_close(fd);
    } else {
        fd = sys_open("/var/lib/pkg/db", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            const char *db = "Package: coreutils\n"
                             "Version: 1.0.0\n"
                             "Status: installed\n"
                             "Description: Core system utilities for AuroraOS\n"
                             "Files:\n"
                             "  /bin/ls\n"
                             "  /bin/cat\n"
                             "  /bin/cp\n"
                             "  /bin/ps\n"
                             "  /bin/vi\n"
                             "  /bin/grep\n"
                             "  /bin/sh\n\n";
            sys_write(fd, db, strlen(db));
            sys_close(fd);
        }
    }
}
