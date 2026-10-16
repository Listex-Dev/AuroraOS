#include <aurora/syscall.h>
#include <aurora/proc/sched.h>
#include <aurora/printk.h>

int64_t sys_getpid(void)
{
    task_t *curr = sched_current();
    return curr ? curr->pid : 0;
}

int64_t sys_exit(int status)
{
    task_t *curr = sched_current();
    if (curr) {
        printk("<6>[SYSCALL] Process %s (PID %d) exited with status %d\n",
               curr->comm, curr->pid, status);
    }
    sched_exit(status);
    return 0;
}
