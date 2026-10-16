#ifndef _AURORA_SIGNAL_H
#define _AURORA_SIGNAL_H

#include <aurora/types.h>

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGWINCH  28
#define SIGSYS    31

#define NSIG      32

typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

/* WEXITSTATUS helper macros for wait4 */
#define WEXITSTATUS(status) (((status) >> 8) & 0xFF)
#define WIFEXITED(status)   (((status) & 0x7F) == 0)
#define WIFSIGNALED(status) (((signed char)(((status) & 0x7F) + 1) >> 1) > 0)
#define WTERMSIG(status)    ((status) & 0x7F)

#define WNOHANG 1

int send_signal(pid_t pid, int sig);

#endif /* _AURORA_SIGNAL_H */
