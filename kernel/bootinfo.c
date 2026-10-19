#include <aurora/bootinfo.h>
#include <aurora/printk.h>
#include <aurora/string.h>

struct boot_info g_boot_info;

/* Higher half physical mapping offset */
#define KERNEL_DIRECT_MAP_BASE 0xFFFF800000000000ULL

static char s_boot_cmdline[256];

static void *phys_to_virt(uint64_t phys)
{
    return (void *)(KERNEL_DIRECT_MAP_BASE + phys);
}

struct mb1_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

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
            const char *src = (const char *)phys_to_virt(cmdline_phys);
            strncpy(s_boot_cmdline, src, sizeof(s_boot_cmdline) - 1);
            s_boot_cmdline[sizeof(s_boot_cmdline) - 1] = '\0';
            g_boot_info.cmdline = s_boot_cmdline;
        }

        if (flags & (1 << 3)) {
            uint32_t mods_count = mbi[5];
            uint32_t mods_addr = mbi[6];
            if (mods_count > 0 && mods_addr != 0) {
                uint32_t *mod = (uint32_t *)phys_to_virt(mods_addr);
                g_boot_info.initrd_start = mod[0];
                g_boot_info.initrd_end = mod[1];
            }
        }

        /* Check for memory map (bit 6) */
        if (flags & (1 << 6)) {
            uint32_t mmap_len = mbi[11];
            uint32_t mmap_addr = mbi[12];
            uint8_t *curr = (uint8_t *)phys_to_virt(mmap_addr);
            uint8_t *end = curr + mmap_len;

            while (curr < end && g_boot_info.mmap_count < BOOTINFO_MMAP_MAX_ENTRIES) {
                struct mb1_mmap_entry *entry = (struct mb1_mmap_entry *)curr;
                g_boot_info.mmap[g_boot_info.mmap_count].base = entry->addr;
                g_boot_info.mmap[g_boot_info.mmap_count].length = entry->len;
                g_boot_info.mmap[g_boot_info.mmap_count].type = entry->type;
                g_boot_info.mmap_count++;
                curr += entry->size + sizeof(entry->size);
            }
        }
    } else if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        /* Multiboot 2 tags */
        uint8_t *tag_ptr = (uint8_t *)phys_to_virt(mbi_addr + 8);
        while (1) {
            uint32_t type = *(uint32_t *)tag_ptr;
            uint32_t size = *(uint32_t *)(tag_ptr + 4);

            if (type == 0) {
                break;
            }

            if (type == 1) {
                /* Command line */
                const char *src = (const char *)(tag_ptr + 8);
                strncpy(s_boot_cmdline, src, sizeof(s_boot_cmdline) - 1);
                s_boot_cmdline[sizeof(s_boot_cmdline) - 1] = '\0';
                g_boot_info.cmdline = s_boot_cmdline;
            } else if (type == 3) {
                /* Multiboot 2 Module */
                g_boot_info.initrd_start = *(uint32_t *)(tag_ptr + 8);
                g_boot_info.initrd_end = *(uint32_t *)(tag_ptr + 12);
            } else if (type == 4) {
                /* Basic meminfo */
                g_boot_info.mem_lower_kb = *(uint32_t *)(tag_ptr + 8);
                g_boot_info.mem_upper_kb = *(uint32_t *)(tag_ptr + 12);
                g_boot_info.total_memory_bytes = (g_boot_info.mem_upper_kb + 1024) * 1024ULL;
            } else if (type == 6) {
                /* Multiboot 2 Memory Map */
                uint32_t entry_size = *(uint32_t *)(tag_ptr + 8);
                uint8_t *entry_curr = tag_ptr + 16;
                uint8_t *entry_end = tag_ptr + size;

                while (entry_curr < entry_end && g_boot_info.mmap_count < BOOTINFO_MMAP_MAX_ENTRIES) {
                    uint64_t base = *(uint64_t *)(entry_curr);
                    uint64_t len = *(uint64_t *)(entry_curr + 8);
                    uint32_t mtype = *(uint32_t *)(entry_curr + 16);

                    g_boot_info.mmap[g_boot_info.mmap_count].base = base;
                    g_boot_info.mmap[g_boot_info.mmap_count].length = len;
                    g_boot_info.mmap[g_boot_info.mmap_count].type = mtype;
                    g_boot_info.mmap_count++;

                    entry_curr += entry_size;
                }
            }

            tag_ptr += (size + 7) & ~7;
        }
    }

    /* Fallback if bootloader did not supply mmap tag */
    if (g_boot_info.mmap_count == 0 && g_boot_info.total_memory_bytes > 0) {
        g_boot_info.mmap[0].base = 0x100000;
        g_boot_info.mmap[0].length = g_boot_info.total_memory_bytes - 0x100000;
        g_boot_info.mmap[0].type = BOOT_MEM_AVAILABLE;
        g_boot_info.mmap_count = 1;
    }
}
