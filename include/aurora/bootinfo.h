#ifndef _AURORA_BOOTINFO_H
#define _AURORA_BOOTINFO_H

#include <aurora/types.h>

#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

#define BOOTINFO_MMAP_MAX_ENTRIES 32

#define BOOT_MEM_AVAILABLE 1
#define BOOT_MEM_RESERVED  2
#define BOOT_MEM_ACPI      3
#define BOOT_MEM_NVS       4
#define BOOT_MEM_BADRAM    5

struct boot_mmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

/* Unified boot info structure passed to kernel subsystems */
struct boot_info {
    uint32_t magic;
    uint64_t mbi_addr;
    const char *cmdline;
    uint64_t mem_lower_kb;
    uint64_t mem_upper_kb;
    uint64_t total_memory_bytes;
    uint32_t mmap_count;
    struct boot_mmap_entry mmap[BOOTINFO_MMAP_MAX_ENTRIES];
};

extern struct boot_info g_boot_info;

void bootinfo_init(uint64_t magic, uint64_t mbi_addr);

#endif /* _AURORA_BOOTINFO_H */
