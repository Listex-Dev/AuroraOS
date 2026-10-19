#include <aurora/proc/sched.h>
#include <aurora/printk.h>
#include <aurora/io.h>
#include <aurora/string.h>
#include <aurora/panic.h>
#include <aurora/arch/x86_64/cr.h>
#include <aurora/arch/x86_64/tss.h>
#include <aurora/arch/x86_64/msr.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>

#define MAX_TASKS 64

static task_t *runqueue[MAX_TASKS];
static size_t num_tasks = 0;
static size_t current_idx = 0;

static task_t kernel_idle_task;
static task_t *g_current_task = NULL;
static pid_t g_next_pid = 1;

pid_t sched_allocate_pid(void)
{
    return g_next_pid++;
}

size_t sched_num_tasks(void)
{
    return num_tasks;
}

task_t *sched_get_task_at(size_t index)
{
    if (index < num_tasks) {
        return runqueue[index];
    }
    return NULL;
}

void sched_init(void)
{
    memset(&kernel_idle_task, 0, sizeof(kernel_idle_task));
    kernel_idle_task.kstack = kmalloc(KERNEL_STACK_SIZE, GFP_KERNEL);
    kernel_idle_task.pid = 0;
    kernel_idle_task.state = TASK_RUNNING;
    kernel_idle_task.time_slice = DEFAULT_TIME_SLICE;
    strncpy(kernel_idle_task.comm, "swapper/0", sizeof(kernel_idle_task.comm) - 1);

    if (kernel_idle_task.kstack) {
        tss_set_rsp0((uint64_t)kernel_idle_task.kstack + KERNEL_STACK_SIZE);
    }

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

void sched_remove_task(task_t *task)
{
    for (size_t i = 0; i < num_tasks; i++) {
        if (runqueue[i] == task) {
            for (size_t j = i; j < num_tasks - 1; j++) {
                runqueue[j] = runqueue[j + 1];
            }
            num_tasks--;
            if (current_idx >= num_tasks && num_tasks > 0) {
                current_idx = 0;
            }
            break;
        }
    }
}

task_t *sched_current(void)
{
    return g_current_task;
}

task_t *sched_find_task(pid_t pid)
{
    for (size_t i = 0; i < num_tasks; i++) {
        if (runqueue[i] && runqueue[i]->pid == pid) {
            return runqueue[i];
        }
    }
    return NULL;
}

task_t *sched_find_child(pid_t ppid, pid_t pid)
{
    for (size_t i = 0; i < num_tasks; i++) {
        task_t *t = runqueue[i];
        if (t && t->ppid == ppid) {
            if (pid <= 0 || t->pid == pid) {
                return t;
            }
        }
    }
    return NULL;
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

    /* Switch CR3 address space if task has a distinct PML4 */
    if (next->cr3 && next->cr3 != read_cr3()) {
        write_cr3(next->cr3);
    } else if (!next->cr3 && read_cr3() != vmm_virt_to_phys_direct(vmm_get_kernel_pml4())) {
        write_cr3(vmm_virt_to_phys_direct(vmm_get_kernel_pml4()));
    }

    /* Save current task FS base and restore next task's FS base */
    if (prev) {
        prev->fs_base = rdmsr(MSR_FS_BASE);
    }
    wrmsr(MSR_FS_BASE, next->fs_base);

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

        if (g_current_task->pid == 1) {
            panic("Attempted to kill init!");
        }

        /* Re-parent children of the exiting task to init (PID 1) */
        for (size_t i = 0; i < num_tasks; i++) {
            if (runqueue[i] && runqueue[i]->ppid == g_current_task->pid) {
                runqueue[i]->ppid = 1;
            }
        }
    }

    sched_yield();

    /* If no other tasks left, halt */
    while (1) {
        cli();
        hlt();
    }
}
