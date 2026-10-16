#include <aurora/proc/sched.h>
#include <aurora/printk.h>
#include <aurora/io.h>
#include <aurora/string.h>

#define MAX_TASKS 64

static task_t *runqueue[MAX_TASKS];
static size_t num_tasks = 0;
static size_t current_idx = 0;

static task_t kernel_idle_task;
static task_t *g_current_task = NULL;

void sched_init(void)
{
    memset(&kernel_idle_task, 0, sizeof(kernel_idle_task));
    kernel_idle_task.pid = 0;
    kernel_idle_task.state = TASK_RUNNING;
    kernel_idle_task.time_slice = DEFAULT_TIME_SLICE;
    strncpy(kernel_idle_task.comm, "swapper/0", sizeof(kernel_idle_task.comm) - 1);

    g_current_task = &kernel_idle_task;
    runqueue[0] = &kernel_idle_task;
    num_tasks = 1;
    current_idx = 0;

    printk("<6>[  0.000000] Sched: Round-Robin scheduler initialized (quantum %d ticks)\n",
           DEFAULT_TIME_SLICE);
}

void sched_add_task(task_t *task)
{
    if (num_tasks < MAX_TASKS && task != NULL) {
        runqueue[num_tasks++] = task;
    }
}

task_t *sched_current(void)
{
    return g_current_task;
}

void sched_yield(void)
{
    if (num_tasks <= 1) {
        return;
    }

    size_t next_idx = (current_idx + 1) % num_tasks;
    size_t inspected = 0;

    while (inspected < num_tasks) {
        task_t *cand = runqueue[next_idx];
        if (cand && cand->state == TASK_RUNNING) {
            break;
        }
        next_idx = (next_idx + 1) % num_tasks;
        inspected++;
    }

    if (next_idx == current_idx) {
        return; /* No other runnable task */
    }

    task_t *prev = g_current_task;
    task_t *next = runqueue[next_idx];

    current_idx = next_idx;
    g_current_task = next;

    cpu_switch_to(prev, next);
}

void sched_timer_tick(void)
{
    if (!g_current_task) {
        return;
    }

    g_current_task->time_slice--;
    if (g_current_task->time_slice <= 0) {
        g_current_task->time_slice = DEFAULT_TIME_SLICE;
        sched_yield();
    }
}

void sched_exit(int status)
{
    if (g_current_task) {
        g_current_task->exit_code = status;
        g_current_task->state = TASK_ZOMBIE;
    }

    sched_yield();

    /* If no other tasks left, halt */
    while (1) {
        cli();
        hlt();
    }
}
