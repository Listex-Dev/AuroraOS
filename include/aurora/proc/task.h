#ifndef _AURORA_PROC_TASK_H
#define _AURORA_PROC_TASK_H

#include <aurora/types.h>

#define TASK_RUNNING         0
#define TASK_INTERRUPTIBLE   1
#define TASK_UNINTERRUPTIBLE 2
#define TASK_STOPPED         3
#define TASK_ZOMBIE          4
#define TASK_DEAD            5

#define KERNEL_STACK_SIZE    16384 /* 16 KiB kernel stack per task */
#define DEFAULT_TIME_SLICE   5     /* 5 ticks = 50 ms */

struct pt_regs {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t orig_rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct task_struct {
    /* Context switch: rsp must be at known offset */
    uint64_t rsp;
    void *kstack;

    pid_t pid;
    pid_t ppid;
    pid_t pgid;
    pid_t sid;
    pid_t tgid;

    uid_t uid, euid, suid;
    gid_t gid, egid, sgid;

    int state;

    void *mm;
    uint64_t cr3;
    uint64_t *pml4;
    struct pt_regs *regs;
    void *files;
    void *fs;
    void *sig;

    struct task_struct *parent;
    struct task_struct *children;
    struct task_struct *sibling;
    struct task_struct *next; /* Scheduler runqueue link */

    void *tty;

    uint64_t utime;
    uint64_t stime;
    int nice;

    uint64_t brk_start;
    uint64_t brk_current;

    int time_slice;
    int exit_code;
    char comm[16];
};

typedef struct task_struct task_t;

task_t *task_create(const char *name, void (*entry_point)(void));
void task_destroy(task_t *task);

#endif /* _AURORA_PROC_TASK_H */
