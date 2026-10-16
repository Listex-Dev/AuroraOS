#ifndef _AURORA_PROC_SCHED_H
#define _AURORA_PROC_SCHED_H

#include <aurora/proc/task.h>

void sched_init(void);
void sched_add_task(task_t *task);
void sched_remove_task(task_t *task);
void sched_yield(void);
void sched_timer_tick(void);
task_t *sched_current(void);
task_t *sched_find_task(pid_t pid);
task_t *sched_find_child(pid_t ppid, pid_t pid);
void sched_exit(int status);
size_t sched_num_tasks(void);
task_t *sched_get_task_at(size_t index);
pid_t sched_allocate_pid(void);

/* Assembly context switch function in context.S */
void cpu_switch_to(task_t *prev, task_t *next);

#endif /* _AURORA_PROC_SCHED_H */
