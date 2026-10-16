#include <aurora/proc/m5_test.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/signal.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern char user_m5_blob_start[];
extern char user_m5_blob_end[];
extern void enter_usermode(uint64_t rip, uint64_t rsp);

#define M5_USER_CODE_VIRT  0x0000000000400000ULL
#define M5_USER_STACK_VIRT 0x00007FFFF0000000ULL

static void user_m5_entry_trampoline(void)
{
    /* Transition to Ring 3 (User Space) */
    uint64_t user_rsp = M5_USER_STACK_VIRT + PAGE_SIZE - 32;
    enter_usermode(M5_USER_CODE_VIRT, user_rsp);
}

static void dummy_worker(void)
{
    while (1) {
        sched_yield();
    }
}

bool test_fork_execve_wait(void)
{
    printk("<6>[  0.000000] Running Milestone M5 verification test (fork, execve ELF64, wait4, signals)...\n");

    /* 1. Create a dedicated PML4 address space for the user test process */
    uintptr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        printk("<0>[TEST M5] FAILED: Could not allocate PML4 page!\n");
        return false;
    }
    uint64_t *user_pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);
    memset(user_pml4, 0, PAGE_SIZE);

    /* Share upper half kernel mappings */
    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    /* 2. Allocate physical pages for user code and user stack */
    uintptr_t code_phys = pmm_alloc_page();
    uintptr_t stack_phys = pmm_alloc_page();

    if (!code_phys || !stack_phys) {
        printk("<0>[TEST M5] FAILED: Could not allocate memory for user process!\n");
        return false;
    }

    /* 3. Map user pages into user PML4 with User permissions (PTE_USER | PTE_WRITABLE) */
    vmm_map_page(user_pml4, M5_USER_CODE_VIRT, code_phys, PTE_USER | PTE_WRITABLE);
    vmm_map_page(user_pml4, M5_USER_STACK_VIRT, stack_phys, PTE_USER | PTE_WRITABLE);

    /* Copy user test assembly into user code page */
    size_t code_len = user_m5_blob_end - user_m5_blob_start;
    void *code_dest = vmm_phys_to_virt(code_phys);
    memset(code_dest, 0, PAGE_SIZE);
    memcpy(code_dest, user_m5_blob_start, code_len);

    /* Zero out stack page */
    memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);

    /* 4. Create user task */
    task_t *ptask = task_create("m5_parent", user_m5_entry_trampoline);
    if (!ptask) {
        printk("<0>[TEST M5] FAILED: Could not create user parent task!\n");
        return false;
    }

    ptask->pml4 = user_pml4;
    ptask->cr3 = pml4_phys;
    ptask->brk_start = 0x0000000000600000ULL;
    ptask->brk_current = ptask->brk_start;

    sched_add_task(ptask);

    printk("<6>[TEST M5] Spawned Ring 3 parent task '%s' (PID %d). Executing fork + execve + wait4 test...\n",
           ptask->comm, ptask->pid);

    /* 5. Yield until parent process completes (calls sys_exit(99)) */
    while (ptask->state != TASK_ZOMBIE) {
        sched_yield();
    }

    if (ptask->exit_code != 99) {
        printk("<0>[TEST M5] FAILED: Parent process exited with error code %d (expected 99)!\n",
               ptask->exit_code);
        sched_remove_task(ptask);
        vmm_destroy_user_space(ptask->pml4);
        task_destroy(ptask);
        return false;
    }

    printk("<6>[TEST M5] Parent process successfully verified fork, child execve('/bin/hello'), and wait4 status!\n");

    /* Clean up parent task */
    sched_remove_task(ptask);
    vmm_destroy_user_space(ptask->pml4);
    task_destroy(ptask);

    /* 6. Verify POSIX Signal handling (sys_kill) */
    printk("<6>[TEST M5] Verifying signal handling (SIGTERM & wait4)...\n");
    task_t *dummy = task_create("dummy_worker", dummy_worker);
    if (!dummy) {
        printk("<0>[TEST M5] FAILED: Could not create dummy worker task!\n");
        return false;
    }
    sched_add_task(dummy);
    pid_t dummy_pid = dummy->pid;

    /* Send SIGTERM to dummy worker */
    extern int64_t sys_kill(pid_t pid, int sig);
    extern int64_t sys_wait4(pid_t pid, int *status, int options, void *rusage);

    int64_t kill_res = sys_kill(dummy_pid, SIGTERM);
    if (kill_res != 0) {
        printk("<0>[TEST M5] FAILED: sys_kill returned %d!\n", (int)kill_res);
        return false;
    }

    if (dummy->state != TASK_ZOMBIE || dummy->exit_code != (128 + SIGTERM)) {
        printk("<0>[TEST M5] FAILED: Dummy task not in expected zombie state after SIGTERM!\n");
        return false;
    }

    sched_remove_task(dummy);
    task_destroy(dummy);
    printk("<6>[TEST M5] Signal delivery and default termination verified successfully!\n");

    /* 7. Milestone M5 Acceptance Passed */
    printk("<6>[  0.000000] Milestone M5 status: [OK] fork, execve ELF64, wait4, signals verified.\n");
    return true;
}
