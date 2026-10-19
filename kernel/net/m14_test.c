#include <aurora/net/m14_test.h>
#include <aurora/net/netdev.h>
#include <aurora/net/arp.h>
#include <aurora/net/icmp.h>
#include <aurora/net/udp.h>
#include <aurora/net/tcp.h>
#include <aurora/net/socket.h>
#include <aurora/net/dhcp.h>
#include <aurora/drivers/e1000.h>
#include <aurora/drivers/pci.h>
#include <aurora/drivers/pty.h>
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

extern void enter_usermode(uint64_t rip, uint64_t rsp);

static uint64_t g_m14_entry_point = 0;
static uint64_t g_m14_user_sp = 0;

static void user_m14_trampoline(void)
{
    enter_usermode(g_m14_entry_point, g_m14_user_sp);
}

bool test_m14_services_and_pty_subsystem(void)
{
    printk("<6>[  0.000000] Running Milestone M14 verification test (DHCP, DNS, ifconfig, netstat, wget, pty)...\n");

    /* 1. Ensure PTY and network subsystems are active */
    pty_subsys_init();

    struct netdev *eth0 = netdev_get_by_name("eth0");
    if (!eth0) {
        printk("<4>[TEST M14] eth0 not yet registered, initializing e1000 driver...\n");
        e1000_init();
        eth0 = netdev_get_by_name("eth0");
    }

    /* 2. Configure network via DHCP Client */
    struct dhcp_lease lease;
    memset(&lease, 0, sizeof(lease));
    int dhcp_res = dhcp_configure_interface(eth0, &lease);
    if (dhcp_res == 0) {
        printk("<6>[TEST M14] OK: DHCP configuration complete for eth0 (IP %u.%u.%u.%u)\n",
               (net_ntohl(lease.ip) >> 24) & 0xFF, (net_ntohl(lease.ip) >> 16) & 0xFF,
               (net_ntohl(lease.ip) >> 8) & 0xFF, net_ntohl(lease.ip) & 0xFF);
    }

    /* 3. Create dedicated PML4 for /bin/test_m14 */
    uintptr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        printk("<0>[TEST M14] FAILED: Could not allocate PML4 page!\n");
        return false;
    }
    uint64_t *user_pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);
    memset(user_pml4, 0, PAGE_SIZE);

    /* Share upper half kernel mappings */
    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    /* 4. Locate and load /bin/test_m14 binary */
    size_t bin_size = 0;
    const void *elf_data = find_builtin_binary("/bin/test_m14", &bin_size);
    if (!elf_data) {
        printk("<0>[TEST M14] FAILED: /bin/test_m14 not found in builtin binaries!\n");
        return false;
    }

    uint64_t entry_point = 0;
    uint64_t brk_start = 0;
    int ret = elf64_load_pml4(user_pml4, elf_data, bin_size, &entry_point, &brk_start);
    if (ret < 0) {
        printk("<0>[TEST M14] FAILED: Could not load /bin/test_m14 ELF: %d\n", ret);
        return false;
    }

    /* 5. Allocate and map user stack (16 KiB) */
    #define M14_STACK_TOP 0x00007FFFF0000000ULL
    #define M14_STACK_PAGES 4
    for (uint64_t p = 0; p < M14_STACK_PAGES; p++) {
        uintptr_t stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            printk("<0>[TEST M14] FAILED: Could not allocate user stack page!\n");
            return false;
        }
        memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);
        vmm_map_page(user_pml4,
                     M14_STACK_TOP - (p + 1) * PAGE_SIZE,
                     stack_phys,
                     PTE_USER | PTE_WRITABLE);
    }

    /* 6. Setup user stack with argc/argv/envp */
    char *argv[] = { "/bin/test_m14", NULL };
    char *envp[] = { "PATH=/bin:/sbin", "USER=root", "HOME=/root", "TERM=linux", NULL };
    uint64_t orig_cr3 = read_cr3();
    write_cr3(pml4_phys);
    uint64_t user_sp = setup_user_stack(M14_STACK_TOP, 1, argv, envp, entry_point);
    write_cr3(orig_cr3);

    g_m14_entry_point = entry_point;
    g_m14_user_sp = user_sp;

    /* 7. Create user task */
    task_t *task = task_create("test_m14", user_m14_trampoline);
    if (!task) {
        printk("<0>[TEST M14] FAILED: Could not create test_m14 task!\n");
        return false;
    }

    task->pml4 = user_pml4;
    task->cr3 = pml4_phys;
    task->brk_start = brk_start;
    task->brk_current = brk_start;

    sched_add_task(task);
    printk("<6>[INFO]  [TEST M14] Spawned Ring 3 test task '%s' (PID %d). Executing full M14 test suite...\n",
           task->comm, task->pid);

    /* 8. Yield CPU until test process completes */
    while (task->state != TASK_ZOMBIE) {
        sched_yield();
    }

    if (task->exit_code != 0) {
        printk("<0>[TEST M14] FAILED: Test task exited with status %d (expected 0)!\n",
               task->exit_code);
        sched_remove_task(task);
        vmm_destroy_user_space(task->pml4);
        task_destroy(task);
        return false;
    }

    /* 9. Success: clean up task */
    sched_remove_task(task);
    vmm_destroy_user_space(task->pml4);
    task_destroy(task);

    printk("<6>[  0.000000] Milestone M14 status: [OK] DHCP, DNS, ifconfig, netstat, wget, pty verified.\n");
    return true;
}
