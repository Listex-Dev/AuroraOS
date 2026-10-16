#include <aurora/proc/m4_test.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern char user_blob_start[];
extern char user_blob_end[];
extern void enter_usermode(uint64_t rip, uint64_t rsp);

#define USER_CODE_VIRT  0x0000000000400000ULL
#define USER_STACK_VIRT 0x00007FFFF0000000ULL

static void user_thread_entry(void)
{
    /* Transition to Ring 3 (User Space) */
    uint64_t user_rsp = USER_STACK_VIRT + PAGE_SIZE - 16;
    enter_usermode(USER_CODE_VIRT, user_rsp);
}

bool test_usermode_syscall(void)
{
    printk("<6>[  0.000000] Running Milestone M4 verification test (Ring 3 transition & syscalls)...\n");

    uint64_t *pml4 = vmm_get_kernel_pml4();

    /* 1. Allocate physical pages for user code and user stack */
    uintptr_t code_phys = pmm_alloc_page();
    uintptr_t stack_phys = pmm_alloc_page();

    if (!code_phys || !stack_phys) {
        printk("<0>[TEST M4] FAILED: Could not allocate physical pages for user process!\n");
        return false;
    }

    /* 2. Map pages with User permissions (PTE_USER | PTE_WRITABLE) */
    if (vmm_map_page(pml4, USER_CODE_VIRT, code_phys, PTE_USER | PTE_WRITABLE) != 0 ||
        vmm_map_page(pml4, USER_STACK_VIRT, stack_phys, PTE_USER | PTE_WRITABLE) != 0) {
        printk("<0>[TEST M4] FAILED: Could not map user virtual pages!\n");
        return false;
    }

    /* 3. Copy user payload into code page */
    size_t code_len = user_blob_end - user_blob_start;
    void *code_dest = vmm_phys_to_virt(code_phys);
    memset(code_dest, 0, PAGE_SIZE);
    memcpy(code_dest, user_blob_start, code_len);

    /* Zero out user stack page */
    memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);

    /* 4. Create task for user process */
    task_t *utask = task_create("user_init", user_thread_entry);
    if (!utask) {
        printk("<0>[TEST M4] FAILED: Could not create user task!\n");
        return false;
    }

    utask->brk_start = 0x0000000000600000ULL;
    utask->brk_current = utask->brk_start;

    sched_add_task(utask);

    printk("<6>[TEST M4] Spawned Ring 3 task '%s' (PID %d). Yielding CPU to user task...\n",
           utask->comm, utask->pid);

    /* Yield until user process calls sys_exit(42) */
    while (utask->state != TASK_ZOMBIE) {
        sched_yield();
    }

    /* Clean up mapped pages */
    vmm_unmap_page(pml4, USER_CODE_VIRT);
    vmm_unmap_page(pml4, USER_STACK_VIRT);
    pmm_free_page(code_phys);
    pmm_free_page(stack_phys);

    if (utask->exit_code == 42) {
        printk("<6>[TEST M4] User process exited cleanly with expected status code %d!\n",
               utask->exit_code);
        printk("<6>[  0.000000] Milestone M4 status: [OK] Fast syscall gate, write, exit, getpid, brk verified.\n");
        task_destroy(utask);
        return true;
    } else {
        printk("<0>[TEST M4] FAILED: User process exit code was %d (expected 42)!\n",
               utask->exit_code);
        task_destroy(utask);
        return false;
    }
}
