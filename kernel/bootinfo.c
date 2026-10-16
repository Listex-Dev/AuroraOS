#include <aurora/bootinfo.h>
#include <aurora/printk.h>
#include <aurora/string.h>

struct boot_info g_boot_info;

/* Higher half physical mapping offset */
#define KERNEL_DIRECT_MAP_BASE 0xFFFF800000000000ULL

static void *phys_to_virt(uint64_t phys)
{
    /* In early boot with identity + higher-half mapping, we can access via direct map */
    return (void *)(KERNEL_DIRECT_MAP_BASE + phys);
}

void bootinfo_init(uint64_t magic, uint64_t mbi_addr)
{
    memset(&g_boot_info, 0, sizeof(g_boot_info));
    g_boot_info.magic = (uint32_t)magic;
    g_boot_info.mbi_addr = mbi_addr;

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        /* Multiboot 1 */
        uint32_t *mbi = (uint32_t *)phys_to_virt(mbi_addr);
        uint32_t flags = mbi[0];

        if (flags & (1 << 0)) {
            g_boot_info.mem_lower_kb = mbi[1];
            g_boot_info.mem_upper_kb = mbi[2];
            g_boot_info.total_memory_bytes = (g_boot_info.mem_upper_kb + 1024) * 1024ULL;
        }

        if (flags & (1 << 2)) {
            uint32_t cmdline_phys = mbi[4];
            g_boot_info.cmdline = (const char *)phys_to_virt(cmdline_phys);
        }
    } else if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        /* Multiboot 2 tags */
        uint8_t *tag_ptr = (uint8_t *)phys_to_virt(mbi_addr + 8);
        while (1) {
            uint32_t type = *(uint32_t *)tag_ptr;
            uint32_t size = *(uint32_t *)(tag_ptr + 4);

            if (type == 0) {
                /* End tag */
                break;
            }

            if (type == 1) {
                /* Command line string */
                g_boot_info.cmdline = (const char *)(tag_ptr + 8);
            } else if (type == 4) {
                /* Basic memory information */
                g_boot_info.mem_lower_kb = *(uint32_t *)(tag_ptr + 8);
                g_boot_info.mem_upper_kb = *(uint32_t *)(tag_ptr + 12);
                g_boot_info.total_memory_bytes = (g_boot_info.mem_upper_kb + 1024) * 1024ULL;
            }

            /* Tags are 8-byte aligned */
            tag_ptr += (size + 7) & ~7;
        }
    }
}
