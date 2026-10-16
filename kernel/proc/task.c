#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>

extern void task_trampoline(void);

task_t *task_create(const char *name, void (*entry_point)(void))
{
    task_t *task = (task_t *)kzalloc(sizeof(task_t), GFP_KERNEL);
    if (!task) {
        return NULL;
    }

    task->kstack = kmalloc(KERNEL_STACK_SIZE, GFP_KERNEL);
    if (!task->kstack) {
        kfree(task);
        return NULL;
    }

    task->pid = sched_allocate_pid();
    task->state = TASK_RUNNING;
    task->time_slice = DEFAULT_TIME_SLICE;
    strncpy(task->comm, name ? name : "kthread", sizeof(task->comm) - 1);

    /* Setup initial stack for cpu_switch_to */
    uint64_t *stack_top = (uint64_t *)((uintptr_t)task->kstack + KERNEL_STACK_SIZE);
    stack_top = (uint64_t *)((uintptr_t)stack_top & ~0xFULL);

    /* Push entry function address for task_trampoline to pop */
    *(--stack_top) = (uint64_t)entry_point;

    /* Return address for cpu_switch_to: task_trampoline */
    *(--stack_top) = (uint64_t)task_trampoline;

    /* Callee-saved registers popped by cpu_switch_to */
    *(--stack_top) = 0; /* r15 */
    *(--stack_top) = 0; /* r14 */
    *(--stack_top) = 0; /* r13 */
    *(--stack_top) = 0; /* r12 */
    *(--stack_top) = 0; /* rbp */
    *(--stack_top) = 0; /* rbx */

    task->rsp = (uint64_t)stack_top;

    return task;
}

void task_destroy(task_t *task)
{
    if (!task) {
        return;
    }

    if (task->kstack) {
        kfree(task->kstack);
    }
    kfree(task);
}
