#include <aurora/types.h>
#include <aurora/string.h>
#include <aurora/serial.h>
#include <aurora/printk.h>
#include <aurora/panic.h>
#include <aurora/bootinfo.h>
#include <aurora/io.h>
#include <aurora/arch/x86_64/gdt.h>
#include <aurora/arch/x86_64/idt.h>
#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/arch/x86_64/cr.h>
#include <aurora/arch/x86_64/pic.h>
#include <aurora/arch/x86_64/pit.h>
#include <aurora/arch/x86_64/m1_test.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/mm/m2_test.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/proc/m3_test.h>
#include <aurora/syscall.h>
#include <aurora/proc/m4_test.h>
#include <aurora/proc/m5_test.h>
#include <aurora/fs/vfs.h>
#include <aurora/fs/m6_test.h>
#include <aurora/fs/m7_test.h>
#include <aurora/fs/m8_test.h>
#include <aurora/proc/binaries.h>
#include <aurora/proc/stack.h>
#include <aurora/elf64.h>
#include <aurora/drivers/console.h>
#include <aurora/drivers/tty.h>
#include <aurora/drivers/kbd.h>
#include <aurora/drivers/pci.h>
#include <aurora/drivers/ahci.h>
#include <aurora/drivers/ata.h>
#include <aurora/drivers/block.h>
#include <aurora/drivers/m9_test.h>
#include <aurora/proc/m10_test.h>
#include <aurora/proc/m11_test.h>
#include <aurora/drivers/m12_test.h>
#include <aurora/net/m13_test.h>
#include <aurora/net/m14_test.h>
#include <aurora/proc/m15_test.h>
#include <aurora/proc/m16_test.h>
#include <aurora/proc/m17_test.h>
#include <aurora/fs/docs.h>
#include <aurora/net/dhcp.h>
#include <aurora/net/netdev.h>
#include <aurora/net/arp.h>
#include <aurora/net/icmp.h>
#include <aurora/net/udp.h>
#include <aurora/net/tcp.h>
#include <aurora/net/socket.h>
#include <aurora/drivers/e1000.h>

extern void enter_usermode(uint64_t rip, uint64_t rsp);

static uint64_t g_init_entry = 0;
static uint64_t g_init_sp = 0;

static void init_trampoline(void)
{
    enter_usermode(g_init_entry, g_init_sp);
}

void security_subsys_init(void)
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    sys_mkdir("/etc", 0755);
    sys_mkdir("/bin", 0755);
    sys_mkdir("/sbin", 0755);
    sys_mkdir("/tmp", 0777);
    sys_mkdir("/run", 0755);
    sys_mkdir("/root", 0700);
    sys_mkdir("/home", 0755);
    sys_mkdir("/home/aurora", 0755);
    sys_mkdir("/var", 0755);
    sys_mkdir("/var/log", 0755);

    int fd = sys_open("/etc/issue", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *issue = "\n\033[1;36mAuroraOS 1.0.0 (x86_64)\033[0m\n\n";
        sys_write(fd, issue, strlen(issue));
        sys_close(fd);
    }

    fd = sys_open("/etc/motd", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *motd = "\n\033[1;32mWelcome to AuroraOS 1.0.0!\033[0m\n"
                           "All systems operating in 64-bit Long Mode.\n"
                           "Available commands: ls, ps, cat, cp, vi, grep, fdisk, mount, sh.\n\n";
        sys_write(fd, motd, strlen(motd));
        sys_close(fd);
    }

    fd = sys_open("/etc/passwd", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *pw = "root:x:0:0:root:/root:/bin/sh\n"
                         "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin\n"
                         "aurora:x:1000:1000:Aurora User:/home/aurora:/bin/sh\n";
        sys_write(fd, pw, strlen(pw));
        sys_close(fd);
    }

    fd = sys_open("/etc/shadow", O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fd >= 0) {
        const char *sh = "root:$6$saltsalt$FCtXpTmG7SvkmMFnnZIotZMsxPcyMf9rYNlYFfsA2Fau3J9awLnj6tPMJW6qxQdpHAgjs0cvMWQ2Ve2HWIdX2/:19000:0:99999:7:::\n"
                         "aurora:$6$saltsalt$VcYMCBfELnOcDK8outogXZyOp5rTpzY5E2LZQKa6N.QAElzmqtqwgRygFvpH/3Mcts8sy/gn7EtOsH5Rw/Xw4/:19000:0:99999:7:::\n";
        sys_write(fd, sh, strlen(sh));
        sys_close(fd);
    }

    fd = sys_open("/etc/group", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *grp = "root:x:0:root\n"
                          "wheel:x:10:root\n"
                          "shadow:x:42:root\n"
                          "tty:x:5:\n"
                          "disk:x:6:\n"
                          "users:x:1000:aurora\n";
        sys_write(fd, grp, strlen(grp));
        sys_close(fd);
    }

    fd = sys_open("/var/log/auth.log", O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fd >= 0) {
        const char *auth_init = "System security and audit logging initialized.\n";
        sys_write(fd, auth_init, strlen(auth_init));
        sys_close(fd);
    }
}

