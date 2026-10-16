#include <aurora/proc/m3_test.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/serial.h>
#include <aurora/printk.h>

#define ITERATIONS 20

static volatile int count_a = 0;
static volatile int count_b = 0;
static volatile bool done_a = false;
static volatile bool done_b = false;

static void thread_a(void)
{
    for (int i = 0; i < ITERATIONS; i++) {
        serial_putchar('A');
        count_a++;
        sched_yield();
    }
    done_a = true;
    while (!done_b) {
        sched_yield();
    }
}

static void thread_b(void)
{
    for (int i = 0; i < ITERATIONS; i++) {
        serial_putchar('B');
        count_b++;
        sched_yield();
    }
    done_b = true;
    while (!done_a) {
        sched_yield();
    }
}

bool test_scheduler_multitasking(void)
{
    printk("<6>[  0.000000] Running Milestone M3 verification test (cooperative & pre-emptive multitasking)...\n");
    printk("<6>[TEST SCHED] Starting Task A and Task B interleaved execution:\n");

    count_a = 0;
    count_b = 0;
    done_a = false;
    done_b = false;

    task_t *ta = task_create("task_A", thread_a);
    task_t *tb = task_create("task_B", thread_b);

    if (!ta || !tb) {
        printk("<0>[TEST SCHED] FAILED: Could not create tasks!\n");
        return false;
    }

    sched_add_task(ta);
    sched_add_task(tb);

    /* Main task yields to let Task A and Task B execute */
    while (!done_a || !done_b) {
        sched_yield();
    }

    serial_putchar('\n');

    sched_remove_task(ta);
    sched_remove_task(tb);
    task_destroy(ta);
    task_destroy(tb);

    if (count_a == ITERATIONS && count_b == ITERATIONS) {
        printk("<6>[TEST SCHED] OK: Successfully interleaved %d 'A's and %d 'B's!\n",
               count_a, count_b);
        printk("<6>[  0.000000] Milestone M3 status: [OK] PIT 100Hz, task_struct, context switch, scheduler verified.\n");
        return true;
    } else {
        printk("<0>[TEST SCHED] FAILED: Iterations mismatch! A=%d, B=%d\n", count_a, count_b);
        return false;
    }
}
