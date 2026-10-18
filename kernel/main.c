#include <aurora/types.h>
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
#include <aurora/drivers/m9_test.h>
#include <aurora/proc/m10_test.h>
#include <aurora/proc/m11_test.h>

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

    /* --- Milestone M1: GDT, TSS, IDT, Exceptions --- */
    gdt_init();
    idt_init();
    exceptions_init();
    sse_init();

    /* Run Milestone M1 verification */
    test_page_fault_handling();

    /* --- Milestone M2: PMM, VMM, Direct Map, Heap --- */
    pmm_init();
    vmm_init();
    kmalloc_init();

    /* --- Milestone M6: VFS Subsystem Initialization --- */
    vfs_init();

    /* Run Milestone M2 verification */
    test_memory_subsystem();

    /* --- Milestone M3: PIC, PIT 100Hz, Scheduler --- */
    pic_init();
    pit_init(PIT_TARGET_HZ);
    sched_init();

    /* Enable hardware interrupts */
    sti();

    /* Run Milestone M3 verification */
    test_scheduler_multitasking();

    /* --- Milestone M4: Syscall Gate & User Mode --- */
    syscall_init();

    /* Run Milestone M4 verification */
    test_usermode_syscall();

    /* --- Milestone M5: fork, execve (ELF64), wait4, signals --- */
    test_fork_execve_wait();

    /* --- Milestone M6: VFS Subsystem verification --- */
    test_vfs_subsystem();

    /* --- Milestone M7: Virtual Filesystems verification (ramfs+cpio, procfs, devtmpfs, tmpfs, sysfs) --- */
    test_virtual_filesystems();

    /* --- Milestone M8: ext2/ext4 R/W, mkfs, fsck, pivot_root, fstab --- */
    test_ext4_root_subsystem();

    /* --- Milestone M9: TTY line discipline, VT0-5, kbd, termios, Ctrl-C --- */
    test_tty_subsystem();

    /* --- Milestone M10: libc full + crt0 + user utilities (ls, cat, cp, ps, vi, grep) --- */
    test_m10_libc_and_user_utils();

    /* --- Milestone M11: /bin/sh (pipelines, redirections, job control), init (PID 1), getty, login --- */
    test_m11_init_getty_login_shell();

    /* Halt CPU awaiting interrupts */
    while (1) {
        hlt();
    }
}