void services_subsys_init(void)
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    security_subsys_init();

    sys_mkdir("/etc", 0755);
    sys_mkdir("/etc/init.d", 0755);
    sys_mkdir("/run", 0755);
    sys_mkdir("/var", 0755);
    sys_mkdir("/var/log", 0755);
    sys_mkdir("/var/spool", 0755);
    sys_mkdir("/var/spool/cron", 0755);
    sys_mkdir("/var/spool/cron/crontabs", 0755);

    /* /etc/rc.conf */
    int fd = sys_open("/etc/rc.conf", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *rc = "# /etc/rc.conf - AuroraOS system service configuration\n"
                         "services=\"networking syslogd crond sshd\"\n";
        sys_write(fd, rc, strlen(rc));
        sys_close(fd);
    }

    /* /etc/crontab */
    fd = sys_open("/etc/crontab", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *cr = "# /etc/crontab - system crontab\n"
                         "* * * * * root /bin/echo \"cron task running\" >> /var/log/cron\n";
        sys_write(fd, cr, strlen(cr));
        sys_close(fd);
    }

    /* /etc/init.d/syslogd */
    fd = sys_open("/etc/init.d/syslogd", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd >= 0) {
        const char *s = "#!/bin/sh\n"
                        "[ \"$1\" = \"status\" ] && [ -f /run/syslogd.pid ] && echo \"syslogd is running\" && exit 0\n"
                        "[ \"$1\" = \"status\" ] && echo \"syslogd is not running\" && exit 3\n"
                        "[ \"$1\" = \"start\" ] && [ -f /run/syslogd.pid ] && echo \"syslogd already running\" && exit 0\n"
                        "[ \"$1\" = \"start\" ] && /sbin/syslogd && exit 0\n"
                        "[ \"$1\" = \"stop\" ] && killall syslogd 2>/dev/null; rm -f /run/syslogd.pid\n"
                        "[ \"$1\" = \"stop\" ] && exit 0\n"
                        "[ \"$1\" = \"restart\" ] && killall syslogd 2>/dev/null; rm -f /run/syslogd.pid\n"
                        "[ \"$1\" = \"restart\" ] && /sbin/syslogd && exit 0\n"
                        "echo \"Usage: $0 start, stop, restart, status\"\n"
                        "exit 1\n";
        sys_write(fd, s, strlen(s));
        sys_close(fd);
    }

    /* /etc/init.d/crond */
    fd = sys_open("/etc/init.d/crond", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd >= 0) {
        const char *s = "#!/bin/sh\n"
                        "[ \"$1\" = \"status\" ] && [ -f /run/crond.pid ] && echo \"crond is running\" && exit 0\n"
                        "[ \"$1\" = \"status\" ] && echo \"crond is not running\" && exit 3\n"
                        "[ \"$1\" = \"start\" ] && [ -f /run/crond.pid ] && echo \"crond already running\" && exit 0\n"
                        "[ \"$1\" = \"start\" ] && /sbin/crond && exit 0\n"
                        "[ \"$1\" = \"stop\" ] && killall crond 2>/dev/null; rm -f /run/crond.pid\n"
                        "[ \"$1\" = \"stop\" ] && exit 0\n"
                        "[ \"$1\" = \"restart\" ] && killall crond 2>/dev/null; rm -f /run/crond.pid\n"
                        "[ \"$1\" = \"restart\" ] && /sbin/crond && exit 0\n"
                        "echo \"Usage: $0 start, stop, restart, status\"\n"
                        "exit 1\n";
        sys_write(fd, s, strlen(s));
        sys_close(fd);
    }

    /* /etc/init.d/sshd */
    fd = sys_open("/etc/init.d/sshd", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd >= 0) {
        const char *s = "#!/bin/sh\n"
                        "[ \"$1\" = \"status\" ] && [ -f /run/sshd.pid ] && echo \"sshd is running\" && exit 0\n"
                        "[ \"$1\" = \"status\" ] && echo \"sshd is not running\" && exit 3\n"
                        "[ \"$1\" = \"start\" ] && [ -f /run/sshd.pid ] && echo \"sshd already running\" && exit 0\n"
                        "[ \"$1\" = \"start\" ] && /sbin/sshd && exit 0\n"
                        "[ \"$1\" = \"stop\" ] && killall sshd 2>/dev/null; rm -f /run/sshd.pid\n"
                        "[ \"$1\" = \"stop\" ] && exit 0\n"
                        "[ \"$1\" = \"restart\" ] && killall sshd 2>/dev/null; rm -f /run/sshd.pid\n"
                        "[ \"$1\" = \"restart\" ] && /sbin/sshd && exit 0\n"
                        "echo \"Usage: $0 start, stop, restart, status\"\n"
                        "exit 1\n";
        sys_write(fd, s, strlen(s));
        sys_close(fd);
    }

    /* /etc/init.d/networking */
    fd = sys_open("/etc/init.d/networking", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd >= 0) {
        const char *s = "#!/bin/sh\n"
                        "[ \"$1\" = \"start\" ] && /bin/ifconfig lo 127.0.0.1 up && /sbin/ifconfig eth0 10.0.2.15 up 2>/dev/null && echo \"networking: configured\" >> /var/log/messages && exit 0\n"
                        "[ \"$1\" = \"stop\" ] && /sbin/ifconfig eth0 down 2>/dev/null && echo \"networking: down\" >> /var/log/messages && exit 0\n"
                        "[ \"$1\" = \"restart\" ] && /sbin/ifconfig eth0 down 2>/dev/null && /bin/ifconfig lo 127.0.0.1 up && /sbin/ifconfig eth0 10.0.2.15 up 2>/dev/null && exit 0\n"
                        "[ \"$1\" = \"status\" ] && /bin/ifconfig lo >/dev/null && exit 0\n"
                        "[ \"$1\" = \"status\" ] && exit 3\n"
                        "echo \"Usage: $0 start, stop, restart, status\"\n"
                        "exit 1\n";
        sys_write(fd, s, strlen(s));
        sys_close(fd);
    }
}

