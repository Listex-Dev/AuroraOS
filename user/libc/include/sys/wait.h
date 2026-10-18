#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG    1
#define WUNTRACED  2
#define WCONTINUED 8

#define WIFEXITED(status)    (((status) & 0x7f) == 0)
#define WEXITSTATUS(status)  (((status) & 0xff00) >> 8)
#define WIFSIGNALED(status)  (((signed char)(((status) & 0x7f) + 1) >> 1) > 0)
#define WTERMSIG(status)     ((status) & 0x7f)
#define WIFSTOPPED(status)   (((status) & 0xff) == 0x7f)
#define WSTOPSIG(status)     WEXITSTATUS(status)

pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
pid_t wait4(pid_t pid, int *wstatus, int options, void *rusage);

#endif /* _SYS_WAIT_H */
