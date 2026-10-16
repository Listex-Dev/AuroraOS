#include <aurora/types.h>
#include <aurora/serial.h>
#include <aurora/printk.h>
#include <aurora/panic.h>
#include <aurora/bootinfo.h>
#include <aurora/io.h>
#include <aurora/arch/x86_64/gdt.h>
#include <aurora/arch/x86_64/idt.h>
#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/arch/x86_64/m1_test.h>

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

    /* Run Milestone M1 automated verification */
    test_page_fault_handling();

    /* Halt CPU awaiting interrupts in future milestones */
    while (1) {
        hlt();
    }
}
