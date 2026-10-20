#ifndef _AURORA_SYSCALL_H
#define _AURORA_SYSCALL_H

#include <aurora/types.h>

/* Linux x86-64 syscall numbers */
#define SYS_read       0
#define SYS_write      1
#define SYS_open       2
#define SYS_close      3
#define SYS_stat       4
#define SYS_fstat      5
#define SYS_lseek      8
#define SYS_brk        12
#define SYS_ioctl      16
#define SYS_pipe       22
#define SYS_dup        32
#define SYS_dup2       33
#define SYS_nanosleep  35
#define SYS_getpid     39
#define SYS_socket     41
#define SYS_connect    42
#define SYS_accept     43
#define SYS_sendto     44
#define SYS_recvfrom   45
#define SYS_shutdown   48
#define SYS_bind       49
#define SYS_listen     50
#define SYS_getsockname 51
#define SYS_getpeername 52
#define SYS_socketpair 53
#define SYS_setsockopt 54
#define SYS_getsockopt 55
#define SYS_fork       57
#define SYS_execve     59
#define SYS_exit       60
#define SYS_wait4      61
#define SYS_kill       62
#define SYS_uname      63
#define SYS_getcwd     79
#define SYS_chdir      80
#define SYS_mkdir      83
#define SYS_rmdir      84
#define SYS_unlink     87
#define SYS_chmod      90
#define SYS_fchmod     91
#define SYS_umask      95
#define SYS_getuid     102
#define SYS_syslog     103
#define SYS_getgid     104
#define SYS_setuid     105
#define SYS_setgid     106
#define SYS_geteuid    107
#define SYS_getegid    108
#define SYS_setpgid    109
#define SYS_getppid    110
#define SYS_getpgrp    111
#define SYS_setsid     112
#define SYS_setreuid   113
#define SYS_fsync      74
#define SYS_setregid   114
#define SYS_getpgid    121
#define SYS_mknod      133
#define SYS_pivot_root 155
#define SYS_arch_prctl 158
#define SYS_sync       162
#define SYS_mount      165
#define SYS_umount2    166
#define SYS_init_module   175
#define SYS_delete_module 176
#define SYS_getdents64 217

struct stat;
struct utsname;
struct timespec;

void syscall_init(void);

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

/* Syscall implementations */
int64_t sys_open(const char *filename, int flags, mode_t mode);
int64_t sys_write(int fd, const void *buf, size_t count);
int64_t sys_read(int fd, void *buf, size_t count);
int64_t sys_close(int fd);
int64_t sys_lseek(int fd, uint64_t offset, int whence);
int64_t sys_dup(int oldfd);
int64_t sys_dup2(int oldfd, int newfd);
int64_t sys_stat(const char *filename, struct stat *statbuf);
int64_t sys_fstat(int fd, struct stat *statbuf);
int64_t sys_mkdir(const char *pathname, mode_t mode);
int64_t sys_mknod(const char *pathname, mode_t mode, dev_t dev);
int64_t sys_unlink(const char *pathname);
int64_t sys_exit(int status);
int64_t sys_getpid(void);
int64_t sys_getppid(void);
int64_t sys_getuid(void);
int64_t sys_getgid(void);
int64_t sys_geteuid(void);
int64_t sys_getegid(void);
int64_t sys_setuid(uid_t uid);
int64_t sys_setgid(gid_t gid);
int64_t sys_brk(uintptr_t new_brk);
int64_t sys_fork(void);
int64_t sys_execve(const char *filename, char *const argv[], char *const envp[]);
int64_t sys_wait4(pid_t pid, int *status, int options, void *rusage);
int64_t sys_kill(pid_t pid, int sig);
int64_t sys_pivot_root(const char *new_root, const char *put_old);
int64_t sys_mount(const char *dev_name, const char *dir_name, const char *type, unsigned long flags, const void *data);
int64_t sys_ioctl(int fd, unsigned long cmd, void *arg);
int64_t sys_getdents64(int fd, void *dirp, size_t count);
int64_t sys_chdir(const char *pathname);
int64_t sys_getcwd(char *buf, size_t size);
int64_t sys_uname(struct utsname *buf);
int64_t sys_nanosleep(const struct timespec *req, struct timespec *rem);
int64_t sys_pipe(int pipefd[2]);
int64_t sys_syslog(int type, char *buf, int len);
int64_t sys_setpgid(pid_t pid, pid_t pgid);
int64_t sys_getpgrp(void);
int64_t sys_setsid(void);
int64_t sys_getpgid(pid_t pid);
int64_t sys_umask(mode_t mask);
int64_t sys_setreuid(uid_t ruid, uid_t euid);
int64_t sys_setregid(gid_t rgid, gid_t egid);
int64_t sys_chmod(const char *pathname, mode_t mode);
int64_t sys_fchmod(int fd, mode_t mode);
int64_t sys_sync(void);
int64_t sys_fsync(int fd);
int64_t sys_arch_prctl(int code, unsigned long addr);
int64_t sys_init_module(void *umod, unsigned long len, const char *uargs);
int64_t sys_delete_module(const char *name, unsigned int flags);

#endif /* _AURORA_SYSCALL_H */
