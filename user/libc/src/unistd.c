#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <termios.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "syscall.h"

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)__syscall_ret(__syscall3(SYS_read, fd, (long)buf, count));
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return (ssize_t)__syscall_ret(__syscall3(SYS_write, fd, (long)buf, count));
}

int open(const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return (int)__syscall_ret(__syscall3(SYS_open, (long)pathname, flags, mode));
}

int creat(const char *pathname, mode_t mode)
{
    return open(pathname, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int close(int fd)
{
    return (int)__syscall_ret(__syscall1(SYS_close, fd));
}

off_t lseek(int fd, off_t offset, int whence)
{
    return (off_t)__syscall_ret(__syscall3(SYS_lseek, fd, offset, whence));
}

pid_t fork(void)
{
    return (pid_t)__syscall_ret(__syscall0(SYS_fork));
}

int execve(const char *pathname, char *const argv[], char *const envp[])
{
    return (int)__syscall_ret(__syscall3(SYS_execve, (long)pathname, (long)argv, (long)envp));
}

int execv(const char *path, char *const argv[])
{
    return execve(path, argv, environ);
}

int execvp(const char *file, char *const argv[])
{
    if (strchr(file, '/')) {
        return execv(file, argv);
    }
    const char *path = getenv("PATH");
    if (!path) path = "/bin:/usr/bin";
    char candidate[256];
    const char *p = path;
    while (*p) {
        size_t len = 0;
        while (p[len] && p[len] != ':') len++;
        if (len + 1 + strlen(file) < sizeof(candidate)) {
            memcpy(candidate, p, len);
            candidate[len] = '/';
            strcpy(candidate + len + 1, file);
            execv(candidate, argv);
        }
        p += len;
        if (*p == ':') p++;
    }
    return -1;
}

void _exit(int status)
{
    __syscall1(SYS_exit, status);
    while (1) {}
}

pid_t getpid(void)
{
    return (pid_t)__syscall_ret(__syscall0(SYS_getpid));
}

pid_t getppid(void)
{
    return (pid_t)__syscall_ret(__syscall0(SYS_getppid));
}

uid_t getuid(void)
{
    return (uid_t)__syscall_ret(__syscall0(SYS_getuid));
}

gid_t getgid(void)
{
    return (gid_t)__syscall_ret(__syscall0(SYS_getgid));
}

uid_t geteuid(void)
{
    return (uid_t)__syscall_ret(__syscall0(SYS_geteuid));
}

gid_t getegid(void)
{
    return (gid_t)__syscall_ret(__syscall0(SYS_getegid));
}

int setuid(uid_t uid)
{
    return (int)__syscall_ret(__syscall1(SYS_setuid, uid));
}

int setgid(gid_t gid)
{
    return (int)__syscall_ret(__syscall1(SYS_setgid, gid));
}

int setreuid(uid_t ruid, uid_t euid)
{
    return (int)__syscall_ret(__syscall2(SYS_setreuid, ruid, euid));
}

int setregid(gid_t rgid, gid_t egid)
{
    return (int)__syscall_ret(__syscall2(SYS_setregid, rgid, egid));
}

int seteuid(uid_t euid)
{
    return setreuid((uid_t)-1, euid);
}

int setegid(gid_t egid)
{
    return setregid((gid_t)-1, egid);
}

int setpgid(pid_t pid, pid_t pgid)
{
    return (int)__syscall_ret(__syscall2(SYS_setpgid, pid, pgid));
}

pid_t getpgrp(void)
{
    return (pid_t)__syscall_ret(__syscall0(SYS_getpgrp));
}

pid_t getpgid(pid_t pid)
{
    return (pid_t)__syscall_ret(__syscall1(SYS_getpgid, pid));
}

pid_t setsid(void)
{
    return (pid_t)__syscall_ret(__syscall0(SYS_setsid));
}

pid_t tcgetpgrp(int fd)
{
    pid_t pgrp = 0;
    if (ioctl(fd, TIOCGPGRP, &pgrp) < 0) {
        return -1;
    }
    return pgrp;
}

int tcsetpgrp(int fd, pid_t pgrp)
{
    return ioctl(fd, TIOCSPGRP, &pgrp);
}

char *getcwd(char *buf, size_t size)
{
    long ret = __syscall_ret(__syscall2(SYS_getcwd, (long)buf, size));
    return (ret < 0) ? NULL : buf;
}

int chdir(const char *path)
{
    return (int)__syscall_ret(__syscall1(SYS_chdir, (long)path));
}

int pipe(int pipefd[2])
{
    return (int)__syscall_ret(__syscall1(SYS_pipe, (long)pipefd));
}

int dup(int oldfd)
{
    return (int)__syscall_ret(__syscall1(SYS_dup, oldfd));
}

int dup2(int oldfd, int newfd)
{
    return (int)__syscall_ret(__syscall2(SYS_dup2, oldfd, newfd));
}

int access(const char *pathname, int mode)
{
    struct stat st;
    if (stat(pathname, &st) < 0) return -1;
    (void)mode;
    return 0;
}

int unlink(const char *pathname)
{
    return (int)__syscall_ret(__syscall1(SYS_unlink, (long)pathname));
}

int rmdir(const char *pathname)
{
    return (int)__syscall_ret(__syscall1(SYS_rmdir, (long)pathname));
}

int isatty(int fd)
{
    struct winsize ws;
    return (ioctl(fd, TIOCGWINSZ, &ws) == 0);
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    return (int)__syscall_ret(__syscall2(SYS_nanosleep, (long)req, (long)rem));
}

unsigned int sleep(unsigned int seconds)
{
    struct timespec req = { .tv_sec = seconds, .tv_nsec = 0 };
    nanosleep(&req, NULL);
    return 0;
}

int usleep(unsigned int usec)
{
    struct timespec req = { .tv_sec = usec / 1000000, .tv_nsec = (usec % 1000000) * 1000 };
    return nanosleep(&req, NULL);
}

int stat(const char *pathname, struct stat *statbuf)
{
    return (int)__syscall_ret(__syscall2(SYS_stat, (long)pathname, (long)statbuf));
}

int fstat(int fd, struct stat *statbuf)
{
    return (int)__syscall_ret(__syscall2(SYS_fstat, fd, (long)statbuf));
}

int lstat(const char *pathname, struct stat *statbuf)
{
    return stat(pathname, statbuf);
}

int mkdir(const char *pathname, mode_t mode)
{
    return (int)__syscall_ret(__syscall2(SYS_mkdir, (long)pathname, mode));
}

int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    return (int)__syscall_ret(__syscall3(SYS_mknod, (long)pathname, mode, dev));
}

int mount(const char *source, const char *target, const char *filesystemtype,
          unsigned long mountflags, const void *data)
{
    return (int)__syscall_ret(__syscall5(SYS_mount, (long)source, (long)target, (long)filesystemtype, mountflags, (long)data));
}

int chmod(const char *pathname, mode_t mode)
{
    return (int)__syscall_ret(__syscall2(SYS_chmod, (long)pathname, mode));
}

mode_t umask(mode_t mask)
{
    return (mode_t)__syscall_ret(__syscall1(SYS_umask, mask));
}

pid_t wait(int *wstatus)
{
    return wait4(-1, wstatus, 0, NULL);
}

pid_t waitpid(pid_t pid, int *wstatus, int options)
{
    return wait4(pid, wstatus, options, NULL);
}

pid_t wait4(pid_t pid, int *wstatus, int options, void *rusage)
{
    return (pid_t)__syscall_ret(__syscall4(SYS_wait4, pid, (long)wstatus, options, (long)rusage));
}

int ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    return (int)__syscall_ret(__syscall3(SYS_ioctl, fd, request, (long)arg));
}

int uname(struct utsname *buf)
{
    return (int)__syscall_ret(__syscall1(SYS_uname, (long)buf));
}

long syscall(long number, ...)
{
    va_list ap;
    va_start(ap, number);
    long a1 = va_arg(ap, long);
    long a2 = va_arg(ap, long);
    long a3 = va_arg(ap, long);
    long a4 = va_arg(ap, long);
    long a5 = va_arg(ap, long);
    long a6 = va_arg(ap, long);
    va_end(ap);
    return __syscall_ret(__syscall6(number, a1, a2, a3, a4, a5, a6));
}
