#include <signal.h>
#include <unistd.h>
#include "syscall.h"

int kill(pid_t pid, int sig)
{
    return (int)__syscall_ret(__syscall2(SYS_kill, pid, sig));
}

int raise(int sig)
{
    return kill(getpid(), sig);
}

sighandler_t signal(int signum, sighandler_t handler)
{
    (void)signum; (void)handler;
    return SIG_DFL;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    (void)signum; (void)act; (void)oldact;
    return 0;
}
