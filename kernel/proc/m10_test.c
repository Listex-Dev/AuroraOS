#include <aurora/proc/m10_test.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/proc/binaries.h>
#include <aurora/proc/stack.h>
#include <aurora/elf64.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/fs/vfs.h>
#include <aurora/syscall.h>
#include <aurora/arch/x86_64/cr.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern void enter_usermode(uint64_t rip, uint64_t rsp);

static uint64_t g_m10_entry_point = 0;
static uint64_t g_m10_user_sp = 0;

static void user_m10_trampoline(void)
{
    enter_usermode(g_m10_entry_point, g_m10_user_sp);
}

bool test_m10_libc_and_user_utils(void)
{
    printk("<6>[  0.000000] Running Milestone M10 verification test (libc full, crt0, user utilities: ls, cat, cp, ps, vi, grep)...\n");

    /* Ensure /tmp and /bin exist */
    sys_mkdir("/tmp", 0777);
    sys_mkdir("/bin", 0755);

    /* 1. Create a dedicated PML4 address space for the test runner */
    uintptr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        printk("<0>[TEST M10] FAILED: Could not allocate PML4 page!\n");
        return false;
    }
    uint64_t *user_pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);
    memset(user_pml4, 0, PAGE_SIZE);

    /* Share upper half kernel mappings */
    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    /* 2. Locate /bin/test_m10 binary */
    size_t bin_size = 0;
    const void *elf_data = find_builtin_binary("/bin/test_m10", &bin_size);
    if (!elf_data) {
        printk("<0>[TEST M10] FAILED: /bin/test_m10 not found in builtin binaries!\n");
        return false;
    }

    /* 3. Load ELF binary into user PML4 */
    uint64_t entry_point = 0;
    uint64_t brk_start = 0;
    int ret = elf64_load_pml4(user_pml4, elf_data, bin_size, &entry_point, &brk_start);
    if (ret < 0) {
        printk("<0>[TEST M10] FAILED: Could not load /bin/test_m10 ELF: %d\n", ret);
        return false;
    }

    /* 4. Allocate and map user stack (16 KiB = 4 pages) */
    #define USER_STACK_TOP 0x00007FFFF0000000ULL
    #define USER_STACK_PAGES 4
    for (uint64_t p = 0; p < USER_STACK_PAGES; p++) {
        uintptr_t stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            printk("<0>[TEST M10] FAILED: Could not allocate user stack page!\n");
            return false;
        }
        memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);
        vmm_map_page(user_pml4,
                     USER_STACK_TOP - (p + 1) * PAGE_SIZE,
                     stack_phys,
                     PTE_USER | PTE_WRITABLE);
    }

    /* 5. Setup user stack with argc/argv/envp according to System V AMD64 ABI */
    char *argv[] = { "/bin/test_m10", NULL };
    char *envp[] = { "PATH=/bin:/usr/bin", "USER=root", NULL };
    uint64_t orig_cr3 = read_cr3();
    write_cr3(pml4_phys);
    uint64_t user_sp = setup_user_stack(USER_STACK_TOP, 1, argv, envp, entry_point);
    write_cr3(orig_cr3);

    g_m10_entry_point = entry_point;
    g_m10_user_sp = user_sp;

    /* 6. Create user task */
    task_t *task = task_create("test_m10", user_m10_trampoline);
    if (!task) {
        printk("<0>[TEST M10] FAILED: Could not create test_m10 task!\n");
        return false;
    }

    task->pml4 = user_pml4;
    task->cr3 = pml4_phys;
    task->brk_start = brk_start;
    task->brk_current = brk_start;

    sched_add_task(task);
    printk("<6>[TEST M10] Spawned Ring 3 test task '%s' (PID %d). Executing full userland test suite...\n",
           task->comm, task->pid);

    /* 7. Yield CPU until test process completes */
    while (task->state != TASK_ZOMBIE) {
        sched_yield();
    }

    if (task->exit_code != 100) {
        printk("<0>[TEST M10] FAILED: Test task exited with status %d (expected 100)!\n",
               task->exit_code);
        sched_remove_task(task);
        vmm_destroy_user_space(task->pml4);
        task_destroy(task);
        return false;
    }

    /* 8. Success: clean up task and celebrate */
    sched_remove_task(task);
    vmm_destroy_user_space(task->pml4);
    task_destroy(task);

    printk("<6>[  0.000000] Milestone M10 status: [OK] libc full, crt0, and user utilities (ls, cat, cp, ps, vi, grep) verified.\n");
    return true;
}
