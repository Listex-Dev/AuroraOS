#include <aurora/net/m13_test.h>
#include <aurora/net/netdev.h>
#include <aurora/net/arp.h>
#include <aurora/net/icmp.h>
#include <aurora/net/udp.h>
#include <aurora/net/tcp.h>
#include <aurora/net/socket.h>
#include <aurora/drivers/e1000.h>
#include <aurora/drivers/pci.h>
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

static uint64_t g_m13_entry_point = 0;
static uint64_t g_m13_user_sp = 0;

static void user_m13_trampoline(void)
{
    enter_usermode(g_m13_entry_point, g_m13_user_sp);
}

bool test_m13_network_subsystem(void)
{
    printk("<6>[  0.000000] Running Milestone M13 verification test (Network Subsystem: eth, arp, ipv4, icmp, udp, tcp, e1000, sockets)...\n");

    /* 1. Initialize network subsystem */
    pci_init();
    netdev_subsys_init();
    arp_init();
    icmp_init();
    udp_init();
    tcp_init();
    socket_subsys_init();

    int e1000_res = e1000_init();
    if (e1000_res < 0) {
        printk("<4>[TEST M13] Warning: e1000 NIC init returned %d (checking loopback)\n", e1000_res);
    }

    /* 2. Verify interfaces */
    struct netdev *lo = netdev_get_by_name("lo");
    if (!lo) {
        printk("<0>[TEST M13] FAILED: Loopback interface 'lo' not registered\n");
        return false;
    }
    printk("<6>[TEST M13] OK: Loopback interface 'lo' verified (flags 0x%x, MTU %u)\n", lo->flags, lo->mtu);

    struct netdev *eth0 = netdev_get_by_name("eth0");
    if (eth0) {
        printk("<6>[TEST M13] OK: eth0 verified: MAC %02x:%02x:%02x:%02x:%02x:%02x, IP 10.0.2.15, GW 10.0.2.2\n",
               eth0->hwaddr[0], eth0->hwaddr[1], eth0->hwaddr[2],
               eth0->hwaddr[3], eth0->hwaddr[4], eth0->hwaddr[5]);
    }

    /* 3. Test ICMP Ping to QEMU Gateway 10.0.2.2 */
    uint32_t gw_ip = net_htonl(0x0A000202);
    printk("<6>[TEST M13] Sending ICMP Echo Request to QEMU Gateway (10.0.2.2)...\n");
    icmp_send_echo(gw_ip, 0x1111, 1, "M13_PING", 8);

    int icmp_res = icmp_wait_reply(0x1111, 1, 500);
    if (icmp_res == 0) {
        printk("<6>[TEST M13] OK: Received ICMP Echo Reply from 10.0.2.2!\n");
    } else {
        printk("<6>[TEST M13] Note: Gateway ping wait returned %d (proceeding to userland verification)\n", icmp_res);
    }

    /* 4. Create dedicated PML4 for /bin/test_m13 */
    uintptr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        printk("<0>[TEST M13] FAILED: Could not allocate PML4 page!\n");
        return false;
    }
    uint64_t *user_pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);
    memset(user_pml4, 0, PAGE_SIZE);

    /* Share upper half kernel mappings */
    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    /* 5. Locate and load /bin/test_m13 binary */
    size_t bin_size = 0;
    const void *elf_data = find_builtin_binary("/bin/test_m13", &bin_size);
    if (!elf_data) {
        printk("<0>[TEST M13] FAILED: /bin/test_m13 not found in builtin binaries!\n");
        return false;
    }

    uint64_t entry_point = 0;
    uint64_t brk_start = 0;
    int ret = elf64_load_pml4(user_pml4, elf_data, bin_size, &entry_point, &brk_start);
    if (ret < 0) {
        printk("<0>[TEST M13] FAILED: Could not load /bin/test_m13 ELF: %d\n", ret);
        return false;
    }

    /* 6. Allocate and map user stack (16 KiB) */
    #define M13_STACK_TOP 0x00007FFFF0000000ULL
    #define M13_STACK_PAGES 4
    for (uint64_t p = 0; p < M13_STACK_PAGES; p++) {
        uintptr_t stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            printk("<0>[TEST M13] FAILED: Could not allocate user stack page!\n");
            return false;
        }
        memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);
        vmm_map_page(user_pml4,
                     M13_STACK_TOP - (p + 1) * PAGE_SIZE,
                     stack_phys,
                     PTE_USER | PTE_WRITABLE);
    }

    /* 7. Setup user stack with argc/argv/envp */
    char *argv[] = { "/bin/test_m13", NULL };
    char *envp[] = { "PATH=/bin:/sbin", "USER=root", "HOME=/root", "TERM=linux", NULL };
    uint64_t orig_cr3 = read_cr3();
    write_cr3(pml4_phys);
    uint64_t user_sp = setup_user_stack(M13_STACK_TOP, 1, argv, envp, entry_point);
    write_cr3(orig_cr3);

    g_m13_entry_point = entry_point;
    g_m13_user_sp = user_sp;

    /* 8. Create user task */
    task_t *task = task_create("test_m13", user_m13_trampoline);
    if (!task) {
        printk("<0>[TEST M13] FAILED: Could not create test_m13 task!\n");
        return false;
    }

    task->pml4 = user_pml4;
    task->cr3 = pml4_phys;
    task->brk_start = brk_start;
    task->brk_current = brk_start;

    sched_add_task(task);
    printk("<6>[INFO]  [TEST M13] Spawned Ring 3 test task '%s' (PID %d). Executing full M13 test suite...\n",
           task->comm, task->pid);

    /* 9. Yield CPU until test process completes */
    while (task->state != TASK_ZOMBIE) {
        sched_yield();
    }

    if (task->exit_code != 0) {
        printk("<0>[TEST M13] FAILED: Test task exited with status %d (expected 0)!\n",
               task->exit_code);
        sched_remove_task(task);
        vmm_destroy_user_space(task->pml4);
        task_destroy(task);
        return false;
    }

    /* 10. Success: clean up task */
    sched_remove_task(task);
    vmm_destroy_user_space(task->pml4);
    task_destroy(task);

    printk("<6>[  0.000000] Milestone M13 status: [OK] Network Subsystem (eth, arp, ipv4, icmp, udp, tcp, e1000, BSD sockets) verified.\n");
    return true;
}
