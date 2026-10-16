#ifndef _AURORA_PROC_SCHED_H
#define _AURORA_PROC_SCHED_H

#include <aurora/proc/task.h>

void sched_init(void);
void sched_add_task(task_t *task);
void sched_yield(void);
void sched_timer_tick(void);
task_t *sched_current(void);
void sched_exit(int status);

/* Assembly context switch function in context.S */
void cpu_switch_to(task_t *prev, task_t *next);

#endif /* _AURORA_PROC_SCHED_H */