static void launch_init_process(void)
{
    services_subsys_init();
    docs_and_pkg_subsys_init();

    /* Allocate address space for PID 1 */
    uintptr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        panic("launch_init_process: Out of memory for init PML4");
    }
    uint64_t *user_pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);
    memset(user_pml4, 0, PAGE_SIZE);

    /* Share upper-half kernel mappings */
    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    /* Locate init binary (fallback to /bin/sh) */
    size_t bin_size = 0;
    const void *elf_data = find_builtin_binary("/sbin/init", &bin_size);
    if (!elf_data) {
        elf_data = find_builtin_binary("/bin/init", &bin_size);
    }
    if (!elf_data) {
        elf_data = find_builtin_binary("/bin/sh", &bin_size);
    }
    if (!elf_data) {
        panic("launch_init_process: No init or shell binary found");
    }

    /* Load ELF binary */
    uint64_t entry_point = 0;
    uint64_t brk_start = 0;
    int ret = elf64_load_pml4(user_pml4, elf_data, bin_size, &entry_point, &brk_start);
    if (ret < 0) {
        panic("launch_init_process: Failed to load ELF binary");
    }

    /* Allocate and map 16 KiB user stack */
    #define USER_STACK_TOP 0x00007FFFF0000000ULL
    #define USER_STACK_PAGES 4
    for (uint64_t p = 0; p < USER_STACK_PAGES; p++) {
        uintptr_t stack_phys = pmm_alloc_page();
        if (!stack_phys) panic("launch_init_process: Out of memory for stack");
        memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);
        vmm_map_page(user_pml4,
                     USER_STACK_TOP - (p + 1) * PAGE_SIZE,
                     stack_phys,
                     PTE_USER | PTE_WRITABLE | PTE_NX);
    }

    /* Setup user stack with argc/argv/envp */
    char *argv[] = { "/sbin/init", NULL };
    char *envp[] = { "PATH=/bin:/sbin:/usr/bin", "USER=root", "HOME=/root", "TERM=linux", NULL };
    uint64_t orig_cr3 = read_cr3();
    write_cr3(pml4_phys);
    uint64_t user_sp = setup_user_stack(USER_STACK_TOP, 1, argv, envp, entry_point);
    write_cr3(orig_cr3);

    g_init_entry = entry_point;
    g_init_sp = user_sp;

    /* Create task for PID 1 */
    task_t *task = task_create("init", init_trampoline);
    if (!task) panic("launch_init_process: Failed to create init task");
    task->pml4 = user_pml4;
    task->cr3 = pml4_phys;
    task->brk_start = brk_start;
    task->brk_current = brk_start;

    sched_add_task(task);
    printk("\n[  0.000000] AuroraOS: Started /sbin/init (PID 1). Entering interactive mode.\n\n");
}

