/*
 * AuroraOS Milestone M17 Package Management & Documentation Verification
 */

#include <aurora/proc/m17_test.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/proc/binaries.h>
#include <aurora/proc/stack.h>
#include <aurora/elf64.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/arch/x86_64/cr.h>
#include <aurora/printk.h>
#include <aurora/string.h>
#include <aurora/fs/docs.h>

extern void enter_usermode(uint64_t rip, uint64_t rsp);

static uint64_t g_m17_entry_point = 0;
static uint64_t g_m17_user_sp = 0;

static void user_m17_trampoline(void)
{
    enter_usermode(g_m17_entry_point, g_m17_user_sp);
}

bool test_m17_package_and_docs_subsystem(void)
{
    printk("<6>[  0.000000] Running Milestone M17 verification test (pkg package manager, cryptographic signature verification, man pages)...\n");

    /* 1. Ensure docs and package repository structure is populated */
    docs_and_pkg_subsys_init();

    /* 2. Create dedicated PML4 for /bin/test_m17 */
    uintptr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        printk("<0>[TEST M17] FAILED: Could not allocate PML4 page!\n");
        return false;
    }
    uint64_t *user_pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);
    memset(user_pml4, 0, PAGE_SIZE);

    /* Share upper half kernel mappings */
    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    /* 3. Locate and load /bin/test_m17 binary */
    size_t bin_size = 0;
    const void *elf_data = find_builtin_binary("/bin/test_m17", &bin_size);
    if (!elf_data) {
        printk("<0>[TEST M17] FAILED: /bin/test_m17 not found in builtin binaries!\n");
        return false;
    }

    uint64_t entry_point = 0;
    uint64_t brk_start = 0;
    int ret = elf64_load_pml4(user_pml4, elf_data, bin_size, &entry_point, &brk_start);
    if (ret < 0) {
        printk("<0>[TEST M17] FAILED: Could not load /bin/test_m17 ELF: %d\n", ret);
        return false;
    }

    /* 4. Allocate and map user stack (16 KiB) with NX */
    #define M17_STACK_TOP 0x00007FFFF0000000ULL
    #define M17_STACK_PAGES 4
    for (uint64_t p = 0; p < M17_STACK_PAGES; p++) {
        uintptr_t stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            printk("<0>[TEST M17] FAILED: Could not allocate user stack page!\n");
            return false;
        }
        memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);
        vmm_map_page(user_pml4,
                     M17_STACK_TOP - (p + 1) * PAGE_SIZE,
                     stack_phys,
                     PTE_USER | PTE_WRITABLE | PTE_NX);
    }

    /* 5. Setup user stack with argc/argv/envp */
    char *argv[] = { "/bin/test_m17", NULL };
    char *envp[] = { "PATH=/bin:/sbin:/usr/bin", "USER=root", "HOME=/root", "TERM=linux", NULL };
    uint64_t orig_cr3 = read_cr3();
    write_cr3(pml4_phys);
    uint64_t user_sp = setup_user_stack(M17_STACK_TOP, 1, argv, envp, entry_point);
    write_cr3(orig_cr3);

    g_m17_entry_point = entry_point;
    g_m17_user_sp = user_sp;

    /* 6. Create user task */
    task_t *task = task_create("test_m17", user_m17_trampoline);
    if (!task) {
        printk("<0>[TEST M17] FAILED: Could not create test_m17 task!\n");
        return false;
    }

    task->pml4 = user_pml4;
    task->cr3 = pml4_phys;
    task->brk_start = brk_start;
    task->brk_current = brk_start;
    task->uid = 0;
    task->euid = 0;
    task->suid = 0;
    task->gid = 0;
    task->egid = 0;
    task->sgid = 0;
    task->umask = 0022;

    sched_add_task(task);
    printk("<6>[INFO]  [TEST M17] Spawned Ring 3 test task '%s' (PID %d). Executing M17 test suite...\n",
           task->comm, task->pid);

    /* 7. Yield CPU until test process completes */
    while (task->state != TASK_ZOMBIE) {
        sched_yield();
    }

    if (task->exit_code != 0) {
        printk("<0>[TEST M17] FAILED: Test task exited with status %d (expected 0)!\n",
               task->exit_code);
        sched_remove_task(task);
        vmm_destroy_user_space(task->pml4);
        task_destroy(task);
        return false;
    }

    /* 8. Success: clean up task */
    sched_remove_task(task);
    vmm_destroy_user_space(task->pml4);
    task_destroy(task);

    printk("<6>[  0.000000] Milestone M17 status: [OK] Package management (pkg, signature check, /var/lib/pkg/db) and manual pages (man1, man5, man8) verified.\n");
    return true;
}
