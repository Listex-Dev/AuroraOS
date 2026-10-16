#ifndef _AURORA_BOOTINFO_H
#define _AURORA_BOOTINFO_H

#include <aurora/types.h>

#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

/* Minimal unified boot info structure passed to kernel subsystems */
struct boot_info {
    uint32_t magic;
    uint64_t mbi_addr;
    const char *cmdline;
    uint64_t mem_lower_kb;
    uint64_t mem_upper_kb;
    uint64_t total_memory_bytes;
};

extern struct boot_info g_boot_info;

void bootinfo_init(uint64_t magic, uint64_t mbi_addr);

#endif /* _AURORA_BOOTINFO_H */