void kmain(uint64_t magic, uint64_t mbi_addr)
{
    /* Initialize Serial Driver (COM1 at 0x3F8, 115200 8N1) */
    serial_init();

    /* M0 requirement: exact message printed to serial */
    serial_puts("AuroraOS 1.0.0 booting...\n");

    /* Formatted printk tests and boot diagnostics */
    printk("<6>[  0.000000] Linux-like Monolithic Kernel for x86-64 (Long Mode)\n");
    printk("<6>[  0.000000] CPU in 64-bit Long Mode, Paging Enabled (Higher Half)\n");

    bootinfo_init(magic, mbi_addr);

    if (g_boot_info.magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        printk("<6>[  0.000000] Bootloader: Multiboot 1 (magic 0x%x)\n", g_boot_info.magic);
    } else if (g_boot_info.magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        printk("<6>[  0.000000] Bootloader: Multiboot 2 (magic 0x%x)\n", g_boot_info.magic);
    } else {
        printk("<4>[  0.000000] Unknown bootloader magic: 0x%x\n", g_boot_info.magic);
    }

    if (g_boot_info.total_memory_bytes > 0) {
        uint64_t mem_mb = g_boot_info.total_memory_bytes / (1024 * 1024);
        printk("<6>[  0.000000] Memory detected: %llu MB (lower %u KB, upper %u KB)\n",
               mem_mb, (unsigned int)g_boot_info.mem_lower_kb, (unsigned int)g_boot_info.mem_upper_kb);
    }

    if (g_boot_info.cmdline) {
        printk("<6>[  0.000000] Kernel cmdline: %s\n", g_boot_info.cmdline);
    }

    printk("<6>[  0.000000] Drivers: serial COM1 initialized at 0x3F8\n");

    /* Core Subsystem Initialization */
    gdt_init();
    idt_init();
    exceptions_init();
    sse_init();

    pmm_init();
    vmm_init();
    kmalloc_init();

    vfs_init();

    pic_init();
    pit_init(PIT_TARGET_HZ);
    sched_init();

    sti();

    syscall_init();

    bool is_test = false;
    if (g_boot_info.cmdline) {
        for (const char *p = g_boot_info.cmdline; *p; p++) {
            if (p[0] == 't' && p[1] == 'e' && p[2] == 's' && p[3] == 't') {
                is_test = true;
                break;
            }
        }
    }

    if (is_test) {
        /* Run Milestone Verification Suites (M1 through M12) */
        test_page_fault_handling();
        test_memory_subsystem();
        test_scheduler_multitasking();
        test_usermode_syscall();
        test_fork_execve_wait();
        test_vfs_subsystem();
        test_virtual_filesystems();
        test_ext4_root_subsystem();
        test_tty_subsystem();
        test_m10_libc_and_user_utils();
        test_m11_init_getty_login_shell();
        test_m12_block_and_pci_subsystem();
        test_m13_network_subsystem();
        test_m14_services_and_pty_subsystem();
        test_m15_security_subsystem();
        test_m16_services_subsystem();
        test_m17_package_and_docs_subsystem();
    } else {
        /* Interactive Production Boot: initialize drivers and start PID 1 */
        pci_init();
        block_subsys_init();
        ahci_init();
        ata_init();
        netdev_subsys_init();
        arp_init();
        icmp_init();
        udp_init();
        tcp_init();
        socket_subsys_init();
        e1000_init();
        dhcp_configure_interface(NULL, NULL);
        tty_subsys_init();
        kbd_init();

        launch_init_process();
    }

    /* Halt CPU awaiting interrupts */
    while (1) {
        hlt();
    }
}
