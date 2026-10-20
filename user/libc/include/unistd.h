#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

extern char **environ;

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);

pid_t fork(void);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
void _exit(int status) __attribute__((noreturn));

pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
gid_t getgid(void);
uid_t geteuid(void);
gid_t getegid(void);
int setuid(uid_t uid);
int setgid(gid_t gid);
int seteuid(uid_t euid);
int setegid(gid_t egid);
int setreuid(uid_t ruid, uid_t euid);
int setregid(gid_t rgid, gid_t egid);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgrp(void);
pid_t getpgid(pid_t pid);
pid_t setsid(void);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);

char *getcwd(char *buf, size_t size);
int chdir(const char *path);

int pipe(int pipefd[2]);
int dup(int oldfd);
int dup2(int oldfd, int newfd);

int access(const char *pathname, int mode);
int unlink(const char *pathname);
int rmdir(const char *pathname);
int isatty(int fd);

unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);

void *sbrk(intptr_t increment);
int brk(void *addr);

long syscall(long number, ...);
int init_module(void *module_image, unsigned long len, const char *param_values);
int delete_module(const char *name, unsigned int flags);

#endif /* _UNISTD_H */
