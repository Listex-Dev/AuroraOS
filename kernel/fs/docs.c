/*
 * AuroraOS Documentation and Package Subsystem Initialization
 * Populates system man-pages and default package repository keys.
 */

#include <aurora/fs/docs.h>
#include <aurora/fs/vfs.h>
#include <aurora/syscall.h>
#include <aurora/string.h>
#include <aurora/printk.h>

extern char pkg_sample_start[], pkg_sample_end[];
extern char pkg_sample_corrupt_start[], pkg_sample_corrupt_end[];

static void write_file(const char *path, const char *content, mode_t mode)
{
    int fd = sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd >= 0) {
        sys_write(fd, content, strlen(content));
        sys_close(fd);
    }
}

static void write_binary_file(const char *path, const void *data, size_t size, mode_t mode)
{
    int fd = sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd >= 0) {
        if (size > 0 && data) {
            sys_write(fd, data, size);
        }
        sys_close(fd);
    }
}

void docs_and_pkg_subsys_init(void)
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    /* 1. Create directory structure */
    sys_mkdir("/usr", 0755);
    sys_mkdir("/usr/share", 0755);
    sys_mkdir("/usr/share/man", 0755);
    sys_mkdir("/usr/share/man/man1", 0755);
    sys_mkdir("/usr/share/man/man5", 0755);
    sys_mkdir("/usr/share/man/man8", 0755);

    sys_mkdir("/etc", 0755);
    sys_mkdir("/etc/pkg", 0755);
    sys_mkdir("/etc/pkg/trusted", 0755);

    sys_mkdir("/var", 0755);
    sys_mkdir("/var/lib", 0755);
    sys_mkdir("/var/lib/pkg", 0755);
    sys_mkdir("/var/cache", 0755);
    sys_mkdir("/var/cache/pkg", 0755);
    sys_mkdir("/packages", 0755);

    /* 2. Trusted signing key */
    write_file("/etc/pkg/trusted/aurora.key",
               "aurora-os-trusted-key-v1-2026-pkg-signing-secret\n", 0600);

    /* 3. Base package database */
    write_file("/var/lib/pkg/db",
               "Package: coreutils\n"
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
               "  /bin/sh\n\n",
               0644);

    /* 4. Write Core Man-Pages */

    /* Section 1: User Utilities */
    write_file("/usr/share/man/man1/ls.1",
               "NAME\n"
               "    ls - list directory contents\n\n"
               "SYNOPSIS\n"
               "    ls [-l] [-a] [-h] [-R] [FILE]...\n\n"
               "DESCRIPTION\n"
               "    List information about the FILEs (the current directory by default).\n\n"
               "OPTIONS\n"
               "    -l     use a long listing format\n"
               "    -a     do not ignore entries starting with .\n"
               "    -h     with -l, print human readable sizes\n"
               "    -R     list subdirectories recursively\n\n"
               "SEE ALSO\n"
               "    cat(1), stat(1)\n", 0644);

    write_file("/usr/share/man/man1/cat.1",
               "NAME\n"
               "    cat - concatenate files and print on the standard output\n\n"
               "SYNOPSIS\n"
               "    cat [FILE]...\n\n"
               "DESCRIPTION\n"
               "    Concatenate FILE(s) to standard output.\n\n"
               "SEE ALSO\n"
               "    cp(1), echo(1)\n", 0644);

    write_file("/usr/share/man/man1/cp.1",
               "NAME\n"
               "    cp - copy files and directories\n\n"
               "SYNOPSIS\n"
               "    cp [-r] SOURCE DEST\n\n"
               "DESCRIPTION\n"
               "    Copy SOURCE to DEST.\n\n"
               "SEE ALSO\n"
               "    cat(1), ls(1)\n", 0644);

    write_file("/usr/share/man/man1/ps.1",
               "NAME\n"
               "    ps - report a snapshot of the current processes\n\n"
               "SYNOPSIS\n"
               "    ps [aux]\n\n"
               "DESCRIPTION\n"
               "    ps displays information about active system processes.\n\n"
               "SEE ALSO\n"
               "    kill(1), top(1)\n", 0644);

    write_file("/usr/share/man/man1/vi.1",
               "NAME\n"
               "    vi - screen-oriented visual text editor\n\n"
               "SYNOPSIS\n"
               "    vi [FILE]\n\n"
               "DESCRIPTION\n"
               "    Full-screen modal text editor operating in raw terminal mode.\n\n"
               "SEE ALSO\n"
               "    cat(1), sh(1)\n", 0644);

    write_file("/usr/share/man/man1/grep.1",
               "NAME\n"
               "    grep - print lines matching a pattern\n\n"
               "SYNOPSIS\n"
               "    grep [-r] [-n] [-i] [-v] PATTERN [FILE]...\n\n"
               "DESCRIPTION\n"
               "    grep searches for PATTERN in each FILE.\n\n"
               "SEE ALSO\n"
               "    sed(1), sh(1)\n", 0644);

    write_file("/usr/share/man/man1/sh.1",
               "NAME\n"
               "    sh - command language interpreter with pipes and job control\n\n"
               "SYNOPSIS\n"
               "    sh [-c string] [file]\n\n"
               "DESCRIPTION\n"
               "    Standard Unix command language interpreter.\n\n"
               "SEE ALSO\n"
               "    init(8), login(1)\n", 0644);

    write_file("/usr/share/man/man1/su.1",
               "NAME\n"
               "    su - run a command with substitute user and group ID\n\n"
               "SYNOPSIS\n"
               "    su [USER]\n\n"
               "DESCRIPTION\n"
               "    su allows a user to become another user during a session.\n\n"
               "SEE ALSO\n"
               "    id(1), login(1)\n", 0644);

    write_file("/usr/share/man/man1/chmod.1",
               "NAME\n"
               "    chmod - change file mode bits\n\n"
               "SYNOPSIS\n"
               "    chmod MODE FILE...\n\n"
               "DESCRIPTION\n"
               "    chmod changes the file mode bits of each given file according to MODE.\n\n"
               "SEE ALSO\n"
               "    ls(1), stat(1)\n", 0644);

    write_file("/usr/share/man/man1/pkg.1",
               "NAME\n"
               "    pkg - AuroraOS package manager\n\n"
               "SYNOPSIS\n"
               "    pkg install <file.aurpkg>\n"
               "    pkg remove <name>\n"
               "    pkg list\n"
               "    pkg info <name>\n"
               "    pkg verify <file|name>\n\n"
               "DESCRIPTION\n"
               "    Manages binary packages with cryptographic signature verification.\n\n"
               "SEE ALSO\n"
               "    man(1)\n", 0644);

    write_file("/usr/share/man/man1/man.1",
               "NAME\n"
               "    man - format and display the on-line manual pages\n\n"
               "SYNOPSIS\n"
               "    man [section] command\n\n"
               "DESCRIPTION\n"
               "    man is the system's manual pager.\n\n"
               "SEE ALSO\n"
               "    pkg(1)\n", 0644);

    /* Section 5: File Formats */
    write_file("/usr/share/man/man5/passwd.5",
               "NAME\n"
               "    passwd - the password file\n\n"
               "DESCRIPTION\n"
               "    /etc/passwd contains user account information.\n"
               "    Format: username:password:UID:GID:GECOS:home:shell\n\n"
               "SEE ALSO\n"
               "    shadow(5), login(1), su(1)\n", 0644);

    write_file("/usr/share/man/man5/fstab.5",
               "NAME\n"
               "    fstab - static information about the filesystems\n\n"
               "DESCRIPTION\n"
               "    /etc/fstab contains file system mount descriptions.\n\n"
               "SEE ALSO\n"
               "    mount(8)\n", 0644);

    /* Section 8: System Administration */
    write_file("/usr/share/man/man8/init.8",
               "NAME\n"
               "    init - AuroraOS process control initialization (PID 1)\n\n"
               "SYNOPSIS\n"
               "    init [runlevel]\n\n"
               "DESCRIPTION\n"
               "    init is the parent of all processes on the system.\n\n"
               "SEE ALSO\n"
               "    telinit(8), sshd(8), syslogd(8)\n", 0644);

    write_file("/usr/share/man/man8/syslogd.8",
               "NAME\n"
               "    syslogd - system logging daemon\n\n"
               "DESCRIPTION\n"
               "    syslogd logs messages to /var/log/messages and /var/log/auth.log.\n\n"
               "SEE ALSO\n"
               "    logger(1)\n", 0644);

    write_file("/usr/share/man/man8/sshd.8",
               "NAME\n"
               "    sshd - AuroraOS remote shell daemon\n\n"
               "DESCRIPTION\n"
               "    sshd provides secure remote terminal access on TCP port 22.\n\n"
               "SEE ALSO\n"
               "    init(8), sh(1)\n", 0644);

    write_file("/usr/share/man/man8/crond.8",
               "NAME\n"
               "    crond - cron daemon for scheduled commands\n\n"
               "DESCRIPTION\n"
               "    crond executes scheduled jobs specified in /etc/crontab.\n\n"
               "SEE ALSO\n"
               "    crontab(1)\n", 0644);

    write_file("/usr/share/man/man8/ifconfig.8",
               "NAME\n"
               "    ifconfig - configure network interfaces\n\n"
               "SYNOPSIS\n"
               "    ifconfig [interface] [address] [up|down]\n\n"
               "DESCRIPTION\n"
               "    Configures kernel network interfaces (lo, eth0).\n\n"
               "SEE ALSO\n"
               "    netstat(1), ping(1)\n", 0644);

    write_file("/usr/share/man/man1/dmesg.1",
               "NAME\n"
               "    dmesg - print or control the kernel ring buffer\n\n"
               "SYNOPSIS\n"
               "    dmesg\n\n"
               "DESCRIPTION\n"
               "    dmesg is used to examine or control the kernel ring buffer.\n\n"
               "SEE ALSO\n"
               "    syslogd(8)\n", 0644);

    /* 5. System Documentation */
    sys_mkdir("/usr/share/doc", 0755);
    sys_mkdir("/usr/share/doc/aurora", 0755);

    write_file("/usr/share/doc/aurora/HANDBOOK.txt",
               "AuroraOS 1.0.0 System Handbook\n"
               "=============================\n\n"
               "1. System Architecture\n"
               "AuroraOS is a modern monolithic POSIX-compatible operating system for x86-64 long mode.\n"
               "All system abstractions follow standard Unix principles: everything is a file,\n"
               "virtual filesystems (/proc, /sys, /dev, /tmp), and ring-3 processes with fast syscall gate.\n\n"
               "2. Process Management\n"
               "Full fork/execve/wait4 lifecycle with signals, job control, and zombie reaping.\n\n"
               "3. Package Management\n"
               "Managed via pkg with cryptographic HMAC-SHA256 signatures and /var/lib/pkg/db.\n\n"
               "4. Services & Daemons\n"
               "SysV service framework under /etc/init.d/ managing syslogd, crond, networking, and sshd.\n",
               0644);

    write_file("/usr/share/doc/aurora/ABI.txt",
               "AuroraOS System Call ABI Specification\n"
               "======================================\n\n"
               "Architecture: x86-64 (AMD64 Long Mode)\n"
               "Instruction: syscall / sysretq\n\n"
               "Registers:\n"
               "  Syscall Number:  RAX\n"
               "  Argument 1:      RDI\n"
               "  Argument 2:      RSI\n"
               "  Argument 3:      RDX\n"
               "  Argument 4:      R10\n"
               "  Argument 5:      R8\n"
               "  Argument 6:      R9\n"
               "  Return Value:    RAX (>=0 success, -errno on error)\n",
               0644);

    write_file("/usr/share/doc/aurora/PORTING.txt",
               "AuroraOS Application Porting Guide\n"
               "==================================\n\n"
               "1. Runtime:\n"
               "AuroraOS libc provides POSIX unistd, stdio, stdlib, dirent, termios, signal, sha256.\n\n"
               "2. Binary Format:\n"
               "ELF64 static or dynamic executables loaded via ELF PT_LOAD segments.\n",
               0644);

    /* 5. Deploy test packages to /packages/ */
    size_t sample_sz = (size_t)(pkg_sample_end - pkg_sample_start);
    if (sample_sz > 0) {
        write_binary_file("/packages/sample.aurpkg", pkg_sample_start, sample_sz, 0644);
    }

    size_t corrupt_sz = (size_t)(pkg_sample_corrupt_end - pkg_sample_corrupt_start);
    if (corrupt_sz > 0) {
        write_binary_file("/packages/sample-corrupt.aurpkg", pkg_sample_corrupt_start, corrupt_sz, 0644);
    }
}
